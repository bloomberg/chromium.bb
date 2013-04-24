// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_POWER_MANAGER_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_POWER_MANAGER_CLIENT_H_

#include <string>

#include "chromeos/dbus/power_manager/policy.pb.h"
#include "chromeos/dbus/power_manager_client.h"

namespace chromeos {

// A fake implementation of PowerManagerClient. This remembers the policy passed
// to SetPolicy() and the user of this class can inspect the last set policy by
// get_policy().
class FakePowerManagerClient : public PowerManagerClient {
 public:
  FakePowerManagerClient();
  virtual ~FakePowerManagerClient();

  // PowerManagerClient overrides.
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;
  virtual bool HasObserver(Observer* observer) OVERRIDE;
  virtual void DecreaseScreenBrightness(bool allow_off) OVERRIDE;
  virtual void IncreaseScreenBrightness() OVERRIDE;
  virtual void SetScreenBrightnessPercent(
      double percent, bool gradual) OVERRIDE;
  virtual void GetScreenBrightnessPercent(
      const GetScreenBrightnessPercentCallback& callback) OVERRIDE;
  virtual void DecreaseKeyboardBrightness() OVERRIDE;
  virtual void IncreaseKeyboardBrightness() OVERRIDE;
  virtual void RequestStatusUpdate(UpdateRequestType update_type) OVERRIDE;
  virtual void RequestRestart() OVERRIDE;
  virtual void RequestShutdown() OVERRIDE;
  virtual void RequestIdleNotification(int64 threshold_secs) OVERRIDE;
  virtual void NotifyUserActivity() OVERRIDE;
  virtual void NotifyVideoActivity(
      const base::TimeTicks& last_activity_time,
      bool is_fullscreen) OVERRIDE;
  virtual void SetPolicy(
      const power_manager::PowerManagementPolicy& policy) OVERRIDE;
  virtual void SetIsProjecting(bool is_projecting) OVERRIDE;
  virtual base::Closure GetSuspendReadinessCallback() OVERRIDE;

  power_manager::PowerManagementPolicy& get_policy() { return policy_; }

 private:
  power_manager::PowerManagementPolicy policy_;

  DISALLOW_COPY_AND_ASSIGN(FakePowerManagerClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_POWER_MANAGER_CLIENT_H_
