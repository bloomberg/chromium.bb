// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_PRIVET_DAEMON_MANAGER_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_PRIVET_DAEMON_MANAGER_CLIENT_H_

#include "chromeos/dbus/privet_daemon_manager_client.h"

namespace chromeos {

// A fake implementation of PrivetDaemonManagerClient. Invokes callbacks
// immediately.
class FakePrivetDaemonManagerClient : public PrivetDaemonManagerClient {
 public:
  FakePrivetDaemonManagerClient();
  ~FakePrivetDaemonManagerClient() override;

  // DBusClient overrides:
  void Init(dbus::Bus* bus) override;

  // PrivetDaemonManagerClient overrides:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void SetDescription(const std::string& description,
                      const VoidDBusMethodCallback& callback) override;
  const Properties* GetProperties() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakePrivetDaemonManagerClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_PRIVET_DAEMON_MANAGER_CLIENT_H_
