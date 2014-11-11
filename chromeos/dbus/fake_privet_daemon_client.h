// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_PRIVET_DAEMON_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_PRIVET_DAEMON_CLIENT_H_

#include "base/macros.h"
#include "chromeos/dbus/privet_daemon_client.h"

namespace chromeos {

// A fake implementation of PrivetDaemonClient. Invokes callbacks immediately.
class FakePrivetDaemonClient : public PrivetDaemonClient {
 public:
  FakePrivetDaemonClient();
  ~FakePrivetDaemonClient() override;

  // DBusClient overrides:
  void Init(dbus::Bus* bus) override;

  // PrivetClient overrides:
  void GetSetupStatus(const GetSetupStatusCallback& callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakePrivetDaemonClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_PRIVET_DAEMON_CLIENT_H_
