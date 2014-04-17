// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_TEST_UTIL_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_TEST_UTIL_H_

#include <set>
#include <string>

#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_object_proxy.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/notifications/sync_notifier/chrome_notifier_service.h"
#include "chrome/browser/profiles/profile.h"
#include "ui/gfx/size.h"

// NotificationDelegate which does nothing, useful for testing when
// the notification events are not important.
class MockNotificationDelegate : public NotificationDelegate {
 public:
  explicit MockNotificationDelegate(const std::string& id);

  // NotificationDelegate interface.
  virtual void Display() OVERRIDE {}
  virtual void Error() OVERRIDE {}
  virtual void Close(bool by_user) OVERRIDE {}
  virtual void Click() OVERRIDE {}
  virtual std::string id() const OVERRIDE;
  virtual content::WebContents* GetWebContents() const OVERRIDE;

 private:
  virtual ~MockNotificationDelegate();

  std::string id_;

  DISALLOW_COPY_AND_ASSIGN(MockNotificationDelegate);
};

// Mock implementation of Javascript object proxy which logs events that
// would have been fired on it.  Useful for tests where the sequence of
// notification events needs to be verified.
//
// |Logger| class provided in template must implement method
// static void log(string);
template<class Logger>
class LoggingNotificationDelegate : public NotificationDelegate {
 public:
  explicit LoggingNotificationDelegate(std::string id)
      : notification_id_(id) {
  }

  // NotificationObjectProxy override
  virtual void Display() OVERRIDE {
    Logger::log("notification displayed\n");
  }
  virtual void Error() OVERRIDE {
    Logger::log("notification error\n");
  }
  virtual void Click() OVERRIDE {
    Logger::log("notification clicked\n");
  }
  virtual void ButtonClick(int index) OVERRIDE {
    Logger::log("notification button clicked\n");
  }
  virtual void Close(bool by_user) OVERRIDE {
    if (by_user)
      Logger::log("notification closed by user\n");
    else
      Logger::log("notification closed by script\n");
  }
  virtual std::string id() const OVERRIDE {
    return notification_id_;
  }
  virtual content::WebContents* GetWebContents() const OVERRIDE {
    return NULL;
  }

 private:
  std::string notification_id_;

  DISALLOW_COPY_AND_ASSIGN(LoggingNotificationDelegate);
};

class StubNotificationUIManager : public NotificationUIManager {
 public:
  explicit StubNotificationUIManager(const GURL& welcome_origin);
  virtual ~StubNotificationUIManager();

  // Adds a notification to be displayed. Virtual for unit test override.
  virtual void Add(const Notification& notification, Profile* profile) OVERRIDE;
  virtual bool Update(const Notification& notification,
                      Profile* profile) OVERRIDE;

  // Returns NULL if no notifications match the supplied ID, either currently
  // displayed or in the queue.
  virtual const Notification* FindById(const std::string& id) const OVERRIDE;

  // Removes any notifications matching the supplied ID, either currently
  // displayed or in the queue.  Returns true if anything was removed.
  virtual bool CancelById(const std::string& notification_id) OVERRIDE;

  // Adds the notification_id for each outstanding notification to the set
  // |notification_ids| (must not be NULL).
  virtual std::set<std::string> GetAllIdsByProfileAndSourceOrigin(
      Profile* profile,
      const GURL& source) OVERRIDE;

  // Removes notifications matching the |source_origin| (which could be an
  // extension ID). Returns true if anything was removed.
  virtual bool CancelAllBySourceOrigin(const GURL& source_origin) OVERRIDE;

  // Removes notifications matching |profile|. Returns true if any were removed.
  virtual bool CancelAllByProfile(Profile* profile) OVERRIDE;

  // Cancels all pending notifications and closes anything currently showing.
  // Used when the app is terminating.
  virtual void CancelAll() OVERRIDE;

  // Test hook to get the notification so we can check it
  const Notification& notification() const { return notification_; }

  // Test hook to check the ID of the last notification cancelled.
  std::string& dismissed_id() { return dismissed_id_; }

  size_t added_notifications() const { return added_notifications_; }
  bool welcomed() const { return welcomed_; }

 private:
  DISALLOW_COPY_AND_ASSIGN(StubNotificationUIManager);
  Notification notification_;
  Profile* profile_;
  std::string dismissed_id_;
  GURL welcome_origin_;
  bool welcomed_;
  size_t added_notifications_;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_TEST_UTIL_H_
