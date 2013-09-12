// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/balloon_notification_ui_manager.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_service.h"
#include "base/stl_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/fullscreen.h"
#include "chrome/browser/idle.h"
#include "chrome/browser/notifications/balloon_collection.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"

// A class which represents a notification waiting to be shown.
class QueuedNotification {
 public:
  QueuedNotification(const Notification& notification, Profile* profile)
      : notification_(notification),
        profile_(profile) {
  }

  const Notification& notification() const { return notification_; }
  Profile* profile() const { return profile_; }

  void Replace(const Notification& new_notification) {
    notification_ = new_notification;
  }

 private:
  // The notification to be shown.
  Notification notification_;

  // Non owned pointer to the user's profile.
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(QueuedNotification);
};

BalloonNotificationUIManager::BalloonNotificationUIManager(
    PrefService* local_state)
    : NotificationPrefsManager(local_state),
      // Passes NULL to blockers since |message_center| is not used from balloon
      // notifications.
      screen_lock_blocker_(NULL),
      fullscreen_blocker_(NULL),
      system_observer_(this) {
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

void BalloonNotificationUIManager::Add(const Notification& notification,
                                       Profile* profile) {
  if (Update(notification, profile)) {
    return;
  }

  VLOG(1) << "Added notification. URL: "
          << notification.content_url().spec();
  show_queue_.push_back(linked_ptr<QueuedNotification>(
      new QueuedNotification(notification, profile)));
  CheckAndShowNotifications();
}

bool BalloonNotificationUIManager::Update(const Notification& notification,
                                          Profile* profile) {
  const GURL& origin = notification.origin_url();
  const string16& replace_id = notification.replace_id();

  if (replace_id.empty())
    return false;

  // First check the queue of pending notifications for replacement.
  // Then check the list of notifications already being shown.
  for (NotificationDeque::const_iterator iter = show_queue_.begin();
       iter != show_queue_.end(); ++iter) {
    if (profile == (*iter)->profile() &&
        origin == (*iter)->notification().origin_url() &&
        replace_id == (*iter)->notification().replace_id()) {
      (*iter)->Replace(notification);
      return true;
    }
  }

  return UpdateNotification(notification, profile);
}

const Notification* BalloonNotificationUIManager::FindById(
    const std::string& id) const {
  for (NotificationDeque::const_iterator iter = show_queue_.begin();
       iter != show_queue_.end(); ++iter) {
    if ((*iter)->notification().notification_id() == id) {
      return &((*iter)->notification());
    }
  }
  return balloon_collection_->FindById(id);
}

bool BalloonNotificationUIManager::CancelById(const std::string& id) {
  // See if this ID hasn't been shown yet.
  for (NotificationDeque::iterator iter = show_queue_.begin();
       iter != show_queue_.end(); ++iter) {
    if ((*iter)->notification().notification_id() == id) {
      show_queue_.erase(iter);
      return true;
    }
  }
  // If it has been shown, remove it from the balloon collections.
  return balloon_collection_->RemoveById(id);
}

std::set<std::string>
BalloonNotificationUIManager::GetAllIdsByProfileAndSourceOrigin(
    Profile* profile,
    const GURL& source) {
  std::set<std::string> notification_ids;
  for (NotificationDeque::iterator iter = show_queue_.begin();
       iter != show_queue_.end(); iter++) {
    if ((*iter)->notification().origin_url() == source &&
        profile->IsSameProfile((*iter)->profile())) {
      notification_ids.insert((*iter)->notification().notification_id());
    }
  }

  const BalloonCollection::Balloons& balloons =
      balloon_collection_->GetActiveBalloons();
  for (BalloonCollection::Balloons::const_iterator iter = balloons.begin();
       iter != balloons.end(); ++iter) {
    if (profile->IsSameProfile((*iter)->profile()) &&
        source == (*iter)->notification().origin_url()) {
      notification_ids.insert((*iter)->notification().notification_id());
    }
  }
  return notification_ids;
}

bool BalloonNotificationUIManager::CancelAllBySourceOrigin(const GURL& source) {
  // Same pattern as CancelById, but more complicated than the above
  // because there may be multiple notifications from the same source.
  bool removed = false;
  for (NotificationDeque::iterator loopiter = show_queue_.begin();
       loopiter != show_queue_.end(); ) {
    if ((*loopiter)->notification().origin_url() != source) {
      ++loopiter;
      continue;
    }

    loopiter = show_queue_.erase(loopiter);
    removed = true;
  }
  return balloon_collection_->RemoveBySourceOrigin(source) || removed;
}

bool BalloonNotificationUIManager::CancelAllByProfile(Profile* profile) {
  // Same pattern as CancelAllBySourceOrigin.
  bool removed = false;
  for (NotificationDeque::iterator loopiter = show_queue_.begin();
       loopiter != show_queue_.end(); ) {
    if ((*loopiter)->profile() != profile) {
      ++loopiter;
      continue;
    }

    loopiter = show_queue_.erase(loopiter);
    removed = true;
  }
  return balloon_collection_->RemoveByProfile(profile) || removed;
}

void BalloonNotificationUIManager::CancelAll() {
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

void BalloonNotificationUIManager::OnBlockingStateChanged() {
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

void BalloonNotificationUIManager::CheckAndShowNotifications() {
  screen_lock_blocker_.CheckState();
  fullscreen_blocker_.CheckState();
  if (screen_lock_blocker_.is_locked() ||
      fullscreen_blocker_.is_fullscreen_mode()) {
    return;
  }
  ShowNotifications();
}

void BalloonNotificationUIManager::OnDesktopNotificationPositionChanged() {
  balloon_collection_->SetPositionPreference(
      static_cast<BalloonCollection::PositionPreference>(
          position_pref_.GetValue()));
}

void BalloonNotificationUIManager::ShowNotifications() {
  while (!show_queue_.empty()) {
    linked_ptr<QueuedNotification> queued_notification(show_queue_.front());
    show_queue_.pop_front();
    if (!ShowNotification(queued_notification->notification(),
                          queued_notification->profile())) {
      show_queue_.push_front(queued_notification);
      return;
    }
  }
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

void BalloonNotificationUIManager::GetQueuedNotificationsForTesting(
    std::vector<const Notification*>* notifications) {
  for (NotificationDeque::const_iterator iter = show_queue_.begin();
       iter != show_queue_.end(); ++iter) {
    notifications->push_back(&(*iter)->notification());
  }
}
