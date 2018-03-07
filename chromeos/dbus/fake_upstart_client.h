// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_UPSTART_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_UPSTART_CLIENT_H_

#include "base/macros.h"

#include "chromeos/dbus/upstart_client.h"

namespace chromeos {

class CHROMEOS_EXPORT FakeUpstartClient : public UpstartClient {
 public:
  FakeUpstartClient();
  ~FakeUpstartClient() override;

  int hammerd_start_count() const { return hammerd_start_count_; }

  // DBusClient overrides.
  void Init(dbus::Bus* bus) override;

  // UpstartClient overrides.
  void StartAuthPolicyService() override;
  void RestartAuthPolicyService() override;
  void StartMediaAnalytics(const std::vector<std::string>& upstart_env,
                           VoidDBusMethodCallback callback) override;
  void RestartMediaAnalytics(VoidDBusMethodCallback callback) override;
  void StopMediaAnalytics() override;
  void StopMediaAnalytics(VoidDBusMethodCallback callback) override;
  void StartHammerd() override;

 private:
  // The number of times hammerd was started.
  int hammerd_start_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(FakeUpstartClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_UPSTART_CLIENT_H_
