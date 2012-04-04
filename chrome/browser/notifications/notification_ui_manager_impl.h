// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_UI_MANAGER_IMPL_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_UI_MANAGER_IMPL_H_
#pragma once

#include <deque>
#include <string>
#include <vector>

#include "base/id_map.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer.h"
#include "chrome/browser/notifications/balloon.h"
#include "chrome/browser/notifications/balloon_collection.h"
#include "chrome/browser/notifications/notification_prefs_manager.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/prefs/pref_member.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Notification;
class PrefService;
class Profile;
class QueuedNotification;

// The notification manager manages use of the desktop for notifications.
// It maintains a queue of pending notifications when space becomes constrained.
class NotificationUIManagerImpl
    : public NotificationUIManager,
      public NotificationPrefsManager,
      public BalloonCollection::BalloonSpaceChangeListener,
      public content::NotificationObserver {
 public:
  explicit NotificationUIManagerImpl(PrefService* local_state);
  virtual ~NotificationUIManagerImpl();

  // Initializes the UI manager with a balloon collection; this object
  // takes ownership of the balloon collection.
  void Initialize(BalloonCollection* balloon_collection);

  // NotificationUIManager:
  virtual void Add(const Notification& notification,
                   Profile* profile) OVERRIDE;
  virtual bool CancelById(const std::string& notification_id) OVERRIDE;
  virtual bool CancelAllBySourceOrigin(const GURL& source_origin) OVERRIDE;
  virtual void CancelAll() OVERRIDE;
  virtual BalloonCollection* balloon_collection() OVERRIDE;
  virtual NotificationPrefsManager* prefs_manager() OVERRIDE;
  virtual void GetQueuedNotificationsForTesting(
      std::vector<const Notification*>* notifications) OVERRIDE;

  // NotificationPrefsManager:
  virtual BalloonCollection::PositionPreference
      GetPositionPreference() const OVERRIDE;
  virtual void SetPositionPreference(
      BalloonCollection::PositionPreference preference) OVERRIDE;

 private:
  // content::NotificationObserver override.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Attempts to display notifications from the show_queue if the user
  // is active.
  void CheckAndShowNotifications();

  // Attempts to display notifications from the show_queue.
  void ShowNotifications();

  // BalloonCollectionObserver implementation.
  virtual void OnBalloonSpaceChanged() OVERRIDE;

  // Replace an existing notification with this one if applicable;
  // returns true if the replacement happened.
  bool TryReplacement(const Notification& notification);

  // Checks the user state to decide if we want to show the notification.
  void CheckUserState();

  // An owned pointer to the collection of active balloons.
  scoped_ptr<BalloonCollection> balloon_collection_;

  // A queue of notifications which are waiting to be shown.
  typedef std::deque<QueuedNotification*> NotificationDeque;
  NotificationDeque show_queue_;

  // Registrar for the other kind of notifications (event signaling).
  content::NotificationRegistrar registrar_;

  // Prefs listener for the position preference.
  IntegerPrefMember position_pref_;

  // Used by screen-saver and full-screen handling support.
  bool is_user_active_;
  base::RepeatingTimer<NotificationUIManagerImpl> user_state_check_timer_;

  DISALLOW_COPY_AND_ASSIGN(NotificationUIManagerImpl);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_UI_MANAGER_IMPL_H_
