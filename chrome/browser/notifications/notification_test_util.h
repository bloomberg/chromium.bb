// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_TEST_UTIL_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_TEST_UTIL_H_
#pragma once

#include "chrome/browser/notifications/notification_object_proxy.h"
#include "chrome/browser/notifications/balloon.h"
#include "gfx/size.h"

// Mock implementation of Javascript object proxy which logs events that
// would have been fired on it. |Logger| class must static "log()" method.
template<class Logger>
class LoggingNotificationProxyBase : public NotificationObjectProxy {
 public:
  LoggingNotificationProxyBase() :
      NotificationObjectProxy(0, 0, 0, false) {}

  // NotificationObjectProxy override
  virtual void Display() {
    Logger::log("notification displayed\n");
  }
  virtual void Error() {
    Logger::log("notification error\n");
  }
  virtual void Close(bool by_user) {
    if (by_user)
      Logger::log("notification closed by user\n");
    else
      Logger::log("notification closed by script\n");
  }
};

// Test version of a balloon view which doesn't do anything
// viewable, but does know how to close itself the same as a regular
// BalloonView.
class MockBalloonView : public BalloonView {
 public:
  explicit MockBalloonView(Balloon * balloon) :
      balloon_(balloon) {}
  void Show(Balloon* balloon) {}
  void Update() {}
  void RepositionToBalloon() {}
  void Close(bool by_user) { balloon_->OnClose(by_user); }
  gfx::Size GetSize() const { return balloon_->content_size(); }
  BalloonHost* GetHost() const { return NULL; }

 private:
  // Non-owned pointer.
  Balloon* balloon_;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_TEST_UTIL_H_
