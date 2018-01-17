// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_POWER_MANAGER_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_POWER_MANAGER_CLIENT_H_

#include <queue>
#include <string>
#include <utility>

#include "base/callback_forward.h"
#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/power_manager/policy.pb.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "chromeos/dbus/power_manager/suspend.pb.h"
#include "chromeos/dbus/power_manager_client.h"

namespace chromeos {

// A fake implementation of PowerManagerClient. This remembers the policy passed
// to SetPolicy() and the user of this class can inspect the last set policy by
// get_policy().
class CHROMEOS_EXPORT FakePowerManagerClient : public PowerManagerClient {
 public:
  FakePowerManagerClient();
  ~FakePowerManagerClient() override;

  const power_manager::PowerManagementPolicy& policy() { return policy_; }
  const power_manager::PowerSupplyProperties& props() const { return props_; }
  int num_request_restart_calls() const { return num_request_restart_calls_; }
  int num_request_shutdown_calls() const { return num_request_shutdown_calls_; }
  int num_set_policy_calls() const { return num_set_policy_calls_; }
  int num_set_is_projecting_calls() const {
    return num_set_is_projecting_calls_;
  }
  double screen_brightness_percent() const {
    return screen_brightness_percent_.value();
  }
  bool is_projecting() const { return is_projecting_; }
  bool have_video_activity_report() const {
    return !video_activity_reports_.empty();
  }
  bool backlights_forced_off() const { return backlights_forced_off_; }
  int num_set_backlights_forced_off_calls() const {
    return num_set_backlights_forced_off_calls_;
  }
  void set_enqueue_brightness_changes_on_backlights_forced_off(bool enqueue) {
    enqueue_brightness_changes_on_backlights_forced_off_ = enqueue;
  }
  const std::queue<double>& pending_brightness_changes() const {
    return pending_brightness_changes_;
  }
  void set_user_activity_callback(base::RepeatingClosure callback) {
    user_activity_callback_ = std::move(callback);
  }

  // PowerManagerClient overrides:
  void Init(dbus::Bus* bus) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  bool HasObserver(const Observer* observer) const override;
  void SetRenderProcessManagerDelegate(
      base::WeakPtr<RenderProcessManagerDelegate> delegate) override;
  void DecreaseScreenBrightness(bool allow_off) override;
  void IncreaseScreenBrightness() override;
  void SetScreenBrightnessPercent(double percent, bool gradual) override;
  void GetScreenBrightnessPercent(DBusMethodCallback<double> callback) override;
  void DecreaseKeyboardBrightness() override;
  void IncreaseKeyboardBrightness() override;
  void RequestStatusUpdate() override;
  void RequestSuspend() override;
  void RequestRestart(power_manager::RequestRestartReason reason,
                      const std::string& description) override;
  void RequestShutdown(power_manager::RequestShutdownReason reason,
                       const std::string& description) override;
  void NotifyUserActivity(power_manager::UserActivityType type) override;
  void NotifyVideoActivity(bool is_fullscreen) override;
  void SetPolicy(const power_manager::PowerManagementPolicy& policy) override;
  void SetIsProjecting(bool is_projecting) override;
  void SetPowerSource(const std::string& id) override;
  void SetBacklightsForcedOff(bool forced_off) override;
  void GetBacklightsForcedOff(DBusMethodCallback<bool> callback) override;
  void GetSwitchStates(DBusMethodCallback<SwitchStates> callback) override;
  void GetInactivityDelays(
      DBusMethodCallback<power_manager::PowerManagementPolicy::Delays> callback)
      override;
  base::Closure GetSuspendReadinessCallback() override;
  int GetNumPendingSuspendReadinessCallbacks() override;

  // Pops the first report from |video_activity_reports_|, returning whether the
  // activity was fullscreen or not. There must be at least one report.
  bool PopVideoActivityReport();

  // Emulates the power manager announcing that the system is starting or
  // completing a suspend attempt.
  void SendSuspendImminent(power_manager::SuspendImminent::Reason reason);
  void SendSuspendDone();
  void SendDarkSuspendImminent();

  // Emulates the power manager announcing that the system is changing
  // display brightness to |level|.
  void SendBrightnessChanged(int level, bool user_initiated);

  // Emulates the power manager announcing that the system is changing
  // keyboard brightness to |level|.
  void SendKeyboardBrightnessChanged(int level, bool user_initiated);

  // Notifies observers about the screen idle state changing.
  void SendScreenIdleStateChanged(const power_manager::ScreenIdleState& proto);

