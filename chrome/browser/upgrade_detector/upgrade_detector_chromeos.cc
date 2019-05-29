// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upgrade_detector/upgrade_detector_chromeos.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/update_engine_client.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

using chromeos::DBusThreadManager;
using chromeos::UpdateEngineClient;

namespace {

// How long to wait (each cycle) before checking which severity level we should
// be at. Once we reach the highest severity, the timer will stop.
constexpr base::TimeDelta kNotifyCycleDelta = base::TimeDelta::FromMinutes(20);

// The default amount of time it takes for the detector's annoyance level
// (upgrade_notification_stage()) to reach UPGRADE_ANNOYANCE_HIGH once an
// upgrade is detected.
constexpr base::TimeDelta kDefaultHighThreshold = base::TimeDelta::FromDays(4);

// The default amount of time between the detector's annoyance level change
// from UPGRADE_ANNOYANCE_ELEVATED to UPGRADE_ANNOYANCE_HIGH in ms.
constexpr int kDefaultHeadsUpPeriodMs = 24 * 60 * 60 * 1000;  // 1 day.

constexpr base::TimeDelta kDefaultHeadsUpPeriod =
    base::TimeDelta::FromMilliseconds(kDefaultHeadsUpPeriodMs);

// The reason of the rollback used in the UpgradeDetector.RollbackReason
// histogram.
enum class RollbackReason {
  kToMoreStableChannel = 0,
  kEnterpriseRollback = 1,
  kMaxValue = kEnterpriseRollback,
};

class ChannelsRequester {
 public:
  typedef base::OnceCallback<void(std::string, std::string)>
      OnChannelsReceivedCallback;

  static void Begin(OnChannelsReceivedCallback callback) {
    ChannelsRequester* instance = new ChannelsRequester(std::move(callback));
    UpdateEngineClient* client =
        DBusThreadManager::Get()->GetUpdateEngineClient();
    // base::Unretained is safe because this instance keeps itself alive until
    // both callbacks have run.
    // TODO: use BindOnce here; see https://crbug.com/825993.
    client->GetChannel(true /* get_current_channel */,
                       base::Bind(&ChannelsRequester::SetCurrentChannel,
                                  base::Unretained(instance)));
    client->GetChannel(false /* get_current_channel */,
                       base::Bind(&ChannelsRequester::SetTargetChannel,
                                  base::Unretained(instance)));
  }

 private:
  explicit ChannelsRequester(OnChannelsReceivedCallback callback)
      : callback_(std::move(callback)) {}

  ~ChannelsRequester() = default;

  void SetCurrentChannel(const std::string& current_channel) {
    DCHECK(!current_channel.empty());
    current_channel_ = current_channel;
    TriggerCallbackAndDieIfReady();
  }

  void SetTargetChannel(const std::string& target_channel) {
    DCHECK(!target_channel.empty());
    target_channel_ = target_channel;
    TriggerCallbackAndDieIfReady();
  }

  void TriggerCallbackAndDieIfReady() {
    if (current_channel_.empty() || target_channel_.empty())
      return;
    if (!callback_.is_null()) {
      std::move(callback_).Run(std::move(current_channel_),
                               std::move(target_channel_));
    }
    delete this;
  }

  OnChannelsReceivedCallback callback_;
  std::string current_channel_;
  std::string target_channel_;

  DISALLOW_COPY_AND_ASSIGN(ChannelsRequester);
};

}  // namespace

UpgradeDetectorChromeos::UpgradeDetectorChromeos(
    const base::Clock* clock,
    const base::TickClock* tick_clock)
    : UpgradeDetector(clock, tick_clock),
      upgrade_notification_timer_(tick_clock),
      initialized_(false),
      weak_factory_(this) {
  CalculateThresholds();
  // Not all tests provide a PrefService for local_state().
  PrefService* local_state = g_browser_process->local_state();
  if (local_state) {
    pref_change_registrar_.Init(local_state);
    // base::Unretained is safe here because |this| outlives the registrar.
    pref_change_registrar_.Add(
        prefs::kRelaunchHeadsUpPeriod,
        base::BindRepeating(
            &UpgradeDetectorChromeos::OnRelaunchHeadsUpPeriodPrefChanged,
            base::Unretained(this)));
  }
}

UpgradeDetectorChromeos::~UpgradeDetectorChromeos() {}

