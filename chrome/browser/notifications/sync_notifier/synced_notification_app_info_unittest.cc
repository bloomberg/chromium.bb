// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/notifications/sync_notifier/sync_notifier_test_utils.h"
#include "chrome/browser/notifications/sync_notifier/synced_notification_app_info.h"
#include "chrome/browser/notifications/sync_notifier/synced_notification_app_info_service.h"
#include "sync/api/sync_error_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

static char kTestSendingServiceName[] = "Sending-Service";
static char kTestAppId1[] = "TestAppId1";
static char kTestAppId2[] = "TestAppId2";

}  // namespace

namespace notifier {

typedef testing::Test SyncedNotificationAppInfoTest;

TEST_F(SyncedNotificationAppInfoTest, AddRemoveTest) {
  SyncedNotificationAppInfo app_info(NULL, kTestSendingServiceName, NULL);

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
  SyncedNotificationAppInfo app_info(NULL, kTestSendingServiceName, NULL);

  // Add a few app infos.
  app_info.AddAppId(kTestAppId1);
  app_info.AddAppId(kTestAppId2);

  std::vector<std::string> app_id_list = app_info.GetAppIdList();

  EXPECT_EQ(std::string(kTestAppId1), app_id_list[0]);
  EXPECT_EQ(std::string(kTestAppId2), app_id_list[1]);
}

TEST_F(SyncedNotificationAppInfoTest, OnFetchCompleteTest) {
  StubSyncedNotificationAppInfoService
      stub_synced_notification_app_info_service(NULL);
  SyncedNotificationAppInfo app_info(
      NULL,
      kTestSendingServiceName,
      &stub_synced_notification_app_info_service);

  app_info.OnFetchComplete();

  // Expect that we reported the fetches all done to the owning service.
  EXPECT_TRUE(stub_synced_notification_app_info_service.
              on_bitmap_fetches_done_called());

}

TEST_F(SyncedNotificationAppInfoTest, AreAllBitmapsFetchedTest) {
  SyncedNotificationAppInfo app_info(NULL, kTestSendingServiceName, NULL);

  // Before we have any images to fetch, we should report all fetching is done.
  EXPECT_TRUE(app_info.AreAllBitmapsFetched());

  // Add some bitmaps to fetch, we should report fetching is not done.
  app_info.SetSettingsURLs(GURL(kIconUrl1), GURL(kIconUrl2));
  EXPECT_FALSE(app_info.AreAllBitmapsFetched());

  // Put a real bitmap into "bitmap".  2x2 bitmap of green 32 bit pixels.
  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kARGB_8888_Config, 2, 2);
  bitmap.allocPixels();
  bitmap.eraseColor(SK_ColorGREEN);

  // Now put in one bitmap, we are not done yet.
  app_info.settings_holder_->OnFetchComplete(GURL(kIconUrl1), &bitmap);
  EXPECT_FALSE(app_info.AreAllBitmapsFetched());

  // Add a second bitmap, and now we should report done.
  app_info.settings_holder_->OnFetchComplete(GURL(kIconUrl2), &bitmap);
  EXPECT_TRUE(app_info.AreAllBitmapsFetched());
}

}  // namespace notifier
