// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_TEST_UTIL_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_TEST_UTIL_H_
#pragma once

#include <string>

#include "chrome/browser/notifications/notification_object_proxy.h"
#include "chrome/browser/notifications/balloon.h"
#include "ui/gfx/size.h"

// NotificationDelegate which does nothing, useful for testing when
// the notification events are not important.
class MockNotificationDelegate : public NotificationDelegate {
 public:
  explicit MockNotificationDelegate(const std::string& id);
  virtual ~MockNotificationDelegate();

  // NotificationDelegate interface.
  virtual void Display() {}
  virtual void Error() {}
  virtual void Close(bool by_user) {}
  virtual void Click() {}
  virtual std::string id() const;

 private:
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
  virtual void Display() {
    Logger::log("notification displayed\n");
  }
  virtual void Error() {
    Logger::log("notification error\n");
  }
  virtual void Click() {
    Logger::log("notification clicked\n");
  }
  virtual void Close(bool by_user) {
    if (by_user)
      Logger::log("notification closed by user\n");
    else
      Logger::log("notification closed by script\n");
  }
  virtual std::string id() const {
    return notification_id_;
  }
 private:
  std::string notification_id_;

  DISALLOW_COPY_AND_ASSIGN(LoggingNotificationDelegate);
};

// Test version of a balloon view which doesn't do anything
// viewable, but does know how to close itself the same as a regular
// BalloonView.
class MockBalloonView : public BalloonView {
 public:
  explicit MockBalloonView(Balloon * balloon) :
      balloon_(balloon) {}

  // BalloonView:
  virtual void Show(Balloon* balloon) {}
  virtual void Update() {}
  virtual void RepositionToBalloon() {}
  virtual void Close(bool by_user);
  virtual gfx::Size GetSize() const;
  virtual BalloonHost* GetHost() const;

 private:
  // Non-owned pointer.
  Balloon* balloon_;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_TEST_UTIL_H_
