// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/balloon_notification_ui_manager.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_service.h"
#include "base/stl_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/fullscreen.h"
#include "chrome/browser/idle.h"
#include "chrome/browser/notifications/balloon_collection.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"

BalloonNotificationUIManager::BalloonNotificationUIManager(
    PrefService* local_state)
    : NotificationUIManagerImpl(),
      NotificationPrefsManager(local_state),
      balloon_collection_(NULL) {
  position_pref_.Init(
      prefs::kDesktopNotificationPosition,
      local_state,
      base::Bind(
          &BalloonNotificationUIManager::OnDesktopNotificationPositionChanged,
          base::Unretained(this)));
}

BalloonNotificationUIManager::~BalloonNotificationUIManager() {
}

void BalloonNotificationUIManager::SetBalloonCollection(
    BalloonCollection* balloon_collection) {
  DCHECK(!balloon_collection_.get() ||
         balloon_collection_->GetActiveBalloons().size() == 0);
  DCHECK(balloon_collection);
  balloon_collection_.reset(balloon_collection);
  balloon_collection_->SetPositionPreference(
      static_cast<BalloonCollection::PositionPreference>(
          position_pref_.GetValue()));
  balloon_collection_->set_space_change_listener(this);
}

bool BalloonNotificationUIManager::DoesIdExist(const std::string& id) {
  if (NotificationUIManagerImpl::DoesIdExist(id))
    return true;
  return balloon_collection_->DoesIdExist(id);
}

bool BalloonNotificationUIManager::CancelById(const std::string& id) {
  // See if this ID hasn't been shown yet.
  if (NotificationUIManagerImpl::CancelById(id))
    return true;
  // If it has been shown, remove it from the balloon collections.
  return balloon_collection_->RemoveById(id);
}

bool BalloonNotificationUIManager::CancelAllBySourceOrigin(const GURL& source) {
  // Same pattern as CancelById, but more complicated than the above
  // because there may be multiple notifications from the same source.
  bool removed = NotificationUIManagerImpl::CancelAllBySourceOrigin(source);
  return balloon_collection_->RemoveBySourceOrigin(source) || removed;
}

bool BalloonNotificationUIManager::CancelAllByProfile(Profile* profile) {
  // Same pattern as CancelAllBySourceOrigin.
  bool removed = NotificationUIManagerImpl::CancelAllByProfile(profile);
  return balloon_collection_->RemoveByProfile(profile) || removed;
}

void BalloonNotificationUIManager::CancelAll() {
  NotificationUIManagerImpl::CancelAll();
  balloon_collection_->RemoveAll();
}

BalloonCollection* BalloonNotificationUIManager::balloon_collection() {
  return balloon_collection_.get();
}

NotificationPrefsManager* BalloonNotificationUIManager::prefs_manager() {
  return this;
}

bool BalloonNotificationUIManager::ShowNotification(
    const Notification& notification,
    Profile* profile) {
  if (!balloon_collection_->HasSpace())
    return false;
  balloon_collection_->Add(notification, profile);
  return true;
}

void BalloonNotificationUIManager::OnBalloonSpaceChanged() {
  CheckAndShowNotifications();
}

bool BalloonNotificationUIManager::UpdateNotification(
    const Notification& notification,
    Profile* profile) {
  const GURL& origin = notification.origin_url();
  const string16& replace_id = notification.replace_id();

  DCHECK(!replace_id.empty());

  const BalloonCollection::Balloons& balloons =
      balloon_collection_->GetActiveBalloons();
  for (BalloonCollection::Balloons::const_iterator iter = balloons.begin();
       iter != balloons.end(); ++iter) {
    if (profile == (*iter)->profile() &&
        origin == (*iter)->notification().origin_url() &&
        replace_id == (*iter)->notification().replace_id()) {
      (*iter)->Update(notification);
      return true;
    }
  }

  return false;
}

BalloonCollection::PositionPreference
BalloonNotificationUIManager::GetPositionPreference() const {
  LOG(INFO) << "Current position preference: " << position_pref_.GetValue();

  return static_cast<BalloonCollection::PositionPreference>(
      position_pref_.GetValue());
}

void BalloonNotificationUIManager::SetPositionPreference(
    BalloonCollection::PositionPreference preference) {
  LOG(INFO) << "Setting position preference: " << preference;
  position_pref_.SetValue(static_cast<int>(preference));
  balloon_collection_->SetPositionPreference(preference);
}

void BalloonNotificationUIManager::OnDesktopNotificationPositionChanged() {
  balloon_collection_->SetPositionPreference(
      static_cast<BalloonCollection::PositionPreference>(
          position_pref_.GetValue()));
}

// static
BalloonNotificationUIManager*
    BalloonNotificationUIManager::GetInstanceForTesting() {
  if (NotificationUIManager::DelegatesToMessageCenter()) {
    LOG(ERROR) << "Attempt to run a test that requires "
               << "BalloonNotificationUIManager while delegating to a "
               << "native MessageCenter. Test will fail. Ask dimich@";
    return NULL;
  }
  return static_cast<BalloonNotificationUIManager*>(
      g_browser_process->notification_ui_manager());
}
