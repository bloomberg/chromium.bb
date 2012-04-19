// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_MOCK_FLIMFLAM_NETWORK_CLIENT_H_
#define CHROMEOS_DBUS_MOCK_FLIMFLAM_NETWORK_CLIENT_H_

#include "chromeos/dbus/flimflam_network_client.h"
#include "dbus/object_path.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockFlimflamNetworkClient : public FlimflamNetworkClient {
 public:
  MockFlimflamNetworkClient();
  virtual ~MockFlimflamNetworkClient();

  MOCK_METHOD2(SetPropertyChangedHandler,
               void(const dbus::ObjectPath& network_path,
                    const PropertyChangedHandler& handler));
  MOCK_METHOD1(ResetPropertyChangedHandler,
               void(const dbus::ObjectPath& network_path));
  MOCK_METHOD2(GetProperties, void(const dbus::ObjectPath& network_path,
                                   const DictionaryValueCallback& callback));
  MOCK_METHOD1(CallGetPropertiesAndBlock,
               base::DictionaryValue*(const dbus::ObjectPath& network_path));
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_MOCK_FLIMFLAM_NETWORK_CLIENT_H_
