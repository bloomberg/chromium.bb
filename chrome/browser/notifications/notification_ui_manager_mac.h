// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_UI_MANAGER_MAC_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_UI_MANAGER_MAC_H_

#import <AppKit/AppKit.h>

#include <map>

#include "base/basictypes.h"
#include "base/memory/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "chrome/browser/notifications/notification_ui_manager.h"

@protocol CrUserNotification;
class Notification;
@class NotificationCenterDelegate;
class NotificationUIManagerImpl;
class PrefService;
class Profile;

// This class is an implementation of NotificationUIManager that will send
// text-only (non-HTML) notifications to Notification Center on 10.8. This
// class is only instantiated on 10.8 and above. For HTML notifications,
// this class uses an instance of NotificationUIManagerImpl to present
// notifications in the typical way.
class NotificationUIManagerMac : public NotificationUIManager {
 public:
  explicit NotificationUIManagerMac(PrefService* local_state);
  virtual ~NotificationUIManagerMac();

  // NotificationUIManager:
  virtual void Add(const Notification& notification,
                   Profile* profile) OVERRIDE;
  virtual bool CancelById(const std::string& notification_id) OVERRIDE;
  virtual bool CancelAllBySourceOrigin(const GURL& source_origin) OVERRIDE;
  virtual bool CancelAllByProfile(Profile* profile) OVERRIDE;
  virtual void CancelAll() OVERRIDE;
  virtual BalloonCollection* balloon_collection() OVERRIDE;
  virtual NotificationPrefsManager* prefs_manager() OVERRIDE;
  virtual void GetQueuedNotificationsForTesting(
      std::vector<const Notification*>* notifications) OVERRIDE;

  // Returns the corresponding C++ object for the Cocoa notification object,
  // or NULL if it could not be found.
  const Notification* FindNotificationWithCocoaNotification(
      id<CrUserNotification> notification) const;

  NotificationUIManagerImpl* builtin_manager() const {
    return builtin_manager_.get();
  }

 private:
  // A ControllerNotification links the Cocoa (view) notification to the C++
  // (model) notification. This assumes ownership (i.e. does not retain a_view)
  // of both objects and will delete them on destruction.
  struct ControllerNotification {
    ControllerNotification(Profile* a_profile,
                           id<CrUserNotification> a_view,
                           Notification* a_model);
    ~ControllerNotification();

    Profile* profile;
    id<CrUserNotification> view;
    Notification* model;

   private:
    DISALLOW_COPY_AND_ASSIGN(ControllerNotification);
  };
  typedef std::map<std::string, ControllerNotification*> NotificationMap;

  // Erases a Cocoa notification from both the application and Notification
  // Center, and deletes the corresponding C++ object.
  bool RemoveNotification(id<CrUserNotification> notification);

  // Finds a notification with a given replacement id.
  id<CrUserNotification> FindNotificationWithReplacementId(
      const string16& replacement_id) const;

  // The class used to present HTML notifications that can't be sent to
  // Notification Center.
  scoped_ptr<NotificationUIManagerImpl> builtin_manager_;

  // Cocoa class that receives callbacks from the NSUserNotificationCenter.
  scoped_nsobject<NotificationCenterDelegate> delegate_;

  // Maps notification_ids to ControllerNotifications. The map owns the value,
  // so it must be deleted when remvoed from the map. This is typically handled
  // in RemoveNotification.
  NotificationMap notification_map_;

  DISALLOW_COPY_AND_ASSIGN(NotificationUIManagerMac);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_UI_MANAGER_MAC_H_
