// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/desktop_notifications_unittest.h"

#include "base/prefs/testing_pref_service.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/notifications/balloon_notification_ui_manager.h"
#include "chrome/browser/notifications/fake_balloon_view.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/common/pref_names.h"
#include "content/public/common/show_desktop_notification_params.h"

#if defined(USE_ASH)
#include "ash/shell.h"
#include "ash/test/test_shell_delegate.h"
#include "chrome/browser/ui/aura/active_desktop_monitor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#endif

using content::BrowserThread;

// static
const int MockBalloonCollection::kMockBalloonSpace = 5;

// static
std::string DesktopNotificationsTest::log_output_;

MockBalloonCollection::MockBalloonCollection() {}

MockBalloonCollection::~MockBalloonCollection() {}

void MockBalloonCollection::Add(const Notification& notification,
                                Profile* profile) {
  // Swap in a logging proxy for the purpose of logging calls that
  // would be made into javascript, then pass this down to the
  // balloon collection.
  Notification test_notification(
      notification.origin_url(),
      notification.content_url(),
      notification.display_source(),
      notification.replace_id(),
      new LoggingNotificationProxy(notification.notification_id()));
  BalloonCollectionImpl::Add(test_notification, profile);
}

bool MockBalloonCollection::HasSpace() const {
  return count() < kMockBalloonSpace;
}

Balloon* MockBalloonCollection::MakeBalloon(const Notification& notification,
                                            Profile* profile) {
  // Start with a normal balloon but mock out the view.
  Balloon* balloon = BalloonCollectionImpl::MakeBalloon(notification, profile);
  balloon->set_view(new FakeBalloonView(balloon));
  balloons_.push_back(balloon);
  return balloon;
}

void MockBalloonCollection::OnBalloonClosed(Balloon* source) {
  std::deque<Balloon*>::iterator it;
  for (it = balloons_.begin(); it != balloons_.end(); ++it) {
    if (*it == source) {
      balloons_.erase(it);
      BalloonCollectionImpl::OnBalloonClosed(source);
      break;
    }
  }
}

const BalloonCollection::Balloons& MockBalloonCollection::GetActiveBalloons() {
  return balloons_;
}

int MockBalloonCollection::UppermostVerticalPosition() {
  int min = 0;
  std::deque<Balloon*>::iterator iter;
  for (iter = balloons_.begin(); iter != balloons_.end(); ++iter) {
    int pos = (*iter)->GetPosition().y();
    if (iter == balloons_.begin() || pos < min)
      min = pos;
  }
  return min;
}

DesktopNotificationsTest::DesktopNotificationsTest()
    : ui_thread_(BrowserThread::UI, &message_loop_) {
}

DesktopNotificationsTest::~DesktopNotificationsTest() {
}

void DesktopNotificationsTest::SetUp() {
#if defined(USE_ASH)
  WebKit::initialize(webkit_platform_support_.Get());
  // MockBalloonCollection retrieves information about the screen on creation.
  // So it is necessary to make sure the desktop gets created first.
  ash::Shell::CreateInstance(new ash::test::TestShellDelegate);
  active_desktop_monitor_.reset(new ActiveDesktopMonitor);
#endif

  chrome::RegisterLocalState(local_state_.registry());
  profile_.reset(new TestingProfile());
  ui_manager_.reset(new BalloonNotificationUIManager(&local_state_));
  balloon_collection_ = new MockBalloonCollection();
  ui_manager_->SetBalloonCollection(balloon_collection_);
  service_.reset(new DesktopNotificationService(profile(), ui_manager_.get()));
  log_output_.clear();
}

void DesktopNotificationsTest::TearDown() {
  service_.reset(NULL);
  ui_manager_.reset(NULL);
  profile_.reset(NULL);
#if defined(USE_ASH)
  active_desktop_monitor_.reset();
  ash::Shell::DeleteInstance();
  aura::Env::DeleteInstance();
  WebKit::shutdown();
#endif
}

content::ShowDesktopNotificationHostMsgParams
DesktopNotificationsTest::StandardTestNotification() {
  content::ShowDesktopNotificationHostMsgParams params;
  params.notification_id = 0;
  params.origin = GURL("http://www.google.com");
  params.is_html = false;
  params.icon_url = GURL("/icon.png");
  params.title = ASCIIToUTF16("Title");
  params.body = ASCIIToUTF16("Text");
  params.direction = WebKit::WebTextDirectionDefault;
  return params;
}

