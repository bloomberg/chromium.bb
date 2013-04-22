// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/drive_notification_manager.h"

#include "chrome/browser/google_apis/drive_notification_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "google/cacheinvalidation/types.pb.h"

namespace google_apis {

// The sync invalidation object ID for Google Drive.
const char kDriveInvalidationObjectId[] = "CHANGELOG";

// TODO(calvinlo): Constants for polling to go here.

DriveNotificationManager::DriveNotificationManager(Profile* profile)
    : profile_(profile),
      push_notification_registered_(false),
      push_notification_enabled_(false) {
  // TODO(calvinlo): Initialize member variables for polling here.
  RegisterDriveNotifications();
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
  SetPushNotificationEnabled(state);
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
  CHECK(profile_sync_service);
  profile_sync_service->AcknowledgeInvalidation(
      invalidation_map.begin()->first,
      invalidation_map.begin()->second.ack_handle);

  NotifyObserversToUpdate();
}

void DriveNotificationManager::AddObserver(
    DriveNotificationObserver* observer) {
  observers_.AddObserver(observer);
}

void DriveNotificationManager::RemoveObserver(
    DriveNotificationObserver* observer) {
  observers_.RemoveObserver(observer);
}

void DriveNotificationManager::NotifyObserversToUpdate() {
  FOR_EACH_OBSERVER(DriveNotificationObserver, observers_, CheckForUpdates());
}

// Register for Google Drive invalidation notifications through XMPP.
void DriveNotificationManager::RegisterDriveNotifications() {
  // Push notification registration might have already occurred if called from
  // a different extension.
  if (!IsDriveNotificationSupported() || push_notification_registered_)
    return;

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
  SetPushNotificationEnabled(profile_sync_service->GetInvalidatorState());
}

// TODO(calvinlo): Remove when all patches for http://crbug.com/173339 done.
bool DriveNotificationManager::IsDriveNotificationSupported() {
  // TODO(calvinlo): A invalidation ID can only be registered to one handler.
  // Therefore ChromeOS and SyncFS cannot both use XMPP notifications until
  // (http://crbug.com/173339) is completed.
  // For now, disable XMPP notifications for SyncFC on ChromeOS to guarantee
  // that ChromeOS's file manager can register itself to the invalidationID.

#if defined(OS_CHROMEOS)
  return false;
#else
  return true;
#endif
}

void DriveNotificationManager::SetPushNotificationEnabled(
    syncer::InvalidatorState state) {
  DVLOG(1) << "SetPushNotificationEnabled() with state=" << state;
  push_notification_enabled_ = (state == syncer::INVALIDATIONS_ENABLED);
  if (!push_notification_enabled_)
    return;

  // Push notifications are enabled so reset polling timer.
  //UpdatePollingDelay(kPollingDelaySecondsWithNotification);
}

}  // namespace google_apis
