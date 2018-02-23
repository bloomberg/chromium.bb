// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_ML_USER_ACTIVITY_LOGGER_H_
#define CHROME_BROWSER_CHROMEOS_POWER_ML_USER_ACTIVITY_LOGGER_H_

#include "base/macros.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "base/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/power/ml/idle_event_notifier.h"
#include "chrome/browser/chromeos/power/ml/user_activity_event.pb.h"
#include "chrome/browser/chromeos/power/ml/user_activity_logger_delegate.h"
#include "chromeos/dbus/power_manager/idle.pb.h"
#include "chromeos/dbus/power_manager/policy.pb.h"
#include "chromeos/dbus/power_manager/suspend.pb.h"
#include "chromeos/dbus/power_manager_client.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "services/viz/public/interfaces/compositing/video_detector_observer.mojom.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/base/user_activity/user_activity_observer.h"

namespace chromeos {
namespace power {
namespace ml {

class BootClock;

// Logs user activity after an idle event is observed.
// TODO(renjieliu): Add power-related activity as well.
class UserActivityLogger : public ui::UserActivityObserver,
                           public IdleEventNotifier::Observer,
                           public PowerManagerClient::Observer,
                           public viz::mojom::VideoDetectorObserver,
                           public session_manager::SessionManagerObserver {
 public:
  // Delay after screen idle event, used to trigger TIMEOUT for user activity
  // logging.
  static constexpr base::TimeDelta kIdleDelay =
      base::TimeDelta::FromSeconds(10);

  // If a suspend has sleep duration shorter than this, the suspend would be
  // considered as cancelled and the event type will be REACTIVATE.
  static constexpr base::TimeDelta kMinSuspendDuration =
      base::TimeDelta::FromSeconds(10);

  UserActivityLogger(UserActivityLoggerDelegate* delegate,
                     IdleEventNotifier* idle_event_notifier,
                     ui::UserActivityDetector* detector,
                     chromeos::PowerManagerClient* power_manager_client,
                     session_manager::SessionManager* session_manager,
                     viz::mojom::VideoDetectorObserverRequest request,
                     const chromeos::ChromeUserManager* user_manager);
  ~UserActivityLogger() override;

  // ui::UserActivityObserver overrides.
  void OnUserActivity(const ui::Event* event) override;

  // chromeos::PowerManagerClient::Observer overrides:
  void LidEventReceived(chromeos::PowerManagerClient::LidState state,
                        const base::TimeTicks& timestamp) override;
  void PowerChanged(const power_manager::PowerSupplyProperties& proto) override;
  void TabletModeEventReceived(chromeos::PowerManagerClient::TabletMode mode,
                               const base::TimeTicks& timestamp) override;
  void ScreenIdleStateChanged(
      const power_manager::ScreenIdleState& proto) override;
  void SuspendImminent(power_manager::SuspendImminent::Reason reason) override;
  void SuspendDone(const base::TimeDelta& sleep_duration) override;
  void InactivityDelaysChanged(
      const power_manager::PowerManagementPolicy::Delays& delays) override;

  // viz::mojom::VideoDetectorObserver overrides:
  void OnVideoActivityStarted() override;
  void OnVideoActivityEnded() override {}

  // IdleEventNotifier::Observer overrides.
  void OnIdleEventObserved(
      const IdleEventNotifier::ActivityData& data) override;

  // session_manager::SessionManagerObserver overrides:
  void OnSessionStateChanged() override;

 private:
  friend class UserActivityLoggerTest;

  // Updates lid state and tablet mode from received switch states.
  void OnReceiveSwitchStates(
      base::Optional<chromeos::PowerManagerClient::SwitchStates> switch_states);

  void OnReceiveInactivityDelays(
      base::Optional<power_manager::PowerManagementPolicy::Delays> delays);

  // Extracts features from last known activity data and from device states.
  void ExtractFeatures(const IdleEventNotifier::ActivityData& activity_data);

  // Log event only when an idle event is observed.
  void MaybeLogEvent(UserActivityEvent::Event::Type type,
                     UserActivityEvent::Event::Reason reason);

  // Set the task runner for testing purpose.
  void SetTaskRunnerForTesting(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      std::unique_ptr<BootClock> test_boot_clock);

  // Time when an idle event is received and we start logging. Null if an idle
  // event hasn't been observed.
  base::Optional<base::TimeDelta> idle_event_start_since_boot_;

  chromeos::PowerManagerClient::LidState lid_state_ =
      chromeos::PowerManagerClient::LidState::NOT_PRESENT;

  chromeos::PowerManagerClient::TabletMode tablet_mode_ =
      chromeos::PowerManagerClient::TabletMode::UNSUPPORTED;

  UserActivityEvent::Features::DeviceType device_type_ =
      UserActivityEvent::Features::UNKNOWN_DEVICE;

  base::Optional<power_manager::PowerSupplyProperties::ExternalPower>
      external_power_;

  // Battery percent. This is in the range [0.0, 100.0].
  base::Optional<float> battery_percent_;

  // Indicates whether the screen is locked.
  bool screen_is_locked_ = false;

  // Features extracted when receives an idle event.
  UserActivityEvent::Features features_;

  // This is the reason for the in-progress suspend, i.e. it's set by
  // SuspendImminent and used by SuspendDone.
  base::Optional<power_manager::SuspendImminent::Reason> suspend_reason_;

  // It is RealBootClock, but will be set to FakeBootClock for tests.
  std::unique_ptr<BootClock> boot_clock_;

  UserActivityLoggerDelegate* const logger_delegate_;

  ScopedObserver<IdleEventNotifier, IdleEventNotifier::Observer>
      idle_event_observer_;
  ScopedObserver<ui::UserActivityDetector, ui::UserActivityObserver>
      user_activity_observer_;
  ScopedObserver<chromeos::PowerManagerClient,
                 chromeos::PowerManagerClient::Observer>
      power_manager_client_observer_;
  ScopedObserver<session_manager::SessionManager,
                 session_manager::SessionManagerObserver>
      session_manager_observer_;

  session_manager::SessionManager* const session_manager_;

  mojo::Binding<viz::mojom::VideoDetectorObserver> binding_;

  const chromeos::ChromeUserManager* const user_manager_;

  // Timer to be triggered when a screen idle event is triggered.
  base::OneShotTimer screen_idle_timer_;

  // Delays to dim and turn off the screen. Zero means disabled.
  base::TimeDelta screen_dim_delay_;
  base::TimeDelta screen_off_delay_;

  base::WeakPtrFactory<UserActivityLogger> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(UserActivityLogger);
};

}  // namespace ml
}  // namespace power
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_ML_USER_ACTIVITY_LOGGER_H_
