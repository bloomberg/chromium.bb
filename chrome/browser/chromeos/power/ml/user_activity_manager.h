// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_ML_USER_ACTIVITY_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_POWER_ML_USER_ACTIVITY_MANAGER_H_

#include "base/macros.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "base/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/power/ml/idle_event_notifier.h"
#include "chrome/browser/chromeos/power/ml/smart_dim/model.h"
#include "chrome/browser/chromeos/power/ml/user_activity_event.pb.h"
#include "chrome/browser/chromeos/power/ml/user_activity_ukm_logger.h"
#include "chrome/browser/resource_coordinator/tab_metrics_event.pb.h"
#include "chromeos/dbus/power_manager/idle.pb.h"
#include "chromeos/dbus/power_manager/policy.pb.h"
#include "chromeos/dbus/power_manager/suspend.pb.h"
#include "chromeos/dbus/power_manager_client.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/viz/public/interfaces/compositing/video_detector_observer.mojom.h"
#include "ui/aura/window.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/base/user_activity/user_activity_observer.h"

namespace chromeos {
namespace power {
namespace ml {

class BootClock;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// The values below are not mutually exclusive. kError is any error which could
// be any of the other kErrors.
enum class PreviousEventLoggingResult {
  kSuccess = 0,
  kError = 1,
  kErrorModelPredictionMissing = 2,
  kErrorModelDisabled = 3,
  kErrorMultiplePreviousEvents = 4,
  kErrorIdleStartMissing = 5,
  kMaxValue = kErrorIdleStartMissing
};

struct TabProperty {
  ukm::SourceId source_id = -1;
  std::string domain;
  // Tab URL's engagement score. -1 if engagement service is disabled.
  int engagement_score = -1;
  // Whether user has form entry, i.e. text input.
  bool has_form_entry;
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// What happens after a screen dim imminent is received.
enum class DimImminentAction {
  kModelIgnored = 0,
  kModelDim = 1,
  kModelNoDim = 2,
  kMaxValue = kModelNoDim
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class FinalResult { kReactivation = 0, kOff = 1, kMaxValue = kOff };

// Logs user activity after an idle event is observed.
// TODO(renjieliu): Add power-related activity as well.
class UserActivityManager : public ui::UserActivityObserver,
                            public IdleEventNotifier::Observer,
                            public PowerManagerClient::Observer,
                            public viz::mojom::VideoDetectorObserver,
                            public session_manager::SessionManagerObserver {
 public:
  UserActivityManager(UserActivityUkmLogger* ukm_logger,
                      IdleEventNotifier* idle_event_notifier,
                      ui::UserActivityDetector* detector,
                      chromeos::PowerManagerClient* power_manager_client,
                      session_manager::SessionManager* session_manager,
                      viz::mojom::VideoDetectorObserverRequest request,
                      const chromeos::ChromeUserManager* user_manager,
                      SmartDimModel* smart_dim_model);
  ~UserActivityManager() override;

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
  friend class UserActivityManagerTest;

  // Data structure associated with the 1st ScreenDimImminent event. See
  // PopulatePreviousEventData function below.
  struct PreviousIdleEventData;

  // Updates lid state and tablet mode from received switch states.
  void OnReceiveSwitchStates(
      base::Optional<chromeos::PowerManagerClient::SwitchStates> switch_states);

  void OnReceiveInactivityDelays(
      base::Optional<power_manager::PowerManagementPolicy::Delays> delays);

  // Gets properties of active tab from visible focused/topmost browser.
  TabProperty UpdateOpenTabURL();

  // Extracts features from last known activity data and from device states.
  void ExtractFeatures(const IdleEventNotifier::ActivityData& activity_data);

  // Log event only when an idle event is observed.
  void MaybeLogEvent(UserActivityEvent::Event::Type type,
                     UserActivityEvent::Event::Reason reason);

  // Set the task runner for testing purpose.
  void SetTaskRunnerForTesting(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      std::unique_ptr<BootClock> test_boot_clock);

  // We could have two consecutive idle events (i.e. two ScreenDimImminent)
  // without a final event logged in between. This could happen when the 1st
  // screen dim is deferred and after another idle period, powerd decides to
  // dim the screen again. We want to log both events. Hence we record the
  // event data associated with the 1st ScreenDimImminent using the method
  // below.
  void PopulatePreviousEventData(const base::TimeDelta& now);

  void ResetAfterLogging();

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

  // It is RealBootClock, but will be set to FakeBootClock for tests.
  std::unique_ptr<BootClock> boot_clock_;

  UserActivityUkmLogger* const ukm_logger_;

  SmartDimModel* const smart_dim_model_;

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

  chromeos::PowerManagerClient* const power_manager_client_;

  // Delays to dim and turn off the screen. Zero means disabled.
  base::TimeDelta screen_dim_delay_;
  base::TimeDelta screen_off_delay_;

  // Whether screen is currently dimmed/off.
  bool screen_dimmed_ = false;
  bool screen_off_ = false;
  // Whether screen dim/off occurred before final event was logged. They are
  // reset to false at the start of each idle event.
  bool screen_dim_occurred_ = false;
  bool screen_off_occurred_ = false;
  bool screen_lock_occurred_ = false;

  // Number of positive/negative actions up to but excluding the current event.
  // REACTIVATE is a negative action, all other event types (OFF, TIMEOUT) are
  // positive actions.
  int previous_negative_actions_count_ = 0;
  int previous_positive_actions_count_ = 0;

  // Whether screen-dim was deferred by the model when the previous
  // ScreenDimImminent event arrived.
  bool dim_deferred_ = false;
  // Whether we are waiting for the final action after an idle event. It's only
  // set to true after we've received an idle event, but haven't received final
  // action to log the event.
  bool waiting_for_final_action_ = false;

  // Model prediction for the current ScreenDimImminent event. Unset if
  // model prediction is disabled by an experiment.
  base::Optional<UserActivityEvent::ModelPrediction> model_prediction_;

  std::unique_ptr<PreviousIdleEventData> previous_idle_event_data_;

  base::WeakPtrFactory<UserActivityManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(UserActivityManager);
};

}  // namespace ml
}  // namespace power
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_ML_USER_ACTIVITY_MANAGER_H_
