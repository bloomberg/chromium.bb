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
  // Initializes the global instance and sets the replaces chromeos_network
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
