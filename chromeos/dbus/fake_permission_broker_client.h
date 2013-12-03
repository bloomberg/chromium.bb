// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_PERMISSION_BROKER_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_PERMISSION_BROKER_CLIENT_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chromeos/dbus/permission_broker_client.h"

namespace chromeos {

class FakePermissionBrokerClient : public PermissionBrokerClient {
 public:
  FakePermissionBrokerClient();
  virtual ~FakePermissionBrokerClient();

  virtual void Init(dbus::Bus* bus) OVERRIDE;
  virtual void RequestPathAccess(const std::string& path,
                                 int interface_id,
                                 const ResultCallback& callback) OVERRIDE;
  virtual void RequestUsbAccess(const uint16_t vendor_id,
                                const uint16_t product_id,
                                int interface_id,
                                const ResultCallback& callback) OVERRIDE;
 private:
  DISALLOW_COPY_AND_ASSIGN(FakePermissionBrokerClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_PERMISSION_BROKER_CLIENT_H_
