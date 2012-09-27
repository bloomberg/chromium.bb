// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_MOCK_SHILL_NETWORK_CLIENT_H_
#define CHROMEOS_DBUS_MOCK_SHILL_NETWORK_CLIENT_H_

#include "chromeos/dbus/shill_network_client.h"
#include "chromeos/dbus/shill_property_changed_observer.h"
#include "dbus/object_path.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockShillNetworkClient : public ShillNetworkClient {
 public:
  MockShillNetworkClient();
  virtual ~MockShillNetworkClient();

  MOCK_METHOD2(AddPropertyChangedObserver,
               void(const dbus::ObjectPath& network_path,
                    ShillPropertyChangedObserver* observer));
  MOCK_METHOD2(RemovePropertyChangedObserver,
               void(const dbus::ObjectPath& network_path,
                    ShillPropertyChangedObserver* observer));
  MOCK_METHOD2(GetProperties, void(const dbus::ObjectPath& network_path,
                                   const DictionaryValueCallback& callback));
  MOCK_METHOD1(CallGetPropertiesAndBlock,
               base::DictionaryValue*(const dbus::ObjectPath& network_path));
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_MOCK_SHILL_NETWORK_CLIENT_H_
