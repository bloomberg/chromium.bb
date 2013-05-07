// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/drive_notification_manager.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/google_apis/drive_notification_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "google/cacheinvalidation/types.pb.h"

namespace google_apis {

namespace {

// The polling interval time is used when XMPP is disabled.
const int kFastPollingIntervalInSecs = 60;

// The polling interval time is used when XMPP is enabled.  Theoretically
// polling should be unnecessary if XMPP is enabled, but just in case.
const int kSlowPollingIntervalInSecs = 300;

// The sync invalidation object ID for Google Drive.
const char kDriveInvalidationObjectId[] = "CHANGELOG";

}  // namespace

DriveNotificationManager::DriveNotificationManager(Profile* profile)
    : profile_(profile),
      push_notification_registered_(false),
      push_notification_enabled_(false),
      observers_notified_(false),
      polling_timer_(true /* retain_user_task */, false /* is_repeating */),
      weak_ptr_factory_(this) {
  RegisterDriveNotifications();
  RestartPollingTimer();
}

DriveNotificationManager::~DriveNotificationManager() {}

void DriveNotificationManager::Shutdown() {
  // Unregister for Drive notifications.
  ProfileSyncService* profile_sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  if (!profile_sync_service || !push_notification_registered_) {
    return;
  }

  profile_sync_service->UpdateRegisteredInvalidationIds(
      this, syncer::ObjectIdSet());
  profile_sync_service->UnregisterInvalidationHandler(this);
}

void DriveNotificationManager::OnInvalidatorStateChange(
    syncer::InvalidatorState state) {
  push_notification_enabled_ = (state == syncer::INVALIDATIONS_ENABLED);
  if (push_notification_enabled_) {
    DVLOG(1) << "XMPP Notifications enabled";
  } else {
    DVLOG(1) << "XMPP Notifications disabled (state=" << state << ")";
  }
  FOR_EACH_OBSERVER(DriveNotificationObserver, observers_,
                    OnPushNotificationEnabled(push_notification_enabled_));
}

void DriveNotificationManager::OnIncomingInvalidation(
    const syncer::ObjectIdInvalidationMap& invalidation_map) {
  DVLOG(2) << "XMPP Drive Notification Received";
  DCHECK_EQ(1U, invalidation_map.size());
  const invalidation::ObjectId object_id(
      ipc::invalidation::ObjectSource::COSMO_CHANGELOG,
      kDriveInvalidationObjectId);
  DCHECK_EQ(1U, invalidation_map.count(object_id));

  // TODO(dcheng): Only acknowledge the invalidation once the fetch has
  // completed. http://crbug.com/156843
  ProfileSyncService* profile_sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  profile_sync_service->AcknowledgeInvalidation(
      invalidation_map.begin()->first,
      invalidation_map.begin()->second.ack_handle);

  NotifyObserversToUpdate(NOTIFICATION_XMPP);
}

void DriveNotificationManager::AddObserver(
    DriveNotificationObserver* observer) {
  observers_.AddObserver(observer);
}

void DriveNotificationManager::RemoveObserver(
    DriveNotificationObserver* observer) {
  observers_.RemoveObserver(observer);
}

void DriveNotificationManager::RestartPollingTimer() {
  const int interval_secs = (push_notification_enabled_ ?
                             kSlowPollingIntervalInSecs :
                             kFastPollingIntervalInSecs);
  polling_timer_.Stop();
  polling_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromSeconds(interval_secs),
      base::Bind(&DriveNotificationManager::NotifyObserversToUpdate,
                 weak_ptr_factory_.GetWeakPtr(),
                 NOTIFICATION_POLLING));
}

void DriveNotificationManager::NotifyObserversToUpdate(
    NotificationSource source) {
  DVLOG(1) << "Notifying observers: " << NotificationSourceToString(source);
  FOR_EACH_OBSERVER(DriveNotificationObserver, observers_,
                    OnNotificationReceived());
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

  ProfileSyncService* profile_sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  if (!profile_sync_service)
    return;

  profile_sync_service->RegisterInvalidationHandler(this);
  syncer::ObjectIdSet ids;
  ids.insert(invalidation::ObjectId(
      ipc::invalidation::ObjectSource::COSMO_CHANGELOG,
      kDriveInvalidationObjectId));
  profile_sync_service->UpdateRegisteredInvalidationIds(this, ids);
  push_notification_registered_ = true;
  OnInvalidatorStateChange(profile_sync_service->GetInvalidatorState());

  UMA_HISTOGRAM_BOOLEAN("Drive.PushNotificationRegistered",
                        push_notification_registered_);
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

}  // namespace google_apis
