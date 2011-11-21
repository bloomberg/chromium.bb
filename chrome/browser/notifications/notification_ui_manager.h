// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_UI_MANAGER_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_UI_MANAGER_H_
#pragma once

#include <deque>
#include <string>
#include <vector>

#include "base/id_map.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer.h"
#include "chrome/browser/notifications/balloon.h"
#include "chrome/browser/notifications/balloon_collection.h"
#include "chrome/browser/prefs/pref_member.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Notification;
class PrefService;
class Profile;
class QueuedNotification;

// The notification manager manages use of the desktop for notifications.
// It maintains a queue of pending notifications when space becomes constrained.
class NotificationUIManager
    : public BalloonCollection::BalloonSpaceChangeListener,
      public content::NotificationObserver {
 public:
  virtual ~NotificationUIManager();

  // Creates an initialized UI manager with a new balloon collection
  // and the listener relationship setup.
  // Except for unit tests, this is the way to construct the object.
  static NotificationUIManager* Create(PrefService* local_state);

  // Creates an initialized UI manager with the given balloon collection
  // and the listener relationship setup.
  // Used primarily by unit tests.
  static NotificationUIManager* Create(PrefService* local_state,
                                       BalloonCollection* balloons);

  // Registers preferences.
  static void RegisterPrefs(PrefService* prefs);

  // Initializes the UI manager with a balloon collection; this object
  // takes ownership of the balloon collection.
  void Initialize(BalloonCollection* balloon_collection);

  // Adds a notification to be displayed. Virtual for unit test override.
  virtual void Add(const Notification& notification,
                   Profile* profile);

  // Removes any notifications matching the supplied ID, either currently
  // displayed or in the queue.  Returns true if anything was removed.
  virtual bool CancelById(const std::string& notification_id);

  // Removes any notifications matching the supplied source origin
  // (which could be an extension ID), either currently displayed or in the
  // queue.  Returns true if anything was removed.
  virtual bool CancelAllBySourceOrigin(const GURL& source_origin);

  // Cancels all pending notifications and closes anything currently showing.
  // Used when the app is terminating.
  void CancelAll();

  // Returns balloon collection.
  BalloonCollection* balloon_collection() {
    return balloon_collection_.get();
  }

  // Gets the preference indicating where notifications should be placed.
  BalloonCollection::PositionPreference GetPositionPreference();

  // Sets the preference that indicates where notifications should
  // be placed on the screen.
  void SetPositionPreference(BalloonCollection::PositionPreference preference);

  // Retrieves an ordered list of all queued notifications.
  // Used only for automation/testing.
  void GetQueuedNotificationsForTesting(
      std::vector<const Notification*>* notifications);

 private:
  explicit NotificationUIManager(PrefService* local_state);

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
  base::RepeatingTimer<NotificationUIManager> user_state_check_timer_;

  DISALLOW_COPY_AND_ASSIGN(NotificationUIManager);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_UI_MANAGER_H_
