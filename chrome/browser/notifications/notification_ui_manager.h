// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_UI_MANAGER_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_UI_MANAGER_H_

#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"

class GURL;
class Notification;
class PrefService;
class Profile;

// This virtual interface is used to manage the UI surfaces for desktop
// notifications. There is one instance per profile.
class NotificationUIManager {
 public:
  virtual ~NotificationUIManager() {}

  // Creates an initialized UI manager.
  static NotificationUIManager* Create(PrefService* local_state);

  // Adds a notification to be displayed. Virtual for unit test override.
  virtual void Add(const Notification& notification, Profile* profile) = 0;

  // Updates an existing notification. If |update_progress_only|, assume
  // only message and progress properties are updated.
  virtual bool Update(const Notification& notification, Profile* profile) = 0;

  // Returns the pointer to a notification if it match the supplied ID, either
  // currently displayed or in the queue.
  virtual const Notification* FindById(
      const std::string& notification_id) const = 0;

  // Removes any notifications matching the supplied ID, either currently
  // displayed or in the queue.  Returns true if anything was removed.
  virtual bool CancelById(const std::string& notification_id) = 0;

  // Adds the notification_id for each outstanding notification to the set
  // |notification_ids| (must not be NULL).
  virtual std::set<std::string> GetAllIdsByProfileAndSourceOrigin(
      Profile* profile,
      const GURL& source) = 0;

  // Removes notifications matching the |source_origin| (which could be an
  // extension ID). Returns true if anything was removed.
  virtual bool CancelAllBySourceOrigin(const GURL& source_origin) = 0;

  // Removes notifications matching |profile|. Returns true if any were removed.
  virtual bool CancelAllByProfile(Profile* profile) = 0;

  // Cancels all pending notifications and closes anything currently showing.
  // Used when the app is terminating.
  virtual void CancelAll() = 0;

 protected:
  NotificationUIManager() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NotificationUIManager);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_UI_MANAGER_H_
