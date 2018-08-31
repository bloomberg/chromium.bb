// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/drive/drive_notification_manager.h"

#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/strings/char_traits.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "components/drive/drive_notification_observer.h"
#include "components/invalidation/public/invalidation_service.h"
#include "components/invalidation/public/object_id_invalidation_map.h"
#include "google/cacheinvalidation/types.pb.h"

namespace drive {

namespace {

// The polling interval time is used when XMPP is disabled.
const int kFastPollingIntervalInSecs = 60;

// The polling interval time is used when XMPP is enabled.  Theoretically
// polling should be unnecessary if XMPP is enabled, but just in case.
const int kSlowPollingIntervalInSecs = 300;

// The sync invalidation object ID for Google Drive.
const char kDriveInvalidationObjectId[] = "CHANGELOG";

// Team drive invalidation ID's are "TD:<team_drive_id>".
constexpr char kTeamDriveChangePrefix[] = "TD:";

constexpr size_t kTeamDriveChangePrefixLength =
    base::CharTraits<char>::length(kTeamDriveChangePrefix);

}  // namespace

DriveNotificationManager::DriveNotificationManager(
    invalidation::InvalidationService* invalidation_service)
    : invalidation_service_(invalidation_service),
      push_notification_registered_(false),
      push_notification_enabled_(false),
      observers_notified_(false),
      weak_ptr_factory_(this) {
  DCHECK(invalidation_service_);
  RegisterDriveNotifications();
  RestartPollingTimer();
}

DriveNotificationManager::~DriveNotificationManager() = default;

void DriveNotificationManager::Shutdown() {
  // Unregister for Drive notifications.
  if (!invalidation_service_ || !push_notification_registered_)
    return;

  // We unregister the handler without updating unregistering our IDs on
  // purpose.  See the class comment on the InvalidationService interface for
  // more information.
  invalidation_service_->UnregisterInvalidationHandler(this);
  invalidation_service_ = nullptr;
}

void DriveNotificationManager::OnInvalidatorStateChange(
    syncer::InvalidatorState state) {
  push_notification_enabled_ = (state == syncer::INVALIDATIONS_ENABLED);
  if (push_notification_enabled_) {
    DVLOG(1) << "XMPP Notifications enabled";
  } else {
    DVLOG(1) << "XMPP Notifications disabled (state=" << state << ")";
  }
  for (auto& observer : observers_)
    observer.OnPushNotificationEnabled(push_notification_enabled_);
}

void DriveNotificationManager::OnIncomingInvalidation(
    const syncer::ObjectIdInvalidationMap& invalidation_map) {
  DVLOG(2) << "XMPP Drive Notification Received";
  syncer::ObjectIdSet ids = invalidation_map.GetObjectIds();

  // TODO(slangley): Look at batching together notifications, it appears that
  // we get can get many consecutive invalidations for the same ObjectId due to
  // how drive stores changes. We should collate a few seconds worth of changes
  // to filter out duplicates.
  std::set<std::string> change_ids;

  for (const auto& id : ids) {
    if (id.name() == kDriveInvalidationObjectId) {
      // Empty string indicates default change list.
      change_ids.insert("");
    } else if (base::StartsWith(id.name(), kTeamDriveChangePrefix,
                                base::CompareCase::SENSITIVE)) {
      change_ids.insert(id.name().substr(kTeamDriveChangePrefixLength));
    } else {
      NOTREACHED() << "Unexpected ID " << id.name();
    }
  }

  // This effectively disables 'local acks'.  It tells the invalidations system
  // to not bother saving invalidations across restarts for us.
  // See crbug.com/320878.
  invalidation_map.AcknowledgeAll();
  NotifyObserversToUpdate(NOTIFICATION_XMPP, std::move(change_ids));
}

std::string DriveNotificationManager::GetOwnerName() const { return "Drive"; }

void DriveNotificationManager::AddObserver(
    DriveNotificationObserver* observer) {
  observers_.AddObserver(observer);
}

void DriveNotificationManager::RemoveObserver(
    DriveNotificationObserver* observer) {
  observers_.RemoveObserver(observer);
}

void DriveNotificationManager::UpdateTeamDriveIds(
    const std::set<std::string>& added_team_drive_ids,
    const std::set<std::string>& removed_team_drive_ids) {
  // We only want to update the invalidation service if we actually change the
  // set of team drive id's we're currently registered for.
  bool set_changed = false;

  for (const auto& added : added_team_drive_ids) {
    if (team_drive_ids_.insert(added).second) {
      set_changed = true;
    }
  }

  for (const auto& removed : removed_team_drive_ids) {
    if (team_drive_ids_.erase(removed)) {
      set_changed = true;
    }
  }

  if (set_changed) {
    UpdateRegisteredDriveNotifications();
  }
}

void DriveNotificationManager::RestartPollingTimer() {
  const int interval_secs = (push_notification_enabled_ ?
                             kSlowPollingIntervalInSecs :
                             kFastPollingIntervalInSecs);
  polling_timer_.Stop();
  polling_timer_.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(interval_secs),
      base::BindOnce(&DriveNotificationManager::NotifyObserversToUpdate,
                     weak_ptr_factory_.GetWeakPtr(), NOTIFICATION_POLLING,
                     std::set<std::string>()));
}

