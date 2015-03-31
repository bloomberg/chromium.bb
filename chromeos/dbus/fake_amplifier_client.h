// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_AMPLIFIER_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_AMPLIFIER_CLIENT_H_

#include "base/basictypes.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/amplifier_client.h"
#include "chromeos/dbus/dbus_client.h"

namespace chromeos {

// A fake implementation of AmplifierClient. Invokes callbacks immediately.
class CHROMEOS_EXPORT FakeAmplifierClient : public AmplifierClient {
 public:
  FakeAmplifierClient();
  ~FakeAmplifierClient() override;

  // DBusClient overrides:
  void Init(dbus::Bus* bus) override;

  // AmplifierClient overrides:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void Initialize(const BoolDBusMethodCallback& callback) override;
  void SetStandbyMode(bool standby,
                      const VoidDBusMethodCallback& callback) override;
  void SetVolume(double db_spl,
                 const VoidDBusMethodCallback& callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeAmplifierClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_AMPLIFIER_CLIENT_H_
