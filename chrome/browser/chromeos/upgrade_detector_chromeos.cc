// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/upgrade_detector_chromeos.h"

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/update_engine_client.h"

using chromeos::DBusThreadManager;
using chromeos::UpdateEngineClient;

namespace {

// How long to wait (each cycle) before checking which severity level we should
// be at. Once we reach the highest severity, the timer will stop.
constexpr base::TimeDelta kNotifyCycleDelta = base::TimeDelta::FromMinutes(20);

constexpr int kHighDaysThreshold = 4;
constexpr int kElevatedDaysThreshold = 2;
constexpr int kLowDaysThreshold = 0;

}  // namespace

class UpgradeDetectorChromeos::ChannelsRequester {
 public:
  typedef base::Callback<void(const std::string&, const std::string&)>
      OnChannelsReceivedCallback;

  ChannelsRequester() : weak_factory_(this) {}

  void RequestChannels(const OnChannelsReceivedCallback& callback) {
    UpdateEngineClient* client =
        DBusThreadManager::Get()->GetUpdateEngineClient();
    callback_ = callback;
    client->GetChannel(true /* get_current_channel */,
                       base::Bind(&ChannelsRequester::SetCurrentChannel,
                                  weak_factory_.GetWeakPtr()));
    client->GetChannel(false /* get_current_channel */,
                       base::Bind(&ChannelsRequester::SetTargetChannel,
                                  weak_factory_.GetWeakPtr()));
  }

 private:
  void SetCurrentChannel(const std::string& current_channel) {
    DCHECK(!current_channel.empty());
    current_channel_ = current_channel;
    TriggerCallbackIfReady();
  }

  void SetTargetChannel(const std::string& target_channel) {
    DCHECK(!target_channel.empty());
    target_channel_ = target_channel;
    TriggerCallbackIfReady();
  }

  void TriggerCallbackIfReady() {
    if (current_channel_.empty() || target_channel_.empty())
      return;
    if (!callback_.is_null())
      callback_.Run(current_channel_, target_channel_);
  }

  std::string current_channel_;
  std::string target_channel_;

  OnChannelsReceivedCallback callback_;

  base::WeakPtrFactory<ChannelsRequester> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChannelsRequester);
};

UpgradeDetectorChromeos::UpgradeDetectorChromeos()
    : initialized_(false), weak_factory_(this) {
}

UpgradeDetectorChromeos::~UpgradeDetectorChromeos() {
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
}

base::TimeDelta UpgradeDetectorChromeos::GetHighAnnoyanceLevelDelta() {
  return base::TimeDelta::FromDays(kHighDaysThreshold - kElevatedDaysThreshold);
}

base::TimeTicks UpgradeDetectorChromeos::GetHighAnnoyanceDeadline() {
  const base::TimeTicks detected_time = upgrade_detected_time();
  if (detected_time.is_null())
    return detected_time;
  return detected_time + base::TimeDelta::FromDays(kHighDaysThreshold);
}

void UpgradeDetectorChromeos::UpdateStatusChanged(
    const UpdateEngineClient::Status& status) {
  if (status.status == UpdateEngineClient::UPDATE_STATUS_UPDATED_NEED_REBOOT) {
    set_upgrade_detected_time(base::TimeTicks::Now());

    channels_requester_.reset(new UpgradeDetectorChromeos::ChannelsRequester());
    channels_requester_->RequestChannels(
        base::Bind(&UpgradeDetectorChromeos::OnChannelsReceived,
                   weak_factory_.GetWeakPtr()));
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

void UpgradeDetectorChromeos::NotifyOnUpgrade() {
  base::TimeDelta delta = base::TimeTicks::Now() - upgrade_detected_time();
  int64_t days_passed = delta.InDays();

  // These if statements must be sorted (highest interval first).
  if (days_passed >= kHighDaysThreshold) {
    set_upgrade_notification_stage(UPGRADE_ANNOYANCE_HIGH);

    // We can't get any higher, baby.
    upgrade_notification_timer_.Stop();
  } else if (days_passed >= kElevatedDaysThreshold) {
    set_upgrade_notification_stage(UPGRADE_ANNOYANCE_ELEVATED);
  } else if (days_passed >= kLowDaysThreshold) {
    set_upgrade_notification_stage(UPGRADE_ANNOYANCE_LOW);
  } else {
    return;  // Not ready to recommend upgrade.
  }

  NotifyUpgrade();
}

void UpgradeDetectorChromeos::OnChannelsReceived(
    const std::string& current_channel,
    const std::string& target_channel) {
  // As current update engine status is UPDATE_STATUS_UPDATED_NEED_REBOOT
  // and target channel is more stable than current channel, powerwash
  // will be performed after reboot.
  set_is_factory_reset_required(UpdateEngineClient::IsTargetChannelMoreStable(
      current_channel, target_channel));

  // ChromeOS shows upgrade arrow once the upgrade becomes available.
  NotifyOnUpgrade();

  // Setup timer to to move along the upgrade advisory system.
  upgrade_notification_timer_.Start(FROM_HERE, kNotifyCycleDelta, this,
                                    &UpgradeDetectorChromeos::NotifyOnUpgrade);
}

// static
UpgradeDetectorChromeos* UpgradeDetectorChromeos::GetInstance() {
  return base::Singleton<UpgradeDetectorChromeos>::get();
}

// static
UpgradeDetector* UpgradeDetector::GetInstance() {
  return UpgradeDetectorChromeos::GetInstance();
}

// static
base::TimeDelta UpgradeDetector::GetDefaultHighAnnoyanceThreshold() {
  return base::TimeDelta::FromDays(kHighDaysThreshold);
}
