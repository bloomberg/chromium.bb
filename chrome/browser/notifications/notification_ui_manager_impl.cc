// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_ui_manager_impl.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/fullscreen.h"
#include "chrome/browser/idle.h"
#include "chrome/browser/notifications/balloon_collection.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"

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

NotificationUIManagerImpl::NotificationUIManagerImpl()
    : is_user_active_(true) {
  registrar_.Add(this, chrome::NOTIFICATION_APP_TERMINATING,
                 content::NotificationService::AllSources());
#if defined(OS_MACOSX)
  InitFullScreenMonitor();
  InitIdleMonitor();
#endif
}

NotificationUIManagerImpl::~NotificationUIManagerImpl() {
  STLDeleteElements(&show_queue_);
#if defined(OS_MACOSX)
  StopIdleMonitor();
  StopFullScreenMonitor();
#endif
}

void NotificationUIManagerImpl::Add(const Notification& notification,
                                    Profile* profile) {
  if (TryReplacement(notification, profile)) {
    return;
  }

  VLOG(1) << "Added notification. URL: "
          << notification.content_url().spec();
  show_queue_.push_back(
      new QueuedNotification(notification, profile));
  CheckAndShowNotifications();
}

bool NotificationUIManagerImpl::CancelById(const std::string& id) {
  // See if this ID hasn't been shown yet.
  for (NotificationDeque::iterator iter = show_queue_.begin();
       iter != show_queue_.end(); ++iter) {
    if ((*iter)->notification().notification_id() == id) {
      show_queue_.erase(iter);
      return true;
    }
  }
  return false;
}

bool NotificationUIManagerImpl::CancelAllBySourceOrigin(const GURL& source) {
  // Same pattern as CancelById, but more complicated than the above
  // because there may be multiple notifications from the same source.
  bool removed = false;
  for (NotificationDeque::iterator loopiter = show_queue_.begin();
       loopiter != show_queue_.end(); ) {
    NotificationDeque::iterator curiter = loopiter++;
    if ((*curiter)->notification().origin_url() == source) {
      show_queue_.erase(curiter);
      removed = true;
    }
  }
  return removed;
}

bool NotificationUIManagerImpl::CancelAllByProfile(Profile* profile) {
  // Same pattern as CancelAllBySourceOrigin.
  bool removed = false;
  for (NotificationDeque::iterator loopiter = show_queue_.begin();
       loopiter != show_queue_.end(); ) {
    NotificationDeque::iterator curiter = loopiter++;
    if ((*curiter)->profile() == profile) {
      show_queue_.erase(curiter);
      removed = true;
    }
  }
  return removed;
}

void NotificationUIManagerImpl::CancelAll() {
  STLDeleteElements(&show_queue_);
}

void NotificationUIManagerImpl::CheckAndShowNotifications() {
  CheckUserState();
  if (is_user_active_)
    ShowNotifications();
}

void NotificationUIManagerImpl::CheckUserState() {
  bool is_user_active_previously = is_user_active_;
  is_user_active_ = !CheckIdleStateIsLocked() && !IsFullScreenMode();
  if (is_user_active_ == is_user_active_previously)
    return;

  if (is_user_active_) {
    user_state_check_timer_.Stop();
    // We need to show any postponed nofications when the user becomes active
    // again.
    ShowNotifications();
  } else if (!user_state_check_timer_.IsRunning()) {
    // Start a timer to detect the moment at which the user becomes active.
    user_state_check_timer_.Start(FROM_HERE,
        base::TimeDelta::FromSeconds(kUserStatePollingIntervalSeconds), this,
        &NotificationUIManagerImpl::CheckUserState);
  }
}

void NotificationUIManagerImpl::ShowNotifications() {
  while (!show_queue_.empty()) {
    scoped_ptr<QueuedNotification> queued_notification(show_queue_.front());
    show_queue_.pop_front();
    if (!ShowNotification(queued_notification->notification(),
                          queued_notification->profile())) {
      // Subclass could not show notification, put it back in the queue.
      show_queue_.push_front(queued_notification.release());
      return;
    }
  }
}

bool NotificationUIManagerImpl::TryReplacement(
    const Notification& notification, Profile* profile) {
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

  // Give the subclass the opportunity to update existing notification.
  return UpdateNotification(notification, profile);
}

void NotificationUIManagerImpl::GetQueuedNotificationsForTesting(
    std::vector<const Notification*>* notifications) {
  for (NotificationDeque::const_iterator iter = show_queue_.begin();
       iter != show_queue_.end(); ++iter) {
    notifications->push_back(&(*iter)->notification());
  }
}

void NotificationUIManagerImpl::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_APP_TERMINATING) {
    CancelAll();
  } else {
    NOTREACHED();
  }
}
