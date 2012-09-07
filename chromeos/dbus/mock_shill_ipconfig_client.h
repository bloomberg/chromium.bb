// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_MOCK_SHILL_IPCONFIG_CLIENT_H_
#define CHROMEOS_DBUS_MOCK_SHILL_IPCONFIG_CLIENT_H_

#include "base/values.h"
#include "chromeos/dbus/shill_ipconfig_client.h"
#include "dbus/object_path.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockShillIPConfigClient : public ShillIPConfigClient {
 public:
  MockShillIPConfigClient();
  virtual ~MockShillIPConfigClient();

  MOCK_METHOD2(SetPropertyChangedHandler,
               void(const dbus::ObjectPath& ipconfig_path,
                    const PropertyChangedHandler& handler));
  MOCK_METHOD1(ResetPropertyChangedHandler,
               void(const dbus::ObjectPath& ipconfig_path));
  MOCK_METHOD2(Refresh, void(const dbus::ObjectPath& ipconfig_path,
                             const VoidDBusMethodCallback& callback));
  MOCK_METHOD2(GetProperties, void(const dbus::ObjectPath& ipconfig_path,
                                   const DictionaryValueCallback& callback));
  MOCK_METHOD1(CallGetPropertiesAndBlock,
               base::DictionaryValue*(const dbus::ObjectPath& ipconfig_path));
  MOCK_METHOD4(SetProperty, void(const dbus::ObjectPath& ipconfig_path,
                                 const std::string& name,
                                 const base::Value& value,
                                 const VoidDBusMethodCallback& callback));
  MOCK_METHOD3(ClearProperty, void(const dbus::ObjectPath& ipconfig_path,
                                   const std::string& name,
                                   const VoidDBusMethodCallback& callback));
  MOCK_METHOD2(Remove, void(const dbus::ObjectPath& ipconfig_path,
                            const VoidDBusMethodCallback& callback));
  MOCK_METHOD1(CallRemoveAndBlock, bool(const dbus::ObjectPath& ipconfig_path));
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_MOCK_SHILL_IPCONFIG_CLIENT_H_
