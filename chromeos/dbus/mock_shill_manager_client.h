// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_MOCK_SHILL_MANAGER_CLIENT_H_
#define CHROMEOS_DBUS_MOCK_SHILL_MANAGER_CLIENT_H_

#include <string>

#include "base/values.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/dbus/shill_property_changed_observer.h"
#include "net/base/ip_endpoint.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockShillManagerClient : public ShillManagerClient {
 public:
  MockShillManagerClient();
  virtual ~MockShillManagerClient();

  MOCK_METHOD1(Init, void(dbus::Bus* bus));
  MOCK_METHOD1(AddPropertyChangedObserver,
               void(ShillPropertyChangedObserver* observer));
  MOCK_METHOD1(RemovePropertyChangedObserver,
               void(ShillPropertyChangedObserver* observer));
  MOCK_METHOD1(GetProperties, void(const DictionaryValueCallback& callback));
  MOCK_METHOD1(GetNetworksForGeolocation,
               void(const DictionaryValueCallback& callback));
  MOCK_METHOD4(SetProperty, void(const std::string& name,
                                 const base::Value& value,
                                 const base::Closure& callback,
                                 const ErrorCallback& error_callback));
  MOCK_METHOD3(RequestScan, void(const std::string& type,
                                 const base::Closure& callback,
                                 const ErrorCallback& error_callback));
  MOCK_METHOD3(EnableTechnology, void(const std::string& type,
                                      const base::Closure& callback,
                                      const ErrorCallback& error_callback));
  MOCK_METHOD3(DisableTechnology, void(const std::string& type,
                                       const base::Closure& callback,
                                       const ErrorCallback& error_callback));
  MOCK_METHOD3(ConfigureService, void(const base::DictionaryValue& properties,
                                      const ObjectPathCallback& callback,
                                      const ErrorCallback& error_callback));
  MOCK_METHOD4(ConfigureServiceForProfile,
               void(const dbus::ObjectPath& profile_path,
                    const base::DictionaryValue& properties,
                    const ObjectPathCallback& callback,
                    const ErrorCallback& error_callback));
  MOCK_METHOD3(GetService, void(const base::DictionaryValue& properties,
                                const ObjectPathCallback& callback,
                                const ErrorCallback& error_callback));
  MOCK_METHOD3(VerifyDestination,
               void(const VerificationProperties& properties,
                    const BooleanCallback& callback,
                    const ErrorCallback& error_callback));
  MOCK_METHOD4(VerifyAndEncryptCredentials,
               void(const VerificationProperties& properties,
                    const std::string& service_path,
                    const StringCallback& callback,
                    const ErrorCallback& error_callback));
  MOCK_METHOD4(VerifyAndEncryptData,
               void(const VerificationProperties& properties,
                    const std::string& data,
                    const StringCallback& callback,
                    const ErrorCallback& error_callback));
  MOCK_METHOD2(ConnectToBestServices,
               void(const base::Closure& callback,
                    const ErrorCallback& error_callback));
  MOCK_METHOD5(SetNetworkThrottlingStatus,
               void(const bool,
                    const uint32_t,
                    const uint32_t,
                    const base::Closure&,
                    const ErrorCallback& error_callback));
  MOCK_METHOD0(GetTestInterface, TestInterface*());
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_MOCK_SHILL_MANAGER_CLIENT_H_
