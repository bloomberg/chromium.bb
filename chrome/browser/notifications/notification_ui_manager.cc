// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_ui_manager.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util-inl.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/fullscreen.h"
#include "chrome/browser/idle.h"
#include "chrome/browser/notifications/balloon_collection.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/pref_names.h"
#include "content/browser/site_instance.h"
#include "content/common/notification_service.h"
#include "content/common/notification_type.h"

namespace {
const int kUserStatePollingIntervalSeconds = 1;
}

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

NotificationUIManager::NotificationUIManager(PrefService* local_state)
    : balloon_collection_(NULL),
      is_user_active_(true) {
  registrar_.Add(this, NotificationType::APP_TERMINATING,
                 NotificationService::AllSources());
  position_pref_.Init(prefs::kDesktopNotificationPosition, local_state, this);
#if defined(OS_MACOSX)
  InitFullScreenMonitor();
#endif
}

NotificationUIManager::~NotificationUIManager() {
  STLDeleteElements(&show_queue_);
#if defined(OS_MACOSX)
  StopFullScreenMonitor();
#endif
}

// static
NotificationUIManager* NotificationUIManager::Create(PrefService* local_state) {
  BalloonCollection* balloons = BalloonCollection::Create();
  NotificationUIManager* instance = new NotificationUIManager(local_state);
  instance->Initialize(balloons);
  balloons->set_space_change_listener(instance);
  return instance;
}

// static
void NotificationUIManager::RegisterPrefs(PrefService* prefs) {
  prefs->RegisterIntegerPref(prefs::kDesktopNotificationPosition,
                             BalloonCollection::DEFAULT_POSITION);
}

void NotificationUIManager::Initialize(
    BalloonCollection* balloon_collection) {
  DCHECK(!balloon_collection_.get());
  DCHECK(balloon_collection);
  balloon_collection_.reset(balloon_collection);
  balloon_collection_->SetPositionPreference(
      static_cast<BalloonCollection::PositionPreference>(
          position_pref_.GetValue()));
}

void NotificationUIManager::Add(const Notification& notification,
                                Profile* profile) {
  if (TryReplacement(notification)) {
    return;
  }

  VLOG(1) << "Added notification. URL: "
          << notification.content_url().spec();
  show_queue_.push_back(
      new QueuedNotification(notification, profile));
  CheckAndShowNotifications();
}

bool NotificationUIManager::CancelById(const std::string& id) {
  // See if this ID hasn't been shown yet.
  NotificationDeque::iterator iter;
  for (iter = show_queue_.begin(); iter != show_queue_.end(); ++iter) {
    if ((*iter)->notification().notification_id() == id) {
      show_queue_.erase(iter);
      return true;
    }
  }
  // If it has been shown, remove it from the balloon collections.
  return balloon_collection_->RemoveById(id);
}

bool NotificationUIManager::CancelAllBySourceOrigin(const GURL& source) {
  // Same pattern as CancelById, but more complicated than the above
  // because there may be multiple notifications from the same source.
  bool removed = false;
  NotificationDeque::iterator iter;
  for (iter = show_queue_.begin(); iter != show_queue_.end();) {
    if ((*iter)->notification().origin_url() == source) {
      iter = show_queue_.erase(iter);
      removed = true;
    } else {
      ++iter;
    }
  }

  return balloon_collection_->RemoveBySourceOrigin(source) || removed;
}

void NotificationUIManager::CancelAll() {
  STLDeleteElements(&show_queue_);
  balloon_collection_->RemoveAll();
}

void NotificationUIManager::CheckAndShowNotifications() {
  CheckUserState();
  if (is_user_active_)
    ShowNotifications();
}

void NotificationUIManager::CheckUserState() {
  bool is_user_active_previously = is_user_active_;
  is_user_active_ = CalculateIdleState(0) != IDLE_STATE_LOCKED &&
                    !IsFullScreenMode();
  if (is_user_active_ == is_user_active_previously)
    return;

  if (is_user_active_) {
    user_state_check_timer_.Stop();
    // We need to show any postponed nofications when the user becomes active
    // again.
    ShowNotifications();
  } else if (!user_state_check_timer_.IsRunning()) {
    // Start a timer to detect the moment at which the user becomes active.
    user_state_check_timer_.Start(
        base::TimeDelta::FromSeconds(kUserStatePollingIntervalSeconds), this,
        &NotificationUIManager::CheckUserState);
  }
}

void NotificationUIManager::ShowNotifications() {
  while (!show_queue_.empty() && balloon_collection_->HasSpace()) {
    scoped_ptr<QueuedNotification> queued_notification(show_queue_.front());
    show_queue_.pop_front();
    balloon_collection_->Add(queued_notification->notification(),
                             queued_notification->profile());
  }
}

void NotificationUIManager::OnBalloonSpaceChanged() {
  CheckAndShowNotifications();
}

bool NotificationUIManager::TryReplacement(const Notification& notification) {
  const GURL& origin = notification.origin_url();
  const string16& replace_id = notification.replace_id();

  if (replace_id.empty())
    return false;

  // First check the queue of pending notifications for replacement.
  // Then check the list of notifications already being shown.
  NotificationDeque::iterator iter;
  for (iter = show_queue_.begin(); iter != show_queue_.end(); ++iter) {
    if (origin == (*iter)->notification().origin_url() &&
        replace_id == (*iter)->notification().replace_id()) {
      (*iter)->Replace(notification);
      return true;
    }
  }

  BalloonCollection::Balloons::iterator balloon_iter;
  BalloonCollection::Balloons balloons =
      balloon_collection_->GetActiveBalloons();
  for (balloon_iter = balloons.begin();
       balloon_iter != balloons.end();
       ++balloon_iter) {
    if (origin == (*balloon_iter)->notification().origin_url() &&
        replace_id == (*balloon_iter)->notification().replace_id()) {
      (*balloon_iter)->Update(notification);
      return true;
    }
  }

  return false;
}

BalloonCollection::PositionPreference
NotificationUIManager::GetPositionPreference() {
  LOG(INFO) << "Current position preference: " << position_pref_.GetValue();

  return static_cast<BalloonCollection::PositionPreference>(
      position_pref_.GetValue());
}

void NotificationUIManager::SetPositionPreference(
    BalloonCollection::PositionPreference preference) {
  LOG(INFO) << "Setting position preference: " << preference;
  position_pref_.SetValue(static_cast<int>(preference));
  balloon_collection_->SetPositionPreference(preference);
}

void NotificationUIManager::Observe(NotificationType type,
                                    const NotificationSource& source,
                                    const NotificationDetails& details) {
  if (type == NotificationType::APP_TERMINATING) {
    CancelAll();
  } else if (type == NotificationType::PREF_CHANGED) {
    std::string* name = Details<std::string>(details).ptr();
    if (*name == prefs::kDesktopNotificationPosition)
      balloon_collection_->SetPositionPreference(
          static_cast<BalloonCollection::PositionPreference>(
              position_pref_.GetValue()));
  } else {
    NOTREACHED();
  }
}
