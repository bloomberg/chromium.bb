// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_BALLOON_NOTIFICATION_UI_MANAGER_H_
#define CHROME_BROWSER_NOTIFICATIONS_BALLOON_NOTIFICATION_UI_MANAGER_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_member.h"
#include "chrome/browser/notifications/balloon.h"
#include "chrome/browser/notifications/balloon_collection.h"
#include "chrome/browser/notifications/notification_prefs_manager.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/notifications/notification_ui_manager_impl.h"

class Notification;
class PrefService;
class Profile;

// The notification manager manages use of the desktop for notifications.
// It maintains a queue of pending notifications when space becomes constrained.
class BalloonNotificationUIManager
    : public NotificationUIManagerImpl,
      public NotificationPrefsManager,
      public BalloonCollection::BalloonSpaceChangeListener {
 public:
  explicit BalloonNotificationUIManager(PrefService* local_state);
  virtual ~BalloonNotificationUIManager();

  // Initializes the UI manager with a balloon collection; this object
  // takes ownership of the balloon collection.
  void SetBalloonCollection(BalloonCollection* balloon_collection);

  // NotificationUIManager:
  virtual bool DoesIdExist(const std::string& notification_id) OVERRIDE;
  virtual bool CancelById(const std::string& notification_id) OVERRIDE;
  virtual bool CancelAllBySourceOrigin(const GURL& source_origin) OVERRIDE;
  virtual bool CancelAllByProfile(Profile* profile) OVERRIDE;
  virtual void CancelAll() OVERRIDE;

  // NotificationUIManagerImpl:
  virtual bool ShowNotification(const Notification& notification,
                                Profile* profile) OVERRIDE;
  virtual bool UpdateNotification(const Notification& notification,
                                  Profile* profile) OVERRIDE;

  // NotificationPrefsManager:
  virtual BalloonCollection::PositionPreference
      GetPositionPreference() const OVERRIDE;
  virtual void SetPositionPreference(
      BalloonCollection::PositionPreference preference) OVERRIDE;

  BalloonCollection* balloon_collection();
  NotificationPrefsManager* prefs_manager();

  // Helper used to pull the static instance for testing.
  // In tests, use this instead of g_browser_process->notification_ui_manager().
  static BalloonNotificationUIManager* GetInstanceForTesting();

 private:
  void OnDesktopNotificationPositionChanged();

  // BalloonCollectionObserver implementation.
  virtual void OnBalloonSpaceChanged() OVERRIDE;

  // An owned pointer to the collection of active balloons.
  scoped_ptr<BalloonCollection> balloon_collection_;

  // Prefs listener for the position preference.
  IntegerPrefMember position_pref_;

  DISALLOW_COPY_AND_ASSIGN(BalloonNotificationUIManager);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_BALLOON_NOTIFICATION_UI_MANAGER_H_
