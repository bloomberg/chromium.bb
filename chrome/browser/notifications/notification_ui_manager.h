// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_UI_MANAGER_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_UI_MANAGER_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"

class BalloonCollection;
class GURL;
class Notification;
class NotificationPrefsManager;
class PrefService;
class Profile;

// This virtual interface is used to manage the UI surfaces for desktop
// notifications. There is one instance per profile.
class NotificationUIManager {
 public:
  virtual ~NotificationUIManager() {}

  // Creates an initialized UI manager with a new balloon collection
  // and the listener relationship setup.
  // Except for unit tests, this is the way to construct the object.
  static NotificationUIManager* Create(PrefService* local_state);

  // Creates an initialized UI manager with the given balloon collection
  // and the listener relationship setup.
  // Used primarily by unit tests.
  static NotificationUIManager* Create(PrefService* local_state,
                                       BalloonCollection* balloons);

  // Adds a notification to be displayed. Virtual for unit test override.
  virtual void Add(const Notification& notification,
                   Profile* profile) = 0;

  // Removes any notifications matching the supplied ID, either currently
  // displayed or in the queue.  Returns true if anything was removed.
  virtual bool CancelById(const std::string& notification_id) = 0;

  // Removes any notifications matching the supplied source origin
  // (which could be an extension ID), either currently displayed or in the
  // queue.  Returns true if anything was removed.
  virtual bool CancelAllBySourceOrigin(const GURL& source_origin) = 0;

  // Cancels all pending notifications and closes anything currently showing.
  // Used when the app is terminating.
  virtual void CancelAll() = 0;

  // Returns balloon collection.
  virtual BalloonCollection* balloon_collection() = 0;

  // Returns the impl, for use primarily by testing.
  virtual NotificationPrefsManager* prefs_manager() = 0;

  // Retrieves an ordered list of all queued notifications.
  // Used only for automation/testing.
  virtual void GetQueuedNotificationsForTesting(
      std::vector<const Notification*>* notifications) {}

 protected:
  NotificationUIManager() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NotificationUIManager);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_UI_MANAGER_H_
