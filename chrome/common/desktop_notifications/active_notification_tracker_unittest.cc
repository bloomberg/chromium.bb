// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/desktop_notifications/active_notification_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(ActiveNotificationTrackerTest, TestLookupAndClear) {
  ActiveNotificationTracker tracker;

  WebKit::WebNotification notification1;
  int id1 = tracker.RegisterNotification(notification1);

  WebKit::WebNotification notification2;
  int id2 = tracker.RegisterNotification(notification2);

  WebKit::WebNotification result;
  tracker.GetNotification(id1, &result);
  EXPECT_TRUE(result == notification1);

  tracker.GetNotification(id2, &result);
  EXPECT_TRUE(result == notification2);

  tracker.Clear();
}
