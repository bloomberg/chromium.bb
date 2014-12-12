// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/notifications/notification_test_util.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/desktop_notification_delegate.h"
#include "content/public/common/platform_notification_data.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace {

class MockDesktopNotificationDelegate
    : public content::DesktopNotificationDelegate {
 public:
  MockDesktopNotificationDelegate()
      : displayed_(false),
        clicked_(false) {}

  ~MockDesktopNotificationDelegate() override {}

  // content::DesktopNotificationDelegate implementation.
  void NotificationDisplayed() override { displayed_ = true; }
  void NotificationClosed(bool by_user) override {}
  void NotificationClick() override { clicked_ = true; }

  bool displayed() const { return displayed_; }
  bool clicked() const { return clicked_; }

 private:
  bool displayed_;
  bool clicked_;
};

}  // namespace

class PlatformNotificationServiceTest : public testing::Test {
 public:
  void SetUp() override {
    ui_manager_.reset(new StubNotificationUIManager);
    profile_.reset(new TestingProfile());

    service()->SetNotificationUIManagerForTesting(ui_manager_.get());
  }

  void TearDown() override {
    service()->SetNotificationUIManagerForTesting(nullptr);

    profile_.reset();
    ui_manager_.reset();
  }

 protected:
  // Displays a simple, fake notifications and returns a weak pointer to the
  // delegate receiving events for it (ownership is transferred to the service).
  MockDesktopNotificationDelegate* CreateSimplePageNotification() const {
    return CreateSimplePageNotificationWithCloseClosure(nullptr);
  }

  // Displays a simple, fake notification and returns a weak pointer to the
  // delegate receiving events for it (ownership is transferred to the service).
  // The close closure may be specified if so desired.
  MockDesktopNotificationDelegate* CreateSimplePageNotificationWithCloseClosure(
      base::Closure* close_closure) const {
    content::PlatformNotificationData notification_data;
    notification_data.title = base::ASCIIToUTF16("My Notification");
    notification_data.body = base::ASCIIToUTF16("Hello, world!");

    MockDesktopNotificationDelegate* delegate =
        new MockDesktopNotificationDelegate();

    service()->DisplayNotification(profile(),
                                   GURL("https://example.com/"),
                                   SkBitmap(),
                                   notification_data,
                                   make_scoped_ptr(delegate),
                                   0 /* render_process_id */,
                                   close_closure);

    return delegate;
  }

  // Returns the Platform Notification Service these unit tests are for.
  PlatformNotificationServiceImpl* service() const {
    return PlatformNotificationServiceImpl::GetInstance();
  }

  // Returns the Profile to be used for these tests.
  Profile* profile() const { return profile_.get(); }

  // Returns the UI Manager on which notifications will be displayed.
  StubNotificationUIManager* ui_manager() const { return ui_manager_.get(); }

 private:
  scoped_ptr<StubNotificationUIManager> ui_manager_;
  scoped_ptr<TestingProfile> profile_;

  content::TestBrowserThreadBundle thread_bundle_;
};

TEST_F(PlatformNotificationServiceTest, DisplayPageDisplayedEvent) {
  auto* delegate = CreateSimplePageNotification();

  EXPECT_EQ(1u, ui_manager()->GetNotificationCount());
  EXPECT_TRUE(delegate->displayed());
}

TEST_F(PlatformNotificationServiceTest, DisplayPageCloseClosure) {
  base::Closure close_closure;
  CreateSimplePageNotificationWithCloseClosure(&close_closure);

  EXPECT_EQ(1u, ui_manager()->GetNotificationCount());

  ASSERT_FALSE(close_closure.is_null());
  close_closure.Run();

  EXPECT_EQ(0u, ui_manager()->GetNotificationCount());

  // Note that we cannot verify whether the closed event was called on the
  // delegate given that it'd result in a use-after-free.
}

TEST_F(PlatformNotificationServiceTest, DisplayPageNotificationMatches) {
  content::PlatformNotificationData notification_data;
  notification_data.title = base::ASCIIToUTF16("My notification's title");
  notification_data.body = base::ASCIIToUTF16("Hello, world!");

  MockDesktopNotificationDelegate* delegate
      = new MockDesktopNotificationDelegate();
  service()->DisplayNotification(profile(),
                                 GURL("https://chrome.com/"),
                                 SkBitmap(),
                                 notification_data,
                                 make_scoped_ptr(delegate),
                                 0 /* render_process_id */,
                                 nullptr);

  ASSERT_EQ(1u, ui_manager()->GetNotificationCount());

  const Notification& notification = ui_manager()->GetNotificationAt(0);
  EXPECT_EQ("https://chrome.com/", notification.origin_url().spec());
  EXPECT_EQ("My notification's title",
      base::UTF16ToUTF8(notification.title()));
  EXPECT_EQ("Hello, world!",
      base::UTF16ToUTF8(notification.message()));
}

TEST_F(PlatformNotificationServiceTest, DisplayNameForOrigin) {
  base::string16 display_name =
      service()->DisplayNameForOriginInProcessId(profile(),
                                                 GURL("https://chrome.com/"),
                                                 0 /* render_process_id */);

  EXPECT_EQ(base::ASCIIToUTF16("chrome.com"), display_name);

  // TODO(peter): Include unit tests for the extension-name translation
  // functionality of DisplayNameForOriginInProcessId.
}
