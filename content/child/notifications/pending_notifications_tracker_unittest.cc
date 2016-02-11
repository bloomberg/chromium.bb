// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/notifications/pending_notifications_tracker.h"

#include <vector>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/thread_task_runner_handle.h"
#include "content/public/common/notification_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/Platform.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "third_party/WebKit/public/platform/WebUnitTestSupport.h"
#include "third_party/WebKit/public/platform/modules/notifications/WebNotificationData.h"
#include "third_party/WebKit/public/platform/modules/notifications/WebNotificationDelegate.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

namespace content {

namespace {

const char kBaseUrl[] = "http://test.com/";
const char kIcon100x100[] = "100x100.png";
const char kIcon110x110[] = "110x110.png";
const char kIcon120x120[] = "120x120.png";

class FakeNotificationDelegate : public blink::WebNotificationDelegate {
 public:
  void dispatchClickEvent() override {}
  void dispatchShowEvent() override {}
  void dispatchErrorEvent() override {}
  void dispatchCloseEvent() override {}
};

}  // namespace

class PendingNotificationsTrackerTest : public testing::Test {
 public:
  PendingNotificationsTrackerTest()
      : tracker_(new PendingNotificationsTracker(
            base::ThreadTaskRunnerHandle::Get())) {}

  ~PendingNotificationsTrackerTest() override {
    UnitTestSupport()->unregisterAllMockedURLs();
  }

  void DidFetchResources(size_t index, const NotificationResources& resources) {
    if (resources_.size() < index + 1)
      resources_.resize(index + 1);
    resources_[index] = resources;
  }

 protected:
  PendingNotificationsTracker* tracker() { return tracker_.get(); }

  size_t CountResources() { return resources_.size(); }

  NotificationResources* GetResources(size_t index) {
    DCHECK_LT(index, resources_.size());
    return &resources_[index];
  }

  size_t CountPendingNotifications() {
    return tracker_->pending_notifications_.size();
  }

  size_t CountDelegates() {
    return tracker_->delegate_to_pending_id_map_.size();
  }

  blink::WebUnitTestSupport* UnitTestSupport() {
    return blink::Platform::current()->unitTestSupport();
  }

  // Registers a mocked url. When fetched, |file_name| will be loaded from the
  // test data directory.
  blink::WebURL RegisterMockedURL(const std::string& file_name) {
    blink::WebURL url(GURL(kBaseUrl + file_name));

    blink::WebURLResponse response(url);
    response.setMIMEType("image/png");
    response.setHTTPStatusCode(200);

    base::FilePath file_path;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &file_path);
    file_path = file_path.Append(FILE_PATH_LITERAL("content"))
                    .Append(FILE_PATH_LITERAL("test"))
                    .Append(FILE_PATH_LITERAL("data"))
                    .Append(FILE_PATH_LITERAL("notifications"))
                    .AppendASCII(file_name);
    file_path = base::MakeAbsoluteFilePath(file_path);
    blink::WebString string_file_path =
        blink::WebString::fromUTF8(file_path.MaybeAsASCII());

    UnitTestSupport()->registerMockedURL(url, response, string_file_path);

    return url;
  }

  // Registers a mocked url that will fail to be fetched, with a 404 error.
  blink::WebURL RegisterMockedErrorURL(const std::string& file_name) {
    blink::WebURL url(GURL(kBaseUrl + file_name));

    blink::WebURLResponse response(url);
    response.setMIMEType("image/png");
    response.setHTTPStatusCode(404);

    blink::WebURLError error;
    error.reason = 404;

    UnitTestSupport()->registerMockedErrorURL(url, response, error);
    return url;
  }

 private:
  base::MessageLoop message_loop_;
  scoped_ptr<PendingNotificationsTracker> tracker_;
  std::vector<NotificationResources> resources_;

  DISALLOW_COPY_AND_ASSIGN(PendingNotificationsTrackerTest);
};

