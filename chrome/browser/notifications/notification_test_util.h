// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_TEST_UTIL_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_TEST_UTIL_H_

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_ui_manager.h"

class Profile;

// NotificationDelegate which does nothing, useful for testing when
// the notification events are not important.
class MockNotificationDelegate : public NotificationDelegate {
 public:
  explicit MockNotificationDelegate(const std::string& id);

  // NotificationDelegate interface.
  std::string id() const override;

 private:
  ~MockNotificationDelegate() override;

  std::string id_;

  DISALLOW_COPY_AND_ASSIGN(MockNotificationDelegate);
};

class StubNotificationUIManager : public NotificationUIManager {
 public:
  StubNotificationUIManager();
  ~StubNotificationUIManager() override;

  // Returns the number of currently active notifications.
  unsigned int GetNotificationCount() const;

  // Returns a reference to the notification at index |index|.
  const Notification& GetNotificationAt(unsigned int index) const;

  // Sets a one-shot callback that will be invoked when a notification has been
  // added to the Notification UI manager. Will be invoked on the UI thread.
  void SetNotificationAddedCallback(const base::Closure& callback);

  // NotificationUIManager implementation.
  void Add(const Notification& notification, Profile* profile) override;
  bool Update(const Notification& notification, Profile* profile) override;
  const Notification* FindById(const std::string& delegate_id,
                               ProfileID profile_id) const override;
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
  using NotificationPair = std::pair<Notification, ProfileID>;
  std::vector<NotificationPair> notifications_;

  base::Closure notification_added_callback_;

  DISALLOW_COPY_AND_ASSIGN(StubNotificationUIManager);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_TEST_UTIL_H_