// static
void UpgradeDetectorChromeos::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kRelaunchHeadsUpPeriod,
                                kDefaultHeadsUpPeriodMs);
}

void UpgradeDetectorChromeos::Init() {
  DBusThreadManager::Get()->GetUpdateEngineClient()->AddObserver(this);
  initialized_ = true;
}

void UpgradeDetectorChromeos::Shutdown() {
  // Init() may not be called from tests.
  if (!initialized_)
    return;
  DBusThreadManager::Get()->GetUpdateEngineClient()->RemoveObserver(this);
  // Discard an outstanding request to a ChannelsRequester.
  weak_factory_.InvalidateWeakPtrs();
  upgrade_notification_timer_.Stop();
  initialized_ = false;
}

base::TimeDelta UpgradeDetectorChromeos::GetHighAnnoyanceLevelDelta() {
  return high_threshold_ - elevated_threshold_;
}

base::Time UpgradeDetectorChromeos::GetHighAnnoyanceDeadline() {
  const base::Time detected_time = upgrade_detected_time();
  if (detected_time.is_null())
    return detected_time;
  return detected_time + high_threshold_;
}

// static
base::TimeDelta UpgradeDetectorChromeos::GetRelaunchHeadsUpPeriod() {
  // Not all tests provide a PrefService for local_state().
  auto* local_state = g_browser_process->local_state();
  if (!local_state)
    return base::TimeDelta();
  const auto* preference =
      local_state->FindPreference(prefs::kRelaunchHeadsUpPeriod);
  const int value = preference->GetValue()->GetInt();
  // Enforce the preference's documented minimum value.
  static constexpr base::TimeDelta kMinValue = base::TimeDelta::FromHours(1);
  if (preference->IsDefaultValue() || value < kMinValue.InMilliseconds())
    return base::TimeDelta();
  return base::TimeDelta::FromMilliseconds(value);
}

void UpgradeDetectorChromeos::CalculateThresholds() {
  base::TimeDelta notification_period = GetRelaunchNotificationPeriod();
  high_threshold_ = notification_period.is_zero() ? kDefaultHighThreshold
                                                  : notification_period;
  base::TimeDelta heads_up_period = GetRelaunchHeadsUpPeriod();
  if (heads_up_period.is_zero())
    heads_up_period = kDefaultHeadsUpPeriod;
  elevated_threshold_ =
      high_threshold_ - std::min(heads_up_period, high_threshold_);
}

void UpgradeDetectorChromeos::OnRelaunchHeadsUpPeriodPrefChanged() {
  // Run OnThresholdPrefChanged using SequencedTaskRunner to avoid double
  // NotifyUpgrade calls in case two polices are changed at one moment.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&UpgradeDetectorChromeos::OnThresholdPrefChanged,
                     weak_factory_.GetWeakPtr()));
}

void UpgradeDetectorChromeos::OnRelaunchNotificationPeriodPrefChanged() {
  // Run OnThresholdPrefChanged using SequencedTaskRunner to avoid double
  // NotifyUpgrade calls in case two polices are changed at one moment.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&UpgradeDetectorChromeos::OnThresholdPrefChanged,
                     weak_factory_.GetWeakPtr()));
}

void UpgradeDetectorChromeos::UpdateStatusChanged(
    const UpdateEngineClient::Status& status) {
  if (status.status == UpdateEngineClient::UPDATE_STATUS_UPDATED_NEED_REBOOT) {
    if (upgrade_detected_time().is_null())
      set_upgrade_detected_time(clock()->Now());

    if (status.is_rollback) {
      // Powerwash will be required, determine what kind of notification to show
      // based on the channel.
      ChannelsRequester::Begin(
          base::BindOnce(&UpgradeDetectorChromeos::OnChannelsReceived,
                         weak_factory_.GetWeakPtr()));
    } else {
      // Not going to an earlier version, no powerwash or rollback message is
      // required.
      set_is_rollback(false);
      set_is_factory_reset_required(false);
      NotifyOnUpgrade();
    }
  } else if (status.status ==
             UpdateEngineClient::UPDATE_STATUS_NEED_PERMISSION_TO_UPDATE) {
    // Update engine broadcasts this state only when update is available but
    // downloading over cellular connection requires user's agreement.
    NotifyUpdateOverCellularAvailable();
  }
}

