// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notifications/platform_notification_context.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "content/browser/notifications/notification_database_data.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

class PlatformNotificationContextTest : public ::testing::Test {
 public:
  PlatformNotificationContextTest()
      : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP),
        success_(false) {}

  // Callback to provide when reading a single notification from the database.
  void DidReadNotificationData(
      bool success, const NotificationDatabaseData& database_data) {
    success_ = success;
    database_data_ = database_data;
  }

  // Callback to provide when writing a notification to the database.
  void DidWriteNotificationData(bool success, int64_t notification_id) {
    success_ = success;
    notification_id_ = notification_id;
  }

  // Callback to provide when deleting notification data from the database.
  void DidDeleteNotificationData(bool success) {
    success_ = success;
  }

 protected:
  // Creates a new PlatformNotificationContext instance. When using this method,
  // the underlying database will always be created in memory. The current
  // message loop proxy will be used as the task runner.
  PlatformNotificationContext* CreatePlatformNotificationContext() {
    PlatformNotificationContext* context =
        new PlatformNotificationContext(base::FilePath());

    context->SetTaskRunnerForTesting(base::MessageLoopProxy::current());
    return context;
  }

  // Returns whether the last invoked callback finished successfully.
  bool success() const { return success_; }

  // Returns the NotificationDatabaseData associated with the last invoked
  // ReadNotificationData callback.
  const NotificationDatabaseData& database_data() const {
    return database_data_;
  }

  // Returns the notification id of the notification last written.
  int64_t notification_id() const { return notification_id_; }

 private:
  TestBrowserThreadBundle thread_bundle_;

  bool success_;
  NotificationDatabaseData database_data_;
  int64_t notification_id_;
};

TEST_F(PlatformNotificationContextTest, ReadNonExistentNotification) {
  scoped_refptr<PlatformNotificationContext> context =
      CreatePlatformNotificationContext();

  context->ReadNotificationData(
      42 /* notification_id */,
      GURL("https://example.com"),
      base::Bind(&PlatformNotificationContextTest::DidReadNotificationData,
                 base::Unretained(this)));

  base::RunLoop().RunUntilIdle();

  // The read operation should have failed, as it does not exist.
  ASSERT_FALSE(success());
}

TEST_F(PlatformNotificationContextTest, WriteReadNotification) {
  scoped_refptr<PlatformNotificationContext> context =
      CreatePlatformNotificationContext();

  GURL origin("https://example.com");
  NotificationDatabaseData notification_database_data;
  notification_database_data.origin = origin;

  context->WriteNotificationData(
      origin,
      notification_database_data,
      base::Bind(&PlatformNotificationContextTest::DidWriteNotificationData,
                 base::Unretained(this)));

  base::RunLoop().RunUntilIdle();

  // The write operation should have succeeded with a notification id.
  ASSERT_TRUE(success());
  EXPECT_GT(notification_id(), 0);

  context->ReadNotificationData(
      notification_id(),
      origin,
      base::Bind(&PlatformNotificationContextTest::DidReadNotificationData,
                 base::Unretained(this)));

  base::RunLoop().RunUntilIdle();

  // The read operation should have succeeded, with the right notification.
  ASSERT_TRUE(success());

  const NotificationDatabaseData& read_database_data = database_data();
  EXPECT_EQ(notification_database_data.origin, read_database_data.origin);
}

TEST_F(PlatformNotificationContextTest, DeleteInvalidNotification) {
  scoped_refptr<PlatformNotificationContext> context =
      CreatePlatformNotificationContext();

  context->DeleteNotificationData(
      42 /* notification_id */,
      GURL("https://example.com"),
      base::Bind(&PlatformNotificationContextTest::DidDeleteNotificationData,
                 base::Unretained(this)));

  base::RunLoop().RunUntilIdle();

  // The notification may not have existed, but since the goal of deleting data
  // is to make sure that it's gone, the goal has been satisfied. As such,
  // deleting a non-existent notification is considered to be a success.
  EXPECT_TRUE(success());
}

TEST_F(PlatformNotificationContextTest, DeleteNotification) {
  scoped_refptr<PlatformNotificationContext> context =
      CreatePlatformNotificationContext();

  GURL origin("https://example.com");
  NotificationDatabaseData notification_database_data;

  context->WriteNotificationData(
      origin,
      notification_database_data,
      base::Bind(&PlatformNotificationContextTest::DidWriteNotificationData,
                 base::Unretained(this)));

  base::RunLoop().RunUntilIdle();

  // The write operation should have succeeded with a notification id.
  ASSERT_TRUE(success());
  EXPECT_GT(notification_id(), 0);

  context->DeleteNotificationData(
      notification_id(),
      origin,
      base::Bind(&PlatformNotificationContextTest::DidDeleteNotificationData,
                 base::Unretained(this)));

  base::RunLoop().RunUntilIdle();

  // The notification existed, so it should have been removed successfully.
  ASSERT_TRUE(success());

  context->ReadNotificationData(
      notification_id(),
      origin,
      base::Bind(&PlatformNotificationContextTest::DidReadNotificationData,
                 base::Unretained(this)));

  base::RunLoop().RunUntilIdle();

  // The notification was removed, so we shouldn't be able to read it from
  // the database anymore.
  EXPECT_FALSE(success());
}

}  // namespace content
