// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_MOCK_PERMISSION_BROKER_CLIENT_H_
#define CHROMEOS_DBUS_MOCK_PERMISSION_BROKER_CLIENT_H_

#include <stdint.h>

#include "chromeos/dbus/permission_broker_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace dbus {
class FileDescriptor;
}  // namespace dbus

namespace chromeos {

class MockPermissionBrokerClient : public PermissionBrokerClient {
 public:
  MockPermissionBrokerClient();
  ~MockPermissionBrokerClient() override;

  MOCK_METHOD1(Init, void(dbus::Bus* bus));
  MOCK_METHOD2(CheckPathAccess,
               void(const std::string& path, const ResultCallback& callback));
  MOCK_METHOD3(OpenPath,
               void(const std::string& path,
                    const OpenPathCallback& callback,
                    const ErrorCallback& error_callback));
  MOCK_METHOD4(RequestTcpPortAccess,
               void(uint16_t port,
                    const std::string& interface,
                    const dbus::FileDescriptor& lifeline_fd,
                    const ResultCallback& callback));
  MOCK_METHOD4(RequestUdpPortAccess,
               void(uint16_t port,
                    const std::string& interface,
                    const dbus::FileDescriptor& lifeline_fd,
                    const ResultCallback& callback));
  MOCK_METHOD3(ReleaseTcpPort,
               void(uint16_t port,
                    const std::string& interface,
                    const ResultCallback& callback));
  MOCK_METHOD3(ReleaseUdpPort,
               void(uint16_t port,
                    const std::string& interface,
                    const ResultCallback& callback));
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_MOCK_PERMISSION_BROKER_CLIENT_H_
