// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_MOCK_PERMISSION_BROKER_CLIENT_H_
#define CHROMEOS_DBUS_MOCK_PERMISSION_BROKER_CLIENT_H_

#include <string>

#include "base/basictypes.h"
#include "chromeos/dbus/permission_broker_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockPermissionBrokerClient : public PermissionBrokerClient {
 public:
  MockPermissionBrokerClient();
  virtual ~MockPermissionBrokerClient();

  MOCK_METHOD2(RequestPathAccess, void(const std::string& path,
                                       const ResultCallback& callback));
  MOCK_METHOD3(RequestUsbAccess, void(const uint16_t vendor_id,
                                      const uint16_t product_id,
                                      const ResultCallback& callback));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPermissionBrokerClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_MOCK_PERMISSION_BROKER_CLIENT_H_
