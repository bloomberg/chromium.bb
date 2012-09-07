// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_MOCK_SHILL_DEVICE_CLIENT_H_
#define CHROMEOS_DBUS_MOCK_SHILL_DEVICE_CLIENT_H_

#include "base/values.h"
#include "chromeos/dbus/shill_device_client.h"
#include "dbus/object_path.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockShillDeviceClient : public ShillDeviceClient {
 public:
  MockShillDeviceClient();
  virtual ~MockShillDeviceClient();

  MOCK_METHOD2(SetPropertyChangedHandler,
               void(const dbus::ObjectPath& device_path,
                    const PropertyChangedHandler& handler));
  MOCK_METHOD1(ResetPropertyChangedHandler,
               void(const dbus::ObjectPath& device_path));
  MOCK_METHOD2(GetProperties, void(const dbus::ObjectPath& device_path,
                                   const DictionaryValueCallback& callback));
  MOCK_METHOD1(CallGetPropertiesAndBlock,
               base::DictionaryValue*(const dbus::ObjectPath& device_path));
  MOCK_METHOD2(ProposeScan, void(const dbus::ObjectPath& device_path,
                                 const VoidDBusMethodCallback& callback));
  MOCK_METHOD4(SetProperty, void(const dbus::ObjectPath& device_path,
                                 const std::string& name,
                                 const base::Value& value,
                                 const VoidDBusMethodCallback& callback));
  MOCK_METHOD3(ClearProperty, void(const dbus::ObjectPath& device_path,
                                   const std::string& name,
                                   const VoidDBusMethodCallback& callback));
  MOCK_METHOD3(AddIPConfig, void(const dbus::ObjectPath& device_path,
                                 const std::string& method,
                                 const ObjectPathDBusMethodCallback& callback));
  MOCK_METHOD2(CallAddIPConfigAndBlock,
               dbus::ObjectPath(const dbus::ObjectPath& device_path,
                                const std::string& method));
  MOCK_METHOD5(RequirePin, void(const dbus::ObjectPath& device_path,
                                const std::string& pin,
                                bool require,
                                const base::Closure& callback,
                                const ErrorCallback& error_callback));
  MOCK_METHOD4(EnterPin, void(const dbus::ObjectPath& device_path,
                              const std::string& pin,
                              const base::Closure& callback,
                              const ErrorCallback& error_callback));
  MOCK_METHOD5(UnblockPin, void(const dbus::ObjectPath& device_path,
                                const std::string& puk,
                                const std::string& pin,
                                const base::Closure& callback,
                                const ErrorCallback& error_callback));
  MOCK_METHOD5(ChangePin, void(const dbus::ObjectPath& device_path,
                               const std::string& old_pin,
                               const std::string& new_pin,
                               const base::Closure& callback,
                               const ErrorCallback& error_callback));
  MOCK_METHOD4(Register, void(const dbus::ObjectPath& device_path,
                              const std::string& network_id,
                              const base::Closure& callback,
                              const ErrorCallback& error_callback));
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_MOCK_SHILL_DEVICE_CLIENT_H_