void DriveNotificationManager::NotifyObserversToUpdate(
    NotificationSource source,
    const std::set<std::string> ids) {
  DVLOG(1) << "Notifying observers: " << NotificationSourceToString(source);

  if (source == NOTIFICATION_XMPP) {
    for (auto& observer : observers_)
      observer.OnNotificationReceived(ids);
  } else {
    for (auto& observer : observers_)
      observer.OnNotificationTimerFired();
  }
  if (!observers_notified_) {
    UMA_HISTOGRAM_BOOLEAN("Drive.PushNotificationInitiallyEnabled",
                          push_notification_enabled_);
  }
  observers_notified_ = true;

  // Note that polling_timer_ is not a repeating timer. Restarting manually
  // here is better as XMPP may be received right before the polling timer is
  // fired (i.e. we don't notify observers twice in a row).
  RestartPollingTimer();
}

void DriveNotificationManager::RegisterDriveNotifications() {
  DCHECK(!push_notification_enabled_);

  if (!invalidation_service_)
    return;

  invalidation_service_->RegisterInvalidationHandler(this);

  UpdateRegisteredDriveNotifications();

  UMA_HISTOGRAM_BOOLEAN("Drive.PushNotificationRegistered",
                        push_notification_registered_);
}

void DriveNotificationManager::UpdateRegisteredDriveNotifications() {
  if (!invalidation_service_)
    return;

  syncer::ObjectIdSet ids;
  ids.insert(
      invalidation::ObjectId(ipc::invalidation::ObjectSource::COSMO_CHANGELOG,
                             kDriveInvalidationObjectId));

  for (const auto& team_drive_id : team_drive_ids_) {
    ids.insert(invalidation::ObjectId(
        ipc::invalidation::ObjectSource::COSMO_CHANGELOG,
        base::StrCat({kTeamDriveChangePrefix, team_drive_id})));
  }

  CHECK(invalidation_service_->UpdateRegisteredInvalidationIds(this, ids));
  push_notification_registered_ = true;
  OnInvalidatorStateChange(invalidation_service_->GetInvalidatorState());
}

// static
std::string DriveNotificationManager::NotificationSourceToString(
    NotificationSource source) {
  switch (source) {
    case NOTIFICATION_XMPP:
      return "NOTIFICATION_XMPP";
    case NOTIFICATION_POLLING:
      return "NOTIFICATION_POLLING";
  }

  NOTREACHED();
  return "";
}

}  // namespace drive
