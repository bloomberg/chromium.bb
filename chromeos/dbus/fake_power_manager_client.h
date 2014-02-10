// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_POWER_MANAGER_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_POWER_MANAGER_CLIENT_H_

#include <string>

#include "base/basictypes.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "chromeos/dbus/power_manager/policy.pb.h"
#include "chromeos/dbus/power_manager/suspend.pb.h"
#include "chromeos/dbus/power_manager_client.h"

namespace chromeos {

// A fake implementation of PowerManagerClient. This remembers the policy passed
// to SetPolicy() and the user of this class can inspect the last set policy by
// get_policy().
class FakePowerManagerClient : public PowerManagerClient {
 public:
  FakePowerManagerClient();
  virtual ~FakePowerManagerClient();

  power_manager::PowerManagementPolicy& policy() { return policy_; }
  int num_request_restart_calls() const {
    return num_request_restart_calls_;
  }
  int num_set_policy_calls() const {
    return num_set_policy_calls_;
  }
  int num_set_is_projecting_calls() const {
    return num_set_is_projecting_calls_;
  }
  bool is_projecting() const {
    return is_projecting_;
  }

  // PowerManagerClient overrides
  virtual void Init(dbus::Bus* bus) OVERRIDE;
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
  virtual void RequestStatusUpdate() OVERRIDE;
  virtual void RequestRestart() OVERRIDE;
  virtual void RequestShutdown() OVERRIDE;
  virtual void NotifyUserActivity(
      power_manager::UserActivityType type) OVERRIDE;
  virtual void NotifyVideoActivity(bool is_fullscreen) OVERRIDE;
  virtual void SetPolicy(
      const power_manager::PowerManagementPolicy& policy) OVERRIDE;
  virtual void SetIsProjecting(bool is_projecting) OVERRIDE;
  virtual base::Closure GetSuspendReadinessCallback() OVERRIDE;
  virtual int GetNumPendingSuspendReadinessCallbacks() OVERRIDE;

  // Emulates that the dbus server sends a message "SuspendImminent" to the
  // client.
  void SendSuspendImminent();

  // Emulates that the dbus server sends a message "SuspendStateChanged" to the
  // client.
  void SendSuspendStateChanged(
      const power_manager::SuspendState& suspend_state);

 private:
  ObserverList<Observer> observers_;

  // Last policy passed to SetPolicy().
  power_manager::PowerManagementPolicy policy_;

  // Last time passed to a SUSPEND_TO_MEMORY call to SendSuspendStateChanged().
  base::Time last_suspend_wall_time_;

  // Number of times that RequestRestart() has been called.
  int num_request_restart_calls_;

  // Number of times that SetPolicy() has been called.
  int num_set_policy_calls_;

  // Count the number of times SetIsProjecting() has been called.
  int num_set_is_projecting_calls_;

  // Last projecting state set in SetIsProjecting().
  bool is_projecting_;

  DISALLOW_COPY_AND_ASSIGN(FakePowerManagerClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_POWER_MANAGER_CLIENT_H_
