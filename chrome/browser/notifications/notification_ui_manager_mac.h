// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_UI_MANAGER_MAC_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_UI_MANAGER_MAC_H_

#include <set>
#include <string>

#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "chrome/browser/notifications/notification_ui_manager.h"

class Notification;
@class NotificationCenterDelegate;
class PrefService;
class Profile;

// This class is an implementation of NotificationUIManager that will
// send platform notifications to the Notification Center on 10.8 and above.
// 10.7 and below don't offer native notification support so chrome
// notifications are still used there.
class NotificationUIManagerMac : public NotificationUIManager {
 public:
  NotificationUIManagerMac();
  ~NotificationUIManagerMac() override;

  // NotificationUIManager implementation
  void Add(const Notification& notification, Profile* profile) override;
  bool Update(const Notification& notification, Profile* profile) override;

  // |delegate_id| need to reference a PersistentNotificationDelegate
  const Notification* FindById(const std::string& delegate_id,
                               ProfileID profile_id) const override;

  // |delegate_id| need to reference a PersistentNotificationDelegate
  bool CancelById(const std::string& delegate_id,
                  ProfileID profile_id) override;
  std::set<std::string> GetAllIdsByProfileAndSourceOrigin(
      ProfileID profile_id,
      const GURL& source) override;
  std::set<std::string> GetAllIdsByProfile(ProfileID profile_id) override;
  bool CancelAllBySourceOrigin(const GURL& source_origin) override;
  bool CancelAllByProfile(ProfileID profile_id) override;
  void CancelAll() override;

 private:
  // Cocoa class that receives callbacks from the NSUserNotificationCenter.
  base::scoped_nsobject<NotificationCenterDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(NotificationUIManagerMac);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_UI_MANAGER_MAC_H_