void UpgradeDetectorChromeos::OnUpdateOverCellularOneTimePermissionGranted() {
  NotifyUpdateOverCellularOneTimePermissionGranted();
}

void UpgradeDetectorChromeos::OnThresholdPrefChanged() {
  // Check the current stage and potentially notify observers now if a change to
  // the observed policies results in changes to the thresholds.
  const base::TimeDelta old_elevated_threshold = elevated_threshold_;
  const base::TimeDelta old_high_threshold = high_threshold_;
  CalculateThresholds();
  if (!upgrade_detected_time().is_null() &&
      (elevated_threshold_ != old_elevated_threshold ||
       high_threshold_ != old_high_threshold)) {
    NotifyOnUpgrade();
  }
}

void UpgradeDetectorChromeos::NotifyOnUpgrade() {
  const base::TimeDelta delta = clock()->Now() - upgrade_detected_time();
  // The delay from now until the next highest notification stage is reached, or
  // zero if the highest notification stage has been reached.
  base::TimeDelta next_delay;

  const auto last_stage = upgrade_notification_stage();
  // These if statements must be sorted (highest interval first).
  if (delta >= high_threshold_) {
    set_upgrade_notification_stage(UPGRADE_ANNOYANCE_HIGH);
  } else if (delta >= elevated_threshold_) {
    set_upgrade_notification_stage(UPGRADE_ANNOYANCE_ELEVATED);
    next_delay = high_threshold_ - delta;
  } else {
    set_upgrade_notification_stage(UPGRADE_ANNOYANCE_NONE);
    next_delay = elevated_threshold_ - delta;
  }
  const auto new_stage = upgrade_notification_stage();

  if (!next_delay.is_zero()) {
    // Schedule the next wakeup in 20 minutes or when the next change to the
    // notification stage should take place.
    upgrade_notification_timer_.Start(
        FROM_HERE, std::min(next_delay, kNotifyCycleDelta), this,
        &UpgradeDetectorChromeos::NotifyOnUpgrade);
  } else if (upgrade_notification_timer_.IsRunning()) {
    // Explicitly stop the timer in case this call is due to a
    // RelaunchNotificationPeriod change that brought the instance up to the
    // "high" annoyance level.
    upgrade_notification_timer_.Stop();
  }

  // Issue a notification if the stage is above "none" or if it's dropped down
  // to "none" from something higher.
  if (new_stage != UPGRADE_ANNOYANCE_NONE ||
      last_stage != UPGRADE_ANNOYANCE_NONE) {
    NotifyUpgrade();
  }
}

void UpgradeDetectorChromeos::OnChannelsReceived(std::string current_channel,
                                                 std::string target_channel) {
  bool to_more_stable_channel = UpdateEngineClient::IsTargetChannelMoreStable(
      current_channel, target_channel);
  // As current update engine status is UPDATE_STATUS_UPDATED_NEED_REBOOT,
  // if target channel is more stable than current channel, powerwash
  // will be performed after reboot.
  set_is_factory_reset_required(to_more_stable_channel);
  // If we are doing a channel switch, we're currently showing the channel
  // switch message instead of the rollback message (even if the channel switch
  // was initiated by the admin).
  // TODO(crbug.com/864672): Fix this by getting is_rollback from update engine.
  set_is_rollback(!to_more_stable_channel);

  UMA_HISTOGRAM_ENUMERATION("UpgradeDetector.RollbackReason",
                            to_more_stable_channel
                                ? RollbackReason::kToMoreStableChannel
                                : RollbackReason::kEnterpriseRollback);
  LOG(WARNING) << "Device is rolling back, will require powerwash. Reason: "
               << to_more_stable_channel
               << ", current_channel: " << current_channel
               << ", target_channel: " << target_channel;

  // ChromeOS shows upgrade arrow once the upgrade becomes available.
  NotifyOnUpgrade();
}

// static
UpgradeDetectorChromeos* UpgradeDetectorChromeos::GetInstance() {
  static base::NoDestructor<UpgradeDetectorChromeos> instance(
      base::DefaultClock::GetInstance(), base::DefaultTickClock::GetInstance());
  return instance.get();
}

// static
UpgradeDetector* UpgradeDetector::GetInstance() {
  return UpgradeDetectorChromeos::GetInstance();
}

// static
base::TimeDelta UpgradeDetector::GetDefaultHighAnnoyanceThreshold() {
  return kDefaultHighThreshold;
}
