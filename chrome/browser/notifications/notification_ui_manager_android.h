// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_UI_MANAGER_ANDROID_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_UI_MANAGER_ANDROID_H_

#include "chrome/browser/notifications/notification_ui_manager.h"

// Implementation of the Notification UI Manager for Android, which defers to
// the Android framework for displaying notifications.
class NotificationUIManagerAndroid : public NotificationUIManager {
 public:
  NotificationUIManagerAndroid();
  virtual ~NotificationUIManagerAndroid();

  // NotificationUIManager implementation;
  virtual void Add(const Notification& notification, Profile* profile) override;
  virtual bool Update(const Notification& notification,
                      Profile* profile) override;
  virtual const Notification* FindById(const std::string& delegate_id,
                                       ProfileID profile_id) const override;
  virtual bool CancelById(const std::string& delegate_id,
                          ProfileID profile_id) override;
  virtual std::set<std::string> GetAllIdsByProfileAndSourceOrigin(
      Profile* profile,
      const GURL& source) override;
  virtual bool CancelAllBySourceOrigin(const GURL& source_origin) override;
  virtual bool CancelAllByProfile(Profile* profile) override;
  virtual void CancelAll() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(NotificationUIManagerAndroid);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_UI_MANAGER_ANDROID_H_