TEST_F(DesktopNotificationsTest, TestShow) {
  content::ShowDesktopNotificationHostMsgParams params =
      StandardTestNotification();
  params.notification_id = 1;

  EXPECT_TRUE(service_->ShowDesktopNotification(
      params, 0, 0, DesktopNotificationService::PageNotification));
  MessageLoopForUI::current()->RunUntilIdle();
  EXPECT_EQ(1, balloon_collection_->count());

  content::ShowDesktopNotificationHostMsgParams params2;
  params2.origin = GURL("http://www.google.com");
  params2.is_html = true;
  params2.contents_url = GURL("http://www.google.com/notification.html");
  params2.notification_id = 2;

  EXPECT_TRUE(service_->ShowDesktopNotification(
      params2, 0, 0, DesktopNotificationService::PageNotification));
  MessageLoopForUI::current()->RunUntilIdle();
  EXPECT_EQ(2, balloon_collection_->count());

  EXPECT_EQ("notification displayed\n"
            "notification displayed\n",
            log_output_);
}

TEST_F(DesktopNotificationsTest, TestClose) {
  content::ShowDesktopNotificationHostMsgParams params =
      StandardTestNotification();
  params.notification_id = 1;

  // Request a notification; should open a balloon.
  EXPECT_TRUE(service_->ShowDesktopNotification(
      params, 0, 0, DesktopNotificationService::PageNotification));
  MessageLoopForUI::current()->RunUntilIdle();
  EXPECT_EQ(1, balloon_collection_->count());

  // Close all the open balloons.
  while (balloon_collection_->count() > 0) {
    (*(balloon_collection_->GetActiveBalloons().begin()))->OnClose(true);
  }

  EXPECT_EQ("notification displayed\n"
            "notification closed by user\n",
            log_output_);
}

TEST_F(DesktopNotificationsTest, TestCancel) {
  int process_id = 0;
  int route_id = 0;
  int notification_id = 1;

  content::ShowDesktopNotificationHostMsgParams params =
      StandardTestNotification();
  params.notification_id = notification_id;

  // Request a notification; should open a balloon.
  EXPECT_TRUE(service_->ShowDesktopNotification(
      params, process_id, route_id,
      DesktopNotificationService::PageNotification));
  MessageLoopForUI::current()->RunUntilIdle();
  EXPECT_EQ(1, balloon_collection_->count());

  // Cancel the same notification
  service_->CancelDesktopNotification(process_id,
                                      route_id,
                                      notification_id);
  MessageLoopForUI::current()->RunUntilIdle();
  // Verify that the balloon collection is now empty.
  EXPECT_EQ(0, balloon_collection_->count());

  EXPECT_EQ("notification displayed\n"
            "notification closed by script\n",
            log_output_);
}

#if defined(OS_WIN) || defined(TOOLKIT_VIEWS)
TEST_F(DesktopNotificationsTest, TestPositioning) {
  content::ShowDesktopNotificationHostMsgParams params =
      StandardTestNotification();
  std::string expected_log;
  // Create some toasts.  After each but the first, make sure there
  // is a minimum separation between the toasts.
  int last_top = 0;
  for (int id = 0; id <= 3; ++id) {
    params.notification_id = id;
    EXPECT_TRUE(service_->ShowDesktopNotification(
        params, 0, 0, DesktopNotificationService::PageNotification));
    expected_log.append("notification displayed\n");
    int top = balloon_collection_->UppermostVerticalPosition();
    if (id > 0)
      EXPECT_LE(top, last_top - balloon_collection_->MinHeight());
    last_top = top;
  }

  EXPECT_EQ(expected_log, log_output_);
}