  // Notifies observers that the power button has been pressed or released.
  void SendPowerButtonEvent(bool down, const base::TimeTicks& timestamp);

  // Sets |lid_state_| or |tablet_mode_| and notifies |observers_| about the
  // change.
  void SetLidState(LidState state, const base::TimeTicks& timestamp);
  void SetTabletMode(TabletMode mode, const base::TimeTicks& timestamp);

  // Sets |inactivity_delays_| and notifies |observers_| about the change.
  void SetInactivityDelays(
      const power_manager::PowerManagementPolicy::Delays& delays);

  // Updates |props_| and notifies observers of its changes.
  void UpdatePowerProperties(
      const power_manager::PowerSupplyProperties& power_props);

  // The PowerAPI requests system wake lock asynchronously. Test can run a
  // RunLoop and set the quit closure by this function to make sure the wake
  // lock has been created.
  void SetPowerPolicyQuitClosure(base::OnceClosure quit_closure);

  // Updates screen brightness to the first pending value in
  // |pending_brightness_changes_|.
  // Returns whether the screen brightness change was applied - this will
  // return false if there are no pending brightness changes.
  bool ApplyPendingBrightnessChange();

  // Sets the screen brightness percent to be returned.
  // The nullopt |percent| means an error. In case of success,
  // |percent| must be in the range of [0, 100].
  void set_screen_brightness_percent(const base::Optional<double>& percent) {
    screen_brightness_percent_ = percent;
  }

 private:
  // Callback that will be run by asynchronous suspend delays to report
  // readiness.
  void HandleSuspendReadiness();

  // Notifies |observers_| that |props_| has been updated.
  void NotifyObservers();

  base::ObserverList<Observer> observers_;

  // Last policy passed to SetPolicy().
  power_manager::PowerManagementPolicy policy_;

  // Power status received from the power manager.
  power_manager::PowerSupplyProperties props_;

  // Number of times that various methods have been called.
  int num_request_restart_calls_ = 0;
  int num_request_shutdown_calls_ = 0;
  int num_set_policy_calls_ = 0;
  int num_set_is_projecting_calls_ = 0;
  int num_set_backlights_forced_off_calls_ = 0;

  // Number of pending suspend readiness callbacks.
  int num_pending_suspend_readiness_callbacks_ = 0;

  // Current screen brightness in the range [0.0, 100.0].
  base::Optional<double> screen_brightness_percent_;

  // Last screen brightness requested via SetScreenBrightnessPercent().
  // Unlike |screen_brightness_percent_|, this value will not be changed by
  // SetBacklightsForcedOff() method - a method that implicitly changes screen
  // brightness.
  // Initially set to an arbitrary non-null value.
  double requested_screen_brightness_percent_ = 80;

  // Last projecting state set in SetIsProjecting().
  bool is_projecting_ = false;

  // Display and keyboard backlights (if present) forced off state set in
  // SetBacklightsForcedOff().
  bool backlights_forced_off_ = false;

  // Whether screen brightness changes in SetBacklightsForcedOff() should be
  // enqueued.
  // If not set, SetBacklightsForcedOff() will update current screen
  // brightness and send a brightness change event (provided undimmed
  // brightness percent is set).
  // If set, brightness changes will be enqueued to
  // pending_brightness_changes_, and will have to be applied explicitly by
  // calling ApplyPendingBrightnessChange().
  bool enqueue_brightness_changes_on_backlights_forced_off_ = false;

  // Pending brightness changes caused by SetBacklightsForcedOff().
  // ApplyPendingBrightnessChange() applies the first pending change.
  std::queue<double> pending_brightness_changes_;

  // Delays returned by GetInactivityDelays().
  power_manager::PowerManagementPolicy::Delays inactivity_delays_;

  // States returned by GetSwitchStates().
  LidState lid_state_ = LidState::OPEN;
  TabletMode tablet_mode_ = TabletMode::UNSUPPORTED;

  // Video activity reports that we were requested to send, in the order they
  // were requested. True if fullscreen.
  base::circular_deque<bool> video_activity_reports_;

  // Delegate for managing power consumption of Chrome's renderer processes.
  base::WeakPtr<RenderProcessManagerDelegate> render_process_manager_delegate_;

  // If non-empty, called by SetPowerPolicy().
  base::OnceClosure power_policy_quit_closure_;

  // If non-empty, called by NotifyUserActivity().
  base::RepeatingClosure user_activity_callback_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<FakePowerManagerClient> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakePowerManagerClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_POWER_MANAGER_CLIENT_H_
