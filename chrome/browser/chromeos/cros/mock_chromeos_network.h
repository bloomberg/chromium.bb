// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_MOCK_CHROMEOS_NETWORK_H_
#define CHROME_BROWSER_CHROMEOS_CROS_MOCK_CHROMEOS_NETWORK_H_

#include "third_party/cros/chromeos_network.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockChromeOSNetwork {
 public:
  // Initializes the global instance and replaces the chromeos_network
  // functions with mocks.
  static void Initialize();

  // Destroyes the global instance and resets the chromeos_network function.
  static void Shutdown();

  // Gets the global instance.
  static MockChromeOSNetwork* Get();

  MOCK_METHOD3(SetNetworkServicePropertyGValue,
               void(const char* service_path,
                    const char* property,
                    const GValue* gvalue));
  MOCK_METHOD2(ClearNetworkServiceProperty,
               void(const char* service_path, const char* property));
  MOCK_METHOD3(SetNetworkDevicePropertyGValue,
               void(const char* device_path,
                    const char* property,
                    const GValue* gvalue));
  MOCK_METHOD3(SetNetworkIPConfigPropertyGValue,
               void(const char* ipconfig_path,
                    const char* property,
                    const GValue* gvalue));
  MOCK_METHOD2(SetNetworkManagerPropertyGValue,
               void(const char* property,
                    const GValue* gvalue));
  MOCK_METHOD1(RequestCellularDataPlanUpdate,
               void(const char* modem_service_path));
  MOCK_METHOD2(MonitorNetworkManagerProperties,
               NetworkPropertiesMonitor(MonitorPropertyGValueCallback callback,
                                        void* object));
  MOCK_METHOD3(MonitorNetworkServiceProperties,
               NetworkPropertiesMonitor(MonitorPropertyGValueCallback callback,
                                        const char* service_path,
                                        void* object));
  MOCK_METHOD3(MonitorNetworkDeviceProperties,
               NetworkPropertiesMonitor(MonitorPropertyGValueCallback callback,
                                        const char* device_path,
                                        void* object));
  MOCK_METHOD1(DisconnectNetworkPropertiesMonitor,
               void(NetworkPropertiesMonitor monitor));
  MOCK_METHOD2(MonitorCellularDataPlan,
               DataPlanUpdateMonitor(MonitorDataPlanCallback callback,
                                     void* object));
  MOCK_METHOD1(DisconnectDataPlanUpdateMonitor,
               void(DataPlanUpdateMonitor monitor));
  MOCK_METHOD3(MonitorSMS, SMSMonitor(const char* modem_device_path,
                                      MonitorSMSCallback callback,
                                      void* object));
  MOCK_METHOD1(DisconnectSMSMonitor, void(SMSMonitor monitor));
  MOCK_METHOD2(RequestNetworkManagerProperties,
               void(NetworkPropertiesGValueCallback callback, void* object));
  MOCK_METHOD3(RequestNetworkServiceProperties,
               void(const char* service_path,
                    NetworkPropertiesGValueCallback callback,
                    void* object));
  MOCK_METHOD3(RequestNetworkDeviceProperties,
               void(const char* device_path,
                    NetworkPropertiesGValueCallback callback,
                    void* object));
  MOCK_METHOD3(RequestNetworkProfileProperties,
               void(const char* profile_path,
                    NetworkPropertiesGValueCallback callback,
                    void* object));
  MOCK_METHOD4(RequestNetworkProfileEntryProperties,
               void(const char* profile_path,
                    const char* profile_entry_path,
                    NetworkPropertiesGValueCallback callback,
                    void* object));
  MOCK_METHOD4(RequestHiddenWifiNetworkProperties,
               void(const char* ssid,
                    const char* security,
                    NetworkPropertiesGValueCallback callback,
                    void* object));
  MOCK_METHOD5(RequestVirtualNetworkProperties,
               void(const char* service_name,
                    const char* server_hostname,
                    const char* provider_type,
                    NetworkPropertiesGValueCallback callback,
                    void* object));
  MOCK_METHOD1(RequestNetworkServiceDisconnect, void(const char* service_path));
  MOCK_METHOD1(RequestRemoveNetworkService, void(const char* service_path));
  MOCK_METHOD1(RequestNetworkScan, void(const char* network_type));
  MOCK_METHOD2(RequestNetworkDeviceEnable, void(const char* network_type,
                                                bool enable));
  MOCK_METHOD2(AddIPConfig, bool(const char* device_path, IPConfigType type));
  MOCK_METHOD1(RemoveIPConfig, bool(IPConfig* config));
  MOCK_METHOD0(GetDeviceNetworkList, DeviceNetworkList*());
  MOCK_METHOD1(FreeDeviceNetworkList, void(DeviceNetworkList* network_list));
  MOCK_METHOD4(ConfigureService,
               void(const char* identifier,
                    const GHashTable* properties,
                    NetworkActionCallback callback,
                    void* object));

 protected:
  MockChromeOSNetwork();
  virtual ~MockChromeOSNetwork();
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_MOCK_CHROMEOS_NETWORK_H_
