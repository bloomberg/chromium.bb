// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NOTIFICATIONS_DESKTOP_NOTIFICATIONS_UNITTEST_H_
#define CHROME_BROWSER_CHROMEOS_NOTIFICATIONS_DESKTOP_NOTIFICATIONS_UNITTEST_H_
#pragma once

#include <set>
#include <string>

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/notifications/balloon_collection_impl.h"
#include "chrome/browser/notifications/balloon.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_test_util.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/test/base/testing_pref_service.h"
#include "chrome/test/base/testing_profile.h"
#include "content/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
struct ShowDesktopNotificationHostMsgParams;
}

namespace chromeos {

class DesktopNotificationsTest;
typedef LoggingNotificationDelegate<DesktopNotificationsTest>
    LoggingNotificationProxy;

// Test version of the balloon collection which counts the number
// of notifications that are added to it.
class MockBalloonCollection : public BalloonCollectionImpl {
 public:
  MockBalloonCollection();
  virtual ~MockBalloonCollection();

  // BalloonCollectionImpl overrides
  virtual void Add(const Notification& notification,
                   Profile* profile) OVERRIDE;
  virtual Balloon* MakeBalloon(const Notification& notification,
                               Profile* profile) OVERRIDE;
  virtual void OnBalloonClosed(Balloon* source) OVERRIDE;

  // Number of balloons being shown.
  std::set<Balloon*>& balloons() { return balloons_; }
  int count() const { return balloons_.size(); }

  // Returns the highest y-coordinate of all the balloons in the collection.
  int UppermostVerticalPosition();

 private:
  std::set<Balloon*> balloons_;
};

class DesktopNotificationsTest : public testing::Test {
 public:
  DesktopNotificationsTest();
  virtual ~DesktopNotificationsTest();

  static void log(const std::string& message) {
    log_output_.append(message);
  }

  Profile* profile() { return profile_.get(); }

 protected:
  // testing::Test overrides
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  void AllowOrigin(const GURL& origin) {
    service_->GrantPermission(origin);
  }

  void DenyOrigin(const GURL& origin) {
    service_->DenyPermission(origin);
  }

  int HasPermission(const GURL& origin) {
    return service_->HasPermission(origin);
  }

  // Constructs a notification parameter structure for use in tests.
  content::ShowDesktopNotificationHostMsgParams StandardTestNotification();

  // Create a message loop to allow notifications code to post tasks,
  // and a thread so that notifications code runs on the expected thread.
  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;

  // Mock local state.
  TestingPrefService local_state_;

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

  // Initializes / shuts down a stub CrosLibrary.
  chromeos::ScopedStubCrosEnabler stub_cros_enabler_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NOTIFICATIONS_DESKTOP_NOTIFICATIONS_UNITTEST_H_
