/*
   __   .__                ____                                                          
 _/  |_ |  |__    ____    / ___\ _______   ____   ____    ____    ____    ____    ____   
 \   __\|  |  \ _/ __ \  / /_/  >\_  __ \_/ __ \_/ __ \  /    \  /  _ \  /    \ _/ __ \  
  |  |  |   Y  \\  ___/  \___  /  |  | \/\  ___/\  ___/ |   |  \(  <_> )|   |  \\  ___/  
  |__|  |___|  / \___  >/_____/   |__|    \___  >\___  >|___|  / \____/ |___|  / \___  > 
             \/      \/                       \/     \/      \/              \/      \/  
                                                                                         
          .__ .__           .__ .__                                                      
 __  _  __|__||  |    ____  |__||  |                                                     
 \ \/ \/ /|  ||  |   /    \ |  ||  |                                                     
  \     / |  ||  |__|   |  \|  ||  |__                                                   
   \/\_/  |__||____/|___|  /|__||____/                                                   
                         \/                             
*/

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_log.h"

#include "bluetooth.h"
#include "udp.h"
#include "mesh.h"
#include "network.h"
#include "config.h"

const uint8_t custom_mac[] = {
    0xCA, 0xFE, 0x69, 0xC5, 0x11, CONFIG_DEVICE_NUM
};
const uint8_t tag_mac_template[] = {
    0xBE, 0xEF, 0x34, 0x25, 0x69, 0x00
};
char wifi_mac_str[18] = {};
bool wifi_ready = false;
bool sntp_ready = false;

void v_heartbeat(void * pvParameters)
{
    for(;;)
    {
        if (wifi_ready && sntp_ready)
        {
            time_t now;
            time(&now);
            //size_t packet_len = strlen("heartbeat") + sizeof(now) + strlen(wifi_mac_str) + 4; // Fuck it, just make the packet length 100.
            char data_str[100]; // 17 is the mac len, 4 is the separators, // 100]; //TODO: Calculate length lmao
            format_heartbeat(data_str, 100, now, wifi_mac_str);
            ESP_LOGI(WIFI_TAG, "%s", data_str);
            udp_send_str(data_str, MAX_SAFE_UDP_BLOCK_SIZE);
        }
        vTaskDelay((CONFIG_HEARTBEAT_INTERVAL * 1000) / portTICK_PERIOD_MS); 
    }
}

void app_main(void)
{
    /* Initialize NVS — it is used to store PHY calibration data */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );
        
    if ((ret = esp_base_mac_addr_set(custom_mac))  != ESP_OK)
    {
        ESP_LOGE(BLE_TAG, "%s Could not set mac address %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    if ((ret = esp_bt_controller_init(&bt_cfg)) != ESP_OK) {
        ESP_LOGE(BT_TAG, "%s initialize controller failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_bt_controller_enable(ESP_BT_MODE_BLE)) != ESP_OK) 
    {
        ESP_LOGE(BLE_TAG, "%s enable controller failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_bluedroid_init()) != ESP_OK) 
    {
        ESP_LOGE(BT_TAG, "%s initialize bluedroid failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_bluedroid_enable()) != ESP_OK) 
    {
        ESP_LOGE(BT_TAG, "%s enable bluedroid failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    start_mesh();
    init_udp_socket(CONFIG_UDP_DEST_ADDRESS, CONFIG_UDP_DEST_PORT);
    sntp_update_time(); // Wait. How the fuck is this functioning?

    if (!socket_ready())
	    ESP_LOGE(WIFI_TAG, "Could not start UDP socket");
    
    get_wifi_mac_str();
    ESP_LOGI(WIFI_TAG,"ESP MAC Address: %s",  wifi_mac_str);


    static uint8_t ucParameterToPass;
    TaskHandle_t xHandle = NULL;

    // Create the task, storing the handle.  Note that the passed parameter ucParameterToPass
    // must exist for the lifetime of the task, so in this case is declared static.  If it was just an
    // an automatic stack variable it might no longer exist, or at least have been corrupted, by the time
    // the new task attempts to access it.
    xTaskCreate( v_heartbeat, "HEARTBEAT", 2048, &ucParameterToPass, tskIDLE_PRIORITY, &xHandle );
    configASSERT( xHandle );

    // Use the handle to delete the task.
    //if( xHandle != NULL )
    //{
    //   vTaskDelete( xHandle );
    //}

    ble_app_gap_start_up(tag_mac_template);
    ESP_LOGI("CSHacked", "Done!");
}