TEST_F(DesktopNotificationsTest, TestVariableSize) {
  content::ShowDesktopNotificationHostMsgParams params;
  params.origin = GURL("http://long.google.com");
  params.is_html = false;
  params.icon_url = GURL("/icon.png");
  params.title = ASCIIToUTF16("Really Really Really Really Really Really "
                              "Really Really Really Really Really Really "
                              "Really Really Really Really Really Really "
                              "Really Long Title"),
  params.body = ASCIIToUTF16("Text");
  params.notification_id = 0;

  std::string expected_log;
  // Create some toasts.  After each but the first, make sure there
  // is a minimum separation between the toasts.
  EXPECT_TRUE(service_->ShowDesktopNotification(
      params, 0, 0, DesktopNotificationService::PageNotification));
  expected_log.append("notification displayed\n");

  params.origin = GURL("http://short.google.com");
  params.title = ASCIIToUTF16("Short title");
  params.notification_id = 1;
  EXPECT_TRUE(service_->ShowDesktopNotification(
      params, 0, 0, DesktopNotificationService::PageNotification));
  expected_log.append("notification displayed\n");

  std::deque<Balloon*>& balloons = balloon_collection_->balloons();
  std::deque<Balloon*>::iterator iter;
  for (iter = balloons.begin(); iter != balloons.end(); ++iter) {
    if ((*iter)->notification().origin_url().host() == "long.google.com") {
      EXPECT_GE((*iter)->GetViewSize().height(),
                balloon_collection_->MinHeight());
      EXPECT_LE((*iter)->GetViewSize().height(),
                balloon_collection_->MaxHeight());
    } else {
      EXPECT_EQ((*iter)->GetViewSize().height(),
                balloon_collection_->MinHeight());
    }
  }
  EXPECT_EQ(expected_log, log_output_);
}
#endif

TEST_F(DesktopNotificationsTest, TestQueueing) {
  int process_id = 0;
  int route_id = 0;

  // Request lots of identical notifications.
  content::ShowDesktopNotificationHostMsgParams params =
      StandardTestNotification();
  const int kLotsOfToasts = 20;
  for (int id = 1; id <= kLotsOfToasts; ++id) {
    params.notification_id = id;
    EXPECT_TRUE(service_->ShowDesktopNotification(
        params, process_id, route_id,
        DesktopNotificationService::PageNotification));
  }
  MessageLoopForUI::current()->RunUntilIdle();

  // Build up an expected log of what should be happening.
  std::string expected_log;
  for (int i = 0; i < balloon_collection_->max_balloon_count(); ++i) {
    expected_log.append("notification displayed\n");
  }

  // The max number that our balloon collection can hold should be
  // shown.
  EXPECT_EQ(balloon_collection_->max_balloon_count(),
            balloon_collection_->count());
  EXPECT_EQ(expected_log, log_output_);

  // Cancel the notifications from the start; the balloon space should
  // remain full.
  {
    int id;
    for (id = 1;
         id <= kLotsOfToasts - balloon_collection_->max_balloon_count();
         ++id) {
      service_->CancelDesktopNotification(process_id, route_id, id);
      MessageLoopForUI::current()->RunUntilIdle();
      expected_log.append("notification closed by script\n");
      expected_log.append("notification displayed\n");
      EXPECT_EQ(balloon_collection_->max_balloon_count(),
                balloon_collection_->count());
      EXPECT_EQ(expected_log, log_output_);
    }

    // Now cancel the rest.  It should empty the balloon space.
    for (; id <= kLotsOfToasts; ++id) {
      service_->CancelDesktopNotification(process_id, route_id, id);
      expected_log.append("notification closed by script\n");
      MessageLoopForUI::current()->RunUntilIdle();
      EXPECT_EQ(expected_log, log_output_);
    }
  }

  // Verify that the balloon collection is now empty.
  EXPECT_EQ(0, balloon_collection_->count());
}

TEST_F(DesktopNotificationsTest, TestEarlyDestruction) {
  // Create some toasts and then prematurely delete the notification service,
  // just to make sure nothing crashes/leaks.
  content::ShowDesktopNotificationHostMsgParams params =
      StandardTestNotification();
  for (int id = 0; id <= 3; ++id) {
    params.notification_id = id;
    EXPECT_TRUE(service_->ShowDesktopNotification(
        params, 0, 0, DesktopNotificationService::PageNotification));
  }
  service_.reset(NULL);
}

TEST_F(DesktopNotificationsTest, TestUserInputEscaping) {
  // Create a test script with some HTML; assert that it doesn't get into the
  // data:// URL that's produced for the balloon.
  content::ShowDesktopNotificationHostMsgParams params =
      StandardTestNotification();
  params.title = ASCIIToUTF16("<script>window.alert('uh oh');</script>");
  params.body = ASCIIToUTF16("<i>this text is in italics</i>");
  params.notification_id = 1;
  EXPECT_TRUE(service_->ShowDesktopNotification(
      params, 0, 0, DesktopNotificationService::PageNotification));

  MessageLoopForUI::current()->RunUntilIdle();
  EXPECT_EQ(1, balloon_collection_->count());
  Balloon* balloon = (*balloon_collection_->balloons().begin());
  GURL data_url = balloon->notification().content_url();
  EXPECT_EQ(std::string::npos, data_url.spec().find("<script>"));
  EXPECT_EQ(std::string::npos, data_url.spec().find("<i>"));
  // URL-encoded versions of tags should also not be found.
  EXPECT_EQ(std::string::npos, data_url.spec().find("%3cscript%3e"));
  EXPECT_EQ(std::string::npos, data_url.spec().find("%3ci%3e"));
}

