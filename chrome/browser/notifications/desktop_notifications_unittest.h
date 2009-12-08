// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_DESKTOP_NOTIFICATIONS_UNITTEST_H_
#define CHROME_BROWSER_NOTIFICATIONS_DESKTOP_NOTIFICATIONS_UNITTEST_H_

#include <set>
#include <string>

#include "base/message_loop.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/notifications/balloon.h"
#include "chrome/browser/notifications/balloon_collection.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_object_proxy.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/notifications/notifications_prefs_cache.h"
#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/WebKit/chromium/public/WebNotificationPresenter.h"

// Mock implementation of Javascript object proxy which logs events that
// would have been fired on it.
class LoggingNotificationProxy : public NotificationObjectProxy {
 public:
  LoggingNotificationProxy() :
      NotificationObjectProxy(0, 0, 0, false) {}

  // NotificationObjectProxy override
  virtual void Display();
  virtual void Error();
  virtual void Close(bool by_user);
};

// Test version of a balloon view which doesn't do anything
// viewable, but does know how to close itself the same as a regular
// BalloonView.
class MockBalloonView : public BalloonView {
 public:
  explicit MockBalloonView(Balloon * balloon) :
      balloon_(balloon) {}
  void Show(Balloon* balloon) {}
  void RepositionToBalloon() {}
  void Close(bool by_user) { balloon_->OnClose(by_user); }
  gfx::Size GetSize() const { return balloon_->content_size(); }

 private:
  // Non-owned pointer.
  Balloon* balloon_;
};

// Test version of the balloon collection which counts the number
// of notifications that are added to it.
class MockBalloonCollection : public BalloonCollectionImpl {
 public:
  MockBalloonCollection() :
    log_proxy_(new LoggingNotificationProxy()) {}

  // Our mock collection has an area large enough for a fixed number
  // of balloons.
  static const int kMockBalloonSpace;
  int max_balloon_count() const { return kMockBalloonSpace; }

  // BalloonCollectionImpl overrides
  virtual void Add(const Notification& notification,
                   Profile* profile);
  virtual bool Remove(const Notification& notification);
  virtual bool HasSpace() const { return count() < kMockBalloonSpace; }
  virtual Balloon* MakeBalloon(const Notification& notification,
                               Profile* profile);
  virtual void OnBalloonClosed(Balloon* source);

  // Number of balloons being shown.
  std::set<Balloon*>& balloons() { return balloons_; }
  int count() const { return balloons_.size(); }

  // Returns the highest y-coordinate of all the balloons in the collection.
  int UppermostVerticalPosition();

  // Returns the height bounds of a balloon.
  int MinHeight() { return Layout::min_balloon_height(); }
  int MaxHeight() { return Layout::max_balloon_height(); }

 private:
  std::set<Balloon*> balloons_;
  scoped_refptr<LoggingNotificationProxy> log_proxy_;
};

class DesktopNotificationsTest : public testing::Test {
 public:
  DesktopNotificationsTest();
  ~DesktopNotificationsTest();

  static void log(const std::string& message) {
    log_output_.append(message);
  }

  Profile* profile() { return profile_.get(); }

 protected:
  // testing::Test overrides
  virtual void SetUp();
  virtual void TearDown();

  void AllowOrigin(const GURL& origin) {
    service_->GrantPermission(origin);
  }

  void DenyOrigin(const GURL& origin) {
    service_->DenyPermission(origin);
  }

  int HasPermission(const GURL& origin) {
    return service_->prefs_cache()->HasPermission(origin);
  }

  // Create a message loop to allow notifications code to post tasks,
  // and a thread so that notifications code runs on the expected thread.
  MessageLoopForUI message_loop_;
  ChromeThread ui_thread_;

  // Test profile.
  scoped_ptr<TestingProfile> profile_;

  // Mock balloon collection -- owned by the NotificationUIManager
  MockBalloonCollection* balloon_collection_;

  // Real UI manager.
  scoped_ptr<NotificationUIManager> ui_manager_;

  // Real DesktopNotificationService
  scoped_ptr<DesktopNotificationService> service_;

  // Contains the cumulative output of the unit test.
  static std::string log_output_;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_DESKTOP_NOTIFICATIONS_UNITTEST_H_
