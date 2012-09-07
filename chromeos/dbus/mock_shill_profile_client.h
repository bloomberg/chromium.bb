// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_MOCK_SHILL_PROFILE_CLIENT_H_
#define CHROMEOS_DBUS_MOCK_SHILL_PROFILE_CLIENT_H_

#include "base/values.h"
#include "chromeos/dbus/shill_profile_client.h"
#include "dbus/object_path.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockShillProfileClient : public ShillProfileClient {
 public:
  MockShillProfileClient();
  virtual ~MockShillProfileClient();

  MOCK_METHOD2(SetPropertyChangedHandler,
               void(const dbus::ObjectPath& profile_path,
                    const PropertyChangedHandler& handler));
  MOCK_METHOD1(ResetPropertyChangedHandler,
               void(const dbus::ObjectPath& profile_path));
  MOCK_METHOD2(GetProperties, void(const dbus::ObjectPath& profile_path,
                                   const DictionaryValueCallback& callback));
  MOCK_METHOD3(GetEntry, void(const dbus::ObjectPath& profile_path,
                              const std::string& entry_path,
                              const DictionaryValueCallback& callback));
  MOCK_METHOD3(DeleteEntry, void(const dbus::ObjectPath& profile_path,
                                 const std::string& entry_path,
                                 const VoidDBusMethodCallback& callback));
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_MOCK_SHILL_PROFILE_CLIENT_H_
