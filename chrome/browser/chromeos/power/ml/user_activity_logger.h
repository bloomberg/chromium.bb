// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_ML_USER_ACTIVITY_LOGGER_H_
#define CHROME_BROWSER_CHROMEOS_POWER_ML_USER_ACTIVITY_LOGGER_H_

#include "base/macros.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/power/ml/idle_event_notifier.h"
#include "chrome/browser/chromeos/power/ml/user_activity_event.pb.h"
#include "chrome/browser/chromeos/power/ml/user_activity_logger_delegate.h"
#include "chromeos/dbus/power_manager_client.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/base/user_activity/user_activity_observer.h"

namespace chromeos {
namespace power {
namespace ml {

// Logs user activity after an idle event is observed.
// TODO(renjieliu): Add power-related activity as well.
class UserActivityLogger : public ui::UserActivityObserver,
                           public IdleEventNotifier::Observer,
                           public PowerManagerClient::Observer {
 public:
  UserActivityLogger(UserActivityLoggerDelegate* delegate,
                     IdleEventNotifier* idle_event_notifier,
                     ui::UserActivityDetector* detector,
                     chromeos::PowerManagerClient* power_manager_client);
  ~UserActivityLogger() override;

  // chromeos::PowerManagerClient::Observer overrides:
  void LidEventReceived(chromeos::PowerManagerClient::LidState state,
                        const base::TimeTicks& timestamp) override;
  void PowerChanged(const power_manager::PowerSupplyProperties& proto) override;
  void TabletModeEventReceived(chromeos::PowerManagerClient::TabletMode mode,
                               const base::TimeTicks& timestamp) override;

  // ui::UserActivityObserver overrides.
  void OnUserActivity(const ui::Event* event) override;

  // IdleEventNotifier::Observer overrides.
  void OnIdleEventObserved(
      const IdleEventNotifier::ActivityData& data) override;

 private:
  friend class UserActivityLoggerTest;

  // Updates lid state and tablet mode from received switch states.
  void OnReceiveSwitchStates(
      base::Optional<chromeos::PowerManagerClient::SwitchStates> switch_states);

  // Extracts features from last known activity data and from device states.
  void ExtractFeatures(const IdleEventNotifier::ActivityData& activity_data);

  // Flag indicating whether an idle event has been observed.
  bool idle_event_observed_ = false;

  chromeos::PowerManagerClient::LidState lid_state_ =
      chromeos::PowerManagerClient::LidState::NOT_PRESENT;

  chromeos::PowerManagerClient::TabletMode tablet_mode_ =
      chromeos::PowerManagerClient::TabletMode::UNSUPPORTED;

  UserActivityEvent::Features::DeviceType device_type_ =
      UserActivityEvent::Features::UNKNOWN_DEVICE;

  // Flag indicating whether the device is on battery.
  bool on_battery_ = false;

  // Battery percent. This is in the range [0.0, 100.0].
  base::Optional<float> battery_percent_;

  // Features extracted when receives an idle event.
  UserActivityEvent::Features features_;

  UserActivityLoggerDelegate* const logger_delegate_;

  ScopedObserver<IdleEventNotifier, IdleEventNotifier::Observer>
      idle_event_observer_;

  ScopedObserver<ui::UserActivityDetector, ui::UserActivityObserver>
      user_activity_observer_;

  ScopedObserver<chromeos::PowerManagerClient,
                 chromeos::PowerManagerClient::Observer>
      power_manager_client_observer_;

  DISALLOW_COPY_AND_ASSIGN(UserActivityLogger);
};

}  // namespace ml
}  // namespace power
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_ML_USER_ACTIVITY_LOGGER_H_
