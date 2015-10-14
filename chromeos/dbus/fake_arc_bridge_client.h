// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_FAKE_ARC_BRIDGE_CLIENT_CLIENT_H_
#define CHROMEOS_FAKE_ARC_BRIDGE_CLIENT_CLIENT_H_

#include "base/basictypes.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/arc_bridge_client.h"
#include "chromeos/dbus/dbus_client.h"

namespace chromeos {

// A fake implementation of ArcBridgeClient.
class CHROMEOS_EXPORT FakeArcBridgeClient : public ArcBridgeClient {
 public:
  FakeArcBridgeClient();
  ~FakeArcBridgeClient() override;

  // DBusClient overrides:
  void Init(dbus::Bus* bus) override;

  // ArcBridgeClient overrides:
  void Ping(const VoidDBusMethodCallback& callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeArcBridgeClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_FAKE_ARC_BRIDGE_CLIENT_CLIENT_H_
