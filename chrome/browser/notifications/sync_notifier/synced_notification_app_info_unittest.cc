// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/notifications/sync_notifier/synced_notification_app_info.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace {

static char kTestSendingServiceName[] = "Sending-Service";
static char kTestAppId1[] = "TestAppId1";
static char kTestAppId2[] = "TestAppId2";

}  // namespace

namespace notifier {

typedef testing::Test SyncedNotificationAppInfoTest;

TEST_F(SyncedNotificationAppInfoTest, AddRemoveTest) {
  SyncedNotificationAppInfo app_info(kTestSendingServiceName);

  app_info.AddAppId(kTestAppId1);

  // Ensure the app id is found
  EXPECT_TRUE(app_info.HasAppId(kTestAppId1));

  // Ensure a second app id that has not been added is not found.
  EXPECT_FALSE(app_info.HasAppId(kTestAppId2));

  // Remove the first ID and ensure it is no longer found.
  app_info.RemoveAppId(kTestAppId1);
  EXPECT_FALSE(app_info.HasAppId(kTestAppId1));
}

TEST_F(SyncedNotificationAppInfoTest, GetAppIdListTest) {
  SyncedNotificationAppInfo app_info(kTestSendingServiceName);

  // Add a few app infos.
  app_info.AddAppId(kTestAppId1);
  app_info.AddAppId(kTestAppId2);

  std::vector<std::string> app_id_list;
  app_info.GetAppIdList(&app_id_list);

  EXPECT_EQ(std::string(kTestAppId1), app_id_list[0]);
  EXPECT_EQ(std::string(kTestAppId2), app_id_list[1]);
}

}  // namespace notifier
