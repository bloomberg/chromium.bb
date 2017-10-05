// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_MOCK_SHILL_SERVICE_CLIENT_H_
#define CHROMEOS_DBUS_MOCK_SHILL_SERVICE_CLIENT_H_

#include <string>
#include <vector>

#include "base/values.h"
#include "chromeos/dbus/shill_property_changed_observer.h"
#include "chromeos/dbus/shill_service_client.h"
#include "dbus/object_path.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockShillServiceClient : public ShillServiceClient {
 public:
  MockShillServiceClient();
  virtual ~MockShillServiceClient();

  MOCK_METHOD1(Init, void(dbus::Bus* dbus));
  MOCK_METHOD2(AddPropertyChangedObserver,
               void(const dbus::ObjectPath& service_path,
                    ShillPropertyChangedObserver* observer));
  MOCK_METHOD2(RemovePropertyChangedObserver,
               void(const dbus::ObjectPath& service_path,
                    ShillPropertyChangedObserver* observer));
  MOCK_METHOD2(GetProperties, void(const dbus::ObjectPath& service_path,
                                   const DictionaryValueCallback& callback));
  MOCK_METHOD5(SetProperty, void(const dbus::ObjectPath& service_path,
                                 const std::string& name,
                                 const base::Value& value,
                                 const base::Closure& callback,
                                 const ErrorCallback& error_callback));
  MOCK_METHOD4(SetProperties, void(const dbus::ObjectPath& service_path,
                                   const base::DictionaryValue& properties,
                                   const base::Closure& callback,
                                   const ErrorCallback& error_callback));
  MOCK_METHOD4(ClearProperty, void(const dbus::ObjectPath& service_path,
                                   const std::string& name,
                                   const base::Closure& callback,
                                   const ErrorCallback& error_callback));
  MOCK_METHOD4(ClearProperties, void(const dbus::ObjectPath& service_path,
                                     const std::vector<std::string>& names,
                                     const ListValueCallback& callback,
                                     const ErrorCallback& error_callback));
  MOCK_METHOD3(Connect, void(const dbus::ObjectPath& service_path,
                             const base::Closure& callback,
                             const ErrorCallback& error_callback));
  MOCK_METHOD3(Disconnect, void(const dbus::ObjectPath& service_path,
                                const base::Closure& callback,
                                const ErrorCallback& error_callback));
  MOCK_METHOD3(Remove, void(const dbus::ObjectPath& service_path,
                            const base::Closure& callback,
                            const ErrorCallback& error_callback));
  MOCK_METHOD4(ActivateCellularModem,
               void(const dbus::ObjectPath& service_path,
                    const std::string& carrier,
                    const base::Closure& callback,
                    const ErrorCallback& error_callback));
  MOCK_METHOD3(CompleteCellularActivation,
               void(const dbus::ObjectPath& service_path,
                    const base::Closure& callback,
                    const ErrorCallback& error_callback));
  MOCK_METHOD2(GetLoadableProfileEntries,
               void(const dbus::ObjectPath& service_path,
                    const DictionaryValueCallback& callback));
  MOCK_METHOD0(GetTestInterface, TestInterface*());
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_MOCK_SHILL_SERVICE_CLIENT_H_
