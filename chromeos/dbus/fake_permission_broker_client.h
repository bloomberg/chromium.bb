// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_PERMISSION_BROKER_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_PERMISSION_BROKER_CLIENT_H_

#include <stdint.h>

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chromeos/dbus/permission_broker_client.h"

namespace chromeos {

class CHROMEOS_EXPORT FakePermissionBrokerClient
    : public PermissionBrokerClient {
 public:
  FakePermissionBrokerClient();
  ~FakePermissionBrokerClient() override;

  void Init(dbus::Bus* bus) override;
  void CheckPathAccess(const std::string& path,
                       const ResultCallback& callback) override;
  void OpenPath(const std::string& path,
                const OpenPathCallback& callback,
                const ErrorCallback& error_callback) override;
  void RequestTcpPortAccess(uint16_t port,
                            const std::string& interface,
                            int lifeline_fd,
                            const ResultCallback& callback) override;
  void RequestUdpPortAccess(uint16_t port,
                            const std::string& interface,
                            int lifeline_fd,
                            const ResultCallback& callback) override;
  void ReleaseTcpPort(uint16_t port,
                      const std::string& interface,
                      const ResultCallback& callback) override;
  void ReleaseUdpPort(uint16_t port,
                      const std::string& interface,
                      const ResultCallback& callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakePermissionBrokerClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_PERMISSION_BROKER_CLIENT_H_