TEST_F(PendingNotificationsTrackerTest, OneNotificationMultipleResources) {
  blink::WebNotificationData notification_data;
  notification_data.icon = RegisterMockedURL(kIcon100x100);
  notification_data.actions =
      blink::WebVector<blink::WebNotificationAction>(static_cast<size_t>(2));
  notification_data.actions[0].icon = RegisterMockedURL(kIcon110x110);
  notification_data.actions[1].icon = RegisterMockedURL(kIcon120x120);

  tracker()->FetchResources(
      notification_data, nullptr /* delegate */,
      base::Bind(&PendingNotificationsTrackerTest::DidFetchResources,
                 base::Unretained(this), 0 /* index */));

  ASSERT_EQ(1u, CountPendingNotifications());
  ASSERT_EQ(0u, CountResources());

  base::RunLoop().RunUntilIdle();
  UnitTestSupport()->serveAsynchronousMockedRequests();

  ASSERT_EQ(0u, CountPendingNotifications());
  ASSERT_EQ(1u, CountResources());

  ASSERT_FALSE(GetResources(0u)->notification_icon.drawsNothing());
  ASSERT_EQ(100, GetResources(0u)->notification_icon.width());

  ASSERT_EQ(2u, GetResources(0u)->action_icons.size());
  ASSERT_FALSE(GetResources(0u)->action_icons[0].drawsNothing());
  ASSERT_EQ(110, GetResources(0u)->action_icons[0].width());
  ASSERT_FALSE(GetResources(0u)->action_icons[1].drawsNothing());
  ASSERT_EQ(120, GetResources(0u)->action_icons[1].width());
}

TEST_F(PendingNotificationsTrackerTest, TwoNotifications) {
  blink::WebNotificationData notification_data;
  notification_data.icon = RegisterMockedURL(kIcon100x100);

  blink::WebNotificationData notification_data_2;
  notification_data_2.icon = RegisterMockedURL(kIcon110x110);

  tracker()->FetchResources(
      notification_data, nullptr /* delegate */,
      base::Bind(&PendingNotificationsTrackerTest::DidFetchResources,
                 base::Unretained(this), 0 /* index */));

  tracker()->FetchResources(
      notification_data_2, nullptr /* delegate */,
      base::Bind(&PendingNotificationsTrackerTest::DidFetchResources,
                 base::Unretained(this), 1 /* index */));

  ASSERT_EQ(2u, CountPendingNotifications());
  ASSERT_EQ(0u, CountResources());

  base::RunLoop().RunUntilIdle();
  UnitTestSupport()->serveAsynchronousMockedRequests();

  ASSERT_EQ(0u, CountPendingNotifications());
  ASSERT_EQ(2u, CountResources());

  ASSERT_FALSE(GetResources(0u)->notification_icon.drawsNothing());
  ASSERT_EQ(100, GetResources(0u)->notification_icon.width());

  ASSERT_FALSE(GetResources(1u)->notification_icon.drawsNothing());
  ASSERT_EQ(110, GetResources(1u)->notification_icon.width());
}

TEST_F(PendingNotificationsTrackerTest, FetchError) {
  blink::WebNotificationData notification_data;
  notification_data.icon = RegisterMockedErrorURL(kIcon100x100);

  tracker()->FetchResources(
      notification_data, nullptr /* delegate */,
      base::Bind(&PendingNotificationsTrackerTest::DidFetchResources,
                 base::Unretained(this), 0 /* index */));

  ASSERT_EQ(1u, CountPendingNotifications());
  ASSERT_EQ(0u, CountResources());

  base::RunLoop().RunUntilIdle();
  UnitTestSupport()->serveAsynchronousMockedRequests();

  ASSERT_EQ(0u, CountPendingNotifications());
  ASSERT_EQ(1u, CountResources());

  ASSERT_TRUE(GetResources(0u)->notification_icon.drawsNothing());
}

TEST_F(PendingNotificationsTrackerTest, CancelFetches) {
  blink::WebNotificationData notification_data;
  notification_data.icon = RegisterMockedURL(kIcon100x100);

  FakeNotificationDelegate delegate;

  tracker()->FetchResources(
      notification_data, &delegate,
      base::Bind(&PendingNotificationsTrackerTest::DidFetchResources,
                 base::Unretained(this), 0 /* index */));

  ASSERT_EQ(1u, CountPendingNotifications());
  ASSERT_EQ(1u, CountDelegates());
  ASSERT_EQ(0u, CountResources());

  base::RunLoop().RunUntilIdle();
  tracker()->CancelResourceFetches(&delegate);

  ASSERT_EQ(0u, CountPendingNotifications());
  ASSERT_EQ(0u, CountDelegates());
  ASSERT_EQ(0u, CountResources());
}

}  // namespace content
