// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_TEST_UTIL_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_TEST_UTIL_H_

#include <string>

#include "chrome/browser/notifications/notification_object_proxy.h"
#include "chrome/browser/notifications/balloon.h"
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
  virtual content::RenderViewHost* GetRenderViewHost() const OVERRIDE;

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
  virtual content::RenderViewHost* GetRenderViewHost() const OVERRIDE {
    return NULL;
  }

 private:
  std::string notification_id_;

  DISALLOW_COPY_AND_ASSIGN(LoggingNotificationDelegate);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_TEST_UTIL_H_
