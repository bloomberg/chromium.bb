// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/ml/user_activity_manager.h"

#include <cmath>

#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/chromeos/power/ml/real_boot_clock.h"
#include "chrome/browser/resource_coordinator/tab_metrics_logger.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "chromeos/system/devicetype.h"
#include "components/ukm/content/source_url_recorder.h"
#include "content/public/common/page_importance_signals.h"

namespace chromeos {
namespace power {
namespace ml {

namespace {
void LogPowerMLPreviousEventLoggingResult(PreviousEventLoggingResult result) {
  UMA_HISTOGRAM_ENUMERATION("PowerML.PreviousEventLogging.Result", result);
}
}  // namespace

struct UserActivityManager::PreviousIdleEventData {
  // Gap between two ScreenDimImminent signals.
  base::TimeDelta dim_imminent_signal_interval;
  // Features recorded for the ScreenDimImminent signal at the beginning of
  // |dim_imminent_signal_interval|.
  UserActivityEvent::Features features;
  // Model prediction recorded for the ScreenDimImminent signal at the beginning
  // of |dim_imminent_signal_interval|.
  UserActivityEvent::ModelPrediction model_prediction;
};

UserActivityManager::UserActivityManager(
    UserActivityUkmLogger* ukm_logger,
    IdleEventNotifier* idle_event_notifier,
    ui::UserActivityDetector* detector,
    chromeos::PowerManagerClient* power_manager_client,
    session_manager::SessionManager* session_manager,
    viz::mojom::VideoDetectorObserverRequest request,
    const chromeos::ChromeUserManager* user_manager,
    SmartDimModel* smart_dim_model)
    : boot_clock_(std::make_unique<RealBootClock>()),
      ukm_logger_(ukm_logger),
      smart_dim_model_(smart_dim_model),
      idle_event_observer_(this),
      user_activity_observer_(this),
      power_manager_client_observer_(this),
      session_manager_observer_(this),
      session_manager_(session_manager),
      binding_(this, std::move(request)),
      user_manager_(user_manager),
      power_manager_client_(power_manager_client),
      weak_ptr_factory_(this) {
  DCHECK(ukm_logger_);
  DCHECK(idle_event_notifier);
  idle_event_observer_.Add(idle_event_notifier);

  DCHECK(detector);
  user_activity_observer_.Add(detector);

  DCHECK(power_manager_client);
  power_manager_client_observer_.Add(power_manager_client);
  power_manager_client->RequestStatusUpdate();
  power_manager_client->GetSwitchStates(
      base::BindOnce(&UserActivityManager::OnReceiveSwitchStates,
                     weak_ptr_factory_.GetWeakPtr()));
  power_manager_client->GetInactivityDelays(
      base::BindOnce(&UserActivityManager::OnReceiveInactivityDelays,
                     weak_ptr_factory_.GetWeakPtr()));

  DCHECK(session_manager);
  session_manager_observer_.Add(session_manager);

  if (chromeos::GetDeviceType() == chromeos::DeviceType::kChromebook) {
    device_type_ = UserActivityEvent::Features::CHROMEBOOK;
  } else {
    device_type_ = UserActivityEvent::Features::UNKNOWN_DEVICE;
  }
}

UserActivityManager::~UserActivityManager() = default;

void UserActivityManager::OnUserActivity(const ui::Event* /* event */) {
  MaybeLogEvent(UserActivityEvent::Event::REACTIVATE,
                UserActivityEvent::Event::USER_ACTIVITY);
}

void UserActivityManager::LidEventReceived(
    chromeos::PowerManagerClient::LidState state,
    const base::TimeTicks& /* timestamp */) {
  lid_state_ = state;
}

void UserActivityManager::PowerChanged(
    const power_manager::PowerSupplyProperties& proto) {
  if (external_power_.has_value()) {
    bool power_source_changed = (*external_power_ != proto.external_power());

    // Only log when power source changed, don't care about percentage change.
    if (power_source_changed) {
      MaybeLogEvent(UserActivityEvent::Event::REACTIVATE,
                    UserActivityEvent::Event::POWER_CHANGED);
    }
  }
  external_power_ = proto.external_power();

  if (proto.has_battery_percent()) {
    battery_percent_ = proto.battery_percent();
  }
}

void UserActivityManager::TabletModeEventReceived(
    chromeos::PowerManagerClient::TabletMode mode,
    const base::TimeTicks& /* timestamp */) {
  tablet_mode_ = mode;
}

void UserActivityManager::ScreenIdleStateChanged(
    const power_manager::ScreenIdleState& proto) {
  if (!screen_dimmed_ && proto.dimmed()) {
    screen_dim_occurred_ = true;
  }
  screen_dimmed_ = proto.dimmed();

  if (!screen_off_ && proto.off()) {
    screen_off_occurred_ = true;
  }
  screen_off_ = proto.off();
}

// We log event when SuspendImminent is received. There is a chance that a
// Suspend is cancelled, so that the corresponding SuspendDone has a short
// sleep duration. However, we ignore these cases because it's infeasible to
// to wait for a SuspendDone before deciding what to log.
void UserActivityManager::SuspendImminent(
    power_manager::SuspendImminent::Reason reason) {
  switch (reason) {
    case power_manager::SuspendImminent_Reason_IDLE:
      MaybeLogEvent(UserActivityEvent::Event::TIMEOUT,
                    UserActivityEvent::Event::IDLE_SLEEP);
      break;
    case power_manager::SuspendImminent_Reason_LID_CLOSED:
      MaybeLogEvent(UserActivityEvent::Event::OFF,
                    UserActivityEvent::Event::LID_CLOSED);
      break;
    case power_manager::SuspendImminent_Reason_OTHER:
      MaybeLogEvent(UserActivityEvent::Event::OFF,
                    UserActivityEvent::Event::MANUAL_SLEEP);
      break;
    default:
      // We don't track other suspend reason.
      break;
  }
}

void UserActivityManager::InactivityDelaysChanged(
    const power_manager::PowerManagementPolicy::Delays& delays) {
  OnReceiveInactivityDelays(delays);
}

void UserActivityManager::OnVideoActivityStarted() {
  MaybeLogEvent(UserActivityEvent::Event::REACTIVATE,
                UserActivityEvent::Event::VIDEO_ACTIVITY);
}

void UserActivityManager::OnIdleEventObserved(
    const IdleEventNotifier::ActivityData& activity_data) {
  base::TimeDelta now = boot_clock_->GetTimeSinceBoot();
  if (waiting_for_final_action_) {
    // ScreenDimImminent is received again after an earlier ScreenDimImminent
    // event without any user action/suspend in between.
    PopulatePreviousEventData(now);
  }

  idle_event_start_since_boot_ = now;

  screen_dim_occurred_ = false;
  screen_off_occurred_ = false;
  screen_lock_occurred_ = false;
  ExtractFeatures(activity_data);
  if (base::FeatureList::IsEnabled(features::kUserActivityPrediction) &&
      smart_dim_model_) {
    float inactivity_probability = -1;
    float threshold = -1;
    const bool should_dim = smart_dim_model_->ShouldDim(
        features_, &inactivity_probability, &threshold);
    DCHECK(inactivity_probability >= 0 && inactivity_probability <= 1.0)
        << inactivity_probability;
    DCHECK(threshold >= 0 && threshold <= 1.0) << threshold;

    UserActivityEvent::ModelPrediction model_prediction;
    // If previous dim was deferred, then model decision will not be applied
    // to this event.
    model_prediction.set_model_applied(!dim_deferred_);
    model_prediction.set_decision_threshold(round(threshold * 100));
    model_prediction.set_inactivity_score(round(inactivity_probability * 100));
    model_prediction_ = model_prediction;

    // Only defer the dim if the model predicts so and also if the dim was not
    // previously deferred.
    if (should_dim || dim_deferred_) {
      dim_deferred_ = false;
    } else {
      power_manager_client_->DeferScreenDim();
      dim_deferred_ = true;
    }
  }
  waiting_for_final_action_ = true;
}

void UserActivityManager::OnSessionStateChanged() {
  DCHECK(session_manager_);
  const bool was_locked = screen_is_locked_;
  screen_is_locked_ = session_manager_->IsScreenLocked();
  if (!was_locked && screen_is_locked_) {
    screen_lock_occurred_ = true;
  }
}

void UserActivityManager::OnReceiveSwitchStates(
    base::Optional<chromeos::PowerManagerClient::SwitchStates> switch_states) {
  if (switch_states.has_value()) {
    lid_state_ = switch_states->lid_state;
    tablet_mode_ = switch_states->tablet_mode;
  }
}

void UserActivityManager::OnReceiveInactivityDelays(
    base::Optional<power_manager::PowerManagementPolicy::Delays> delays) {
  if (delays.has_value()) {
    screen_dim_delay_ =
        base::TimeDelta::FromMilliseconds(delays->screen_dim_ms());
    screen_off_delay_ =
        base::TimeDelta::FromMilliseconds(delays->screen_off_ms());
  }
}

void UserActivityManager::ExtractFeatures(
    const IdleEventNotifier::ActivityData& activity_data) {
  // Set transition times for dim and screen-off.
  if (!screen_dim_delay_.is_zero()) {
    features_.set_on_to_dim_sec(std::ceil(screen_dim_delay_.InSecondsF()));
  }
  if (!screen_off_delay_.is_zero()) {
    features_.set_dim_to_screen_off_sec(
        std::ceil((screen_off_delay_ - screen_dim_delay_).InSecondsF()));
  }

  // Set time related features.
  features_.set_last_activity_day(activity_data.last_activity_day);

  features_.set_last_activity_time_sec(
      activity_data.last_activity_time_of_day.InSeconds());

  if (activity_data.last_user_activity_time_of_day) {
    features_.set_last_user_activity_time_sec(
        activity_data.last_user_activity_time_of_day.value().InSeconds());
  }

  features_.set_recent_time_active_sec(
      activity_data.recent_time_active.InSeconds());

  if (activity_data.time_since_last_key) {
    features_.set_time_since_last_key_sec(
        activity_data.time_since_last_key.value().InSeconds());
  }
  if (activity_data.time_since_last_mouse) {
    features_.set_time_since_last_mouse_sec(
        activity_data.time_since_last_mouse.value().InSeconds());
  }
  if (activity_data.time_since_last_touch) {
    features_.set_time_since_last_touch_sec(
        activity_data.time_since_last_touch.value().InSeconds());
  }

  features_.set_video_playing_time_sec(
      activity_data.video_playing_time.InSeconds());

  if (activity_data.time_since_video_ended) {
    features_.set_time_since_video_ended_sec(
        activity_data.time_since_video_ended.value().InSeconds());
  }

  features_.set_key_events_in_last_hour(activity_data.key_events_in_last_hour);
  features_.set_mouse_events_in_last_hour(
      activity_data.mouse_events_in_last_hour);
  features_.set_touch_events_in_last_hour(
      activity_data.touch_events_in_last_hour);

  // Set device mode.
  if (lid_state_ == chromeos::PowerManagerClient::LidState::CLOSED) {
    features_.set_device_mode(UserActivityEvent::Features::CLOSED_LID);
  } else if (lid_state_ == chromeos::PowerManagerClient::LidState::OPEN) {
    if (tablet_mode_ == chromeos::PowerManagerClient::TabletMode::ON) {
      features_.set_device_mode(UserActivityEvent::Features::TABLET);
    } else {
      features_.set_device_mode(UserActivityEvent::Features::CLAMSHELL);
    }
  } else {
    features_.set_device_mode(UserActivityEvent::Features::UNKNOWN_MODE);
  }

  features_.set_device_type(device_type_);

  if (battery_percent_.has_value()) {
    features_.set_battery_percent(*battery_percent_);
  }
  if (external_power_.has_value()) {
    features_.set_on_battery(
        *external_power_ == power_manager::PowerSupplyProperties::DISCONNECTED);
  }

  if (user_manager_) {
    if (user_manager_->IsEnterpriseManaged()) {
      features_.set_device_management(UserActivityEvent::Features::MANAGED);
    } else {
      features_.set_device_management(UserActivityEvent::Features::UNMANAGED);
    }
  } else {
    features_.set_device_management(
        UserActivityEvent::Features::UNKNOWN_MANAGEMENT);
  }

  features_.set_screen_dimmed_initially(screen_dimmed_);
  features_.set_screen_off_initially(screen_off_);
  features_.set_screen_locked_initially(screen_is_locked_);

  features_.set_previous_negative_actions_count(
      previous_negative_actions_count_);
  features_.set_previous_positive_actions_count(
      previous_positive_actions_count_);

  const TabProperty tab_property = UpdateOpenTabURL();

  if (tab_property.source_id == -1)
    return;

  features_.set_source_id(tab_property.source_id);

  if (!tab_property.domain.empty()) {
    features_.set_tab_domain(tab_property.domain);
  }
  if (tab_property.engagement_score != -1) {
    features_.set_engagement_score(tab_property.engagement_score);
  }
  features_.set_has_form_entry(tab_property.has_form_entry);
}

TabProperty UserActivityManager::UpdateOpenTabURL() {
  BrowserList* browser_list = BrowserList::GetInstance();
  DCHECK(browser_list);

  TabProperty property;

  // Find the active tab in the visible focused or topmost browser.
  for (auto browser_iterator = browser_list->begin_last_active();
       browser_iterator != browser_list->end_last_active();
       ++browser_iterator) {
    Browser* browser = *browser_iterator;

    if (!browser->window()->GetNativeWindow()->IsVisible())
      continue;

    // We only need the visible focused or topmost browser.
    if (browser->profile()->IsOffTheRecord())
      return property;

    const TabStripModel* const tab_strip_model = browser->tab_strip_model();
    DCHECK(tab_strip_model);

    content::WebContents* contents = tab_strip_model->GetActiveWebContents();

    if (contents) {
      ukm::SourceId source_id =
          ukm::GetSourceIdForWebContentsDocument(contents);
      if (source_id == ukm::kInvalidSourceId)
        return property;

      property.source_id = source_id;

      // Domain could be empty.
      property.domain = contents->GetLastCommittedURL().host();
      // Engagement score could be -1 if engagement service is disabled.
      property.engagement_score =
          TabMetricsLogger::GetSiteEngagementScore(contents);
      property.has_form_entry =
          contents->GetPageImportanceSignals().had_form_interaction;
    }
    return property;
  }
  return property;
}

void UserActivityManager::MaybeLogEvent(
    UserActivityEvent::Event::Type type,
    UserActivityEvent::Event::Reason reason) {
  if (!waiting_for_final_action_)
    return;
  UserActivityEvent activity_event;

  UserActivityEvent::Event* event = activity_event.mutable_event();
  event->set_type(type);
  event->set_reason(reason);
  if (idle_event_start_since_boot_) {
    event->set_log_duration_sec(
        (boot_clock_->GetTimeSinceBoot() - idle_event_start_since_boot_.value())
            .InSeconds());
  }
  event->set_screen_dim_occurred(screen_dim_occurred_);
  event->set_screen_lock_occurred(screen_lock_occurred_);
  event->set_screen_off_occurred(screen_off_occurred_);

  *activity_event.mutable_features() = features_;

  if (model_prediction_) {
    *activity_event.mutable_model_prediction() = model_prediction_.value();
  }

  // Log to metrics.
  ukm_logger_->LogActivity(activity_event);

  // If there's an earlier idle event that has not received its own event, log
  // it here too.
  if (previous_idle_event_data_) {
    if (event->has_log_duration_sec()) {
      event->set_log_duration_sec(
          event->log_duration_sec() +
          previous_idle_event_data_->dim_imminent_signal_interval.InSeconds());
    }

    *activity_event.mutable_features() = previous_idle_event_data_->features;
    *activity_event.mutable_model_prediction() =
        previous_idle_event_data_->model_prediction;
    ukm_logger_->LogActivity(activity_event);
  }

  // Update the counters for next event logging.
  if (type == UserActivityEvent::Event::REACTIVATE) {
    previous_negative_actions_count_++;
  } else {
    previous_positive_actions_count_++;
  }
  ResetAfterLogging();
}

void UserActivityManager::SetTaskRunnerForTesting(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    std::unique_ptr<BootClock> test_boot_clock) {
  boot_clock_ = std::move(test_boot_clock);
}

void UserActivityManager::PopulatePreviousEventData(
    const base::TimeDelta& now) {
  PreviousEventLoggingResult result = PreviousEventLoggingResult::kSuccess;
  if (!model_prediction_) {
    result = base::FeatureList::IsEnabled(features::kUserActivityPrediction) &&
                     smart_dim_model_
                 ? PreviousEventLoggingResult::kErrorModelPredictionMissing
                 : PreviousEventLoggingResult::kErrorModelDisabled;
    LogPowerMLPreviousEventLoggingResult(result);
  }

  if (previous_idle_event_data_) {
    result = PreviousEventLoggingResult::kErrorMultiplePreviousEvents;
    previous_idle_event_data_.reset();
    LogPowerMLPreviousEventLoggingResult(result);
  }

  if (!idle_event_start_since_boot_) {
    result = PreviousEventLoggingResult::kErrorIdleStartMissing;
    LogPowerMLPreviousEventLoggingResult(result);
  }

  if (result != PreviousEventLoggingResult::kSuccess) {
    LogPowerMLPreviousEventLoggingResult(PreviousEventLoggingResult::kError);
    return;
  }

  // Only log if none of the errors above occurred.
  LogPowerMLPreviousEventLoggingResult(result);

  previous_idle_event_data_ = std::make_unique<PreviousIdleEventData>();
  previous_idle_event_data_->dim_imminent_signal_interval =
      now - idle_event_start_since_boot_.value();

  previous_idle_event_data_->features = features_;
  previous_idle_event_data_->model_prediction = model_prediction_.value();
}

void UserActivityManager::ResetAfterLogging() {
  features_.Clear();
  idle_event_start_since_boot_ = base::nullopt;
  waiting_for_final_action_ = false;
  model_prediction_ = base::nullopt;

  previous_idle_event_data_.reset();
}

}  // namespace ml
}  // namespace power
}  // namespace chromeos
