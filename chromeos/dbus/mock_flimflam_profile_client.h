// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_MOCK_FLIMFLAM_PROFILE_CLIENT_H_
#define CHROMEOS_DBUS_MOCK_FLIMFLAM_PROFILE_CLIENT_H_

#include "base/values.h"
#include "chromeos/dbus/flimflam_profile_client.h"
#include "dbus/object_path.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockFlimflamProfileClient : public FlimflamProfileClient {
 public:
  MockFlimflamProfileClient();
  virtual ~MockFlimflamProfileClient();

  MOCK_METHOD1(SetPropertyChangedHandler,
               void(const PropertyChangedHandler& handler));
  MOCK_METHOD0(ResetPropertyChangedHandler, void());
  MOCK_METHOD1(GetProperties, void(const DictionaryValueCallback& callback));
  MOCK_METHOD2(GetEntry, void(const dbus::ObjectPath& path,
                              const DictionaryValueCallback& callback));
  MOCK_METHOD2(DeleteEntry, void(const dbus::ObjectPath& path,
                                 const VoidCallback& callback));
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_MOCK_FLIMFLAM_PROFILE_CLIENT_H_
