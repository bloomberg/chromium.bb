// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_MOCK_SHILL_PROFILE_CLIENT_H_
#define CHROMEOS_DBUS_MOCK_SHILL_PROFILE_CLIENT_H_

#include "base/values.h"
#include "chromeos/dbus/shill_profile_client.h"
#include "chromeos/dbus/shill_property_changed_observer.h"
#include "dbus/object_path.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class ShillPropertyChangedObserver;

class MockShillProfileClient : public ShillProfileClient {
 public:
  MockShillProfileClient();
  virtual ~MockShillProfileClient();

  MOCK_METHOD1(Init, void(dbus::Bus* bus));
  MOCK_METHOD2(AddPropertyChangedObserver,
               void(const dbus::ObjectPath& profile_path,
                    ShillPropertyChangedObserver* observer));
  MOCK_METHOD2(RemovePropertyChangedObserver,
               void(const dbus::ObjectPath& profile_path,
                    ShillPropertyChangedObserver* observer));
  MOCK_METHOD3(GetProperties, void(
      const dbus::ObjectPath& profile_path,
      const DictionaryValueCallbackWithoutStatus& callback,
      const ErrorCallback& error_callback));
  MOCK_METHOD4(GetEntry, void(
      const dbus::ObjectPath& profile_path,
      const std::string& entry_path,
      const DictionaryValueCallbackWithoutStatus& callback,
      const ErrorCallback& error_callback));
  MOCK_METHOD4(DeleteEntry, void(const dbus::ObjectPath& profile_path,
                                 const std::string& entry_path,
                                 const base::Closure& callback,
                                 const ErrorCallback& error_callback));
  MOCK_METHOD0(GetTestInterface, TestInterface*());
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_MOCK_SHILL_PROFILE_CLIENT_H_
