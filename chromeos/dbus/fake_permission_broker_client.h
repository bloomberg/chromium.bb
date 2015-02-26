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
  ~FakePermissionBrokerClient() override;

  void Init(dbus::Bus* bus) override;
  void RequestPathAccess(const std::string& path,
                         int interface_id,
                         const ResultCallback& callback) override;
  void RequestTcpPortAccess(uint16 port,
                            const std::string& interface,
                            const dbus::FileDescriptor& lifeline_fd,
                            const ResultCallback& callback) override;
  void RequestUdpPortAccess(uint16 port,
                            const std::string& interface,
                            const dbus::FileDescriptor& lifeline_fd,
                            const ResultCallback& callback) override;
  void ReleaseTcpPort(uint16 port,
                      const std::string& interface,
                      const ResultCallback& callback) override;
  void ReleaseUdpPort(uint16 port,
                      const std::string& interface,
                      const ResultCallback& callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakePermissionBrokerClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_PERMISSION_BROKER_CLIENT_H_