TEST_F(DesktopNotificationsTest, TestBoundingBox) {
  // Create some notifications.
  content::ShowDesktopNotificationHostMsgParams params =
      StandardTestNotification();
  for (int id = 0; id <= 3; ++id) {
    params.notification_id = id;
    EXPECT_TRUE(service_->ShowDesktopNotification(
        params, 0, 0, DesktopNotificationService::PageNotification));
  }

  gfx::Rect box = balloon_collection_->GetBalloonsBoundingBox();

  // Try this for all positions.
  BalloonCollection::PositionPreference pref;
  for (pref = BalloonCollection::UPPER_RIGHT;
       pref <= BalloonCollection::LOWER_LEFT;
       pref = static_cast<BalloonCollection::PositionPreference>(pref + 1)) {
    // Make sure each balloon's 4 corners are inside the box.
    std::deque<Balloon*>& balloons = balloon_collection_->balloons();
    std::deque<Balloon*>::iterator iter;
    for (iter = balloons.begin(); iter != balloons.end(); ++iter) {
      int min_x = (*iter)->GetPosition().x();
      int max_x = min_x + (*iter)->GetViewSize().width() - 1;
      int min_y = (*iter)->GetPosition().y();
      int max_y = min_y + (*iter)->GetViewSize().height() - 1;

      EXPECT_TRUE(box.Contains(gfx::Point(min_x, min_y)));
      EXPECT_TRUE(box.Contains(gfx::Point(min_x, max_y)));
      EXPECT_TRUE(box.Contains(gfx::Point(max_x, min_y)));
      EXPECT_TRUE(box.Contains(gfx::Point(max_x, max_y)));
    }
  }
}

TEST_F(DesktopNotificationsTest, TestPositionPreference) {
  // Set position preference to lower right.
  local_state_.SetInteger(prefs::kDesktopNotificationPosition,
                          BalloonCollection::LOWER_RIGHT);

  // Create some notifications.
  content::ShowDesktopNotificationHostMsgParams params =
      StandardTestNotification();
  for (int id = 0; id <= 3; ++id) {
    params.notification_id = id;
    EXPECT_TRUE(service_->ShowDesktopNotification(
        params, 0, 0, DesktopNotificationService::PageNotification));
  }

  std::deque<Balloon*>& balloons = balloon_collection_->balloons();
  std::deque<Balloon*>::iterator iter;

  // Check that they decrease in y-position (for MAC, with reversed
  // coordinates, they should increase).
  int last_y = -1;
  int last_x = -1;

  for (iter = balloons.begin(); iter != balloons.end(); ++iter) {
    int current_x = (*iter)->GetPosition().x();
    int current_y = (*iter)->GetPosition().y();
    if (last_x > 0)
      EXPECT_EQ(last_x, current_x);

    if (last_y > 0) {
#if defined(OS_MACOSX)
      EXPECT_GT(current_y, last_y);
#else
      EXPECT_LT(current_y, last_y);
#endif
    }

    last_x = current_x;
    last_y = current_y;
  }

  // Now change the position to upper right.  This should cause an immediate
  // repositioning, and we check for the reverse ordering.
  local_state_.SetInteger(prefs::kDesktopNotificationPosition,
                          BalloonCollection::UPPER_RIGHT);
  last_x = -1;
  last_y = -1;

  for (iter = balloons.begin(); iter != balloons.end(); ++iter) {
    int current_x = (*iter)->GetPosition().x();
    int current_y = (*iter)->GetPosition().y();

    if (last_x > 0)
      EXPECT_EQ(last_x, current_x);

    if (last_y > 0) {
#if defined(OS_MACOSX)
      EXPECT_LT(current_y, last_y);
#else
      EXPECT_GT(current_y, last_y);
#endif
    }

    last_x = current_x;
    last_y = current_y;
  }

  // Now change the position to upper left.  Confirm that the X value for the
  // balloons gets smaller.
  local_state_.SetInteger(prefs::kDesktopNotificationPosition,
                          BalloonCollection::UPPER_LEFT);

  int current_x = (*balloons.begin())->GetPosition().x();
  EXPECT_LT(current_x, last_x);
}
