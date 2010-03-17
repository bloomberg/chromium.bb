// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/notifications/desktop_notifications_unittest.h"

namespace chromeos {

// static
std::string DesktopNotificationsTest::log_output_;

class MockNotificationUI : public BalloonCollectionImpl::NotificationUI {
 public:
  virtual void Add(Balloon* balloon) {}
  virtual bool Update(Balloon* balloon) { return false; }
  virtual void Remove(Balloon* balloon) {}
  virtual void ResizeNotification(Balloon* balloon,
                                  const gfx::Size& size) {}
};

MockBalloonCollection::MockBalloonCollection()
    : log_proxy_(new LoggingNotificationProxy()) {
  set_notification_ui(new MockNotificationUI());
}

void MockBalloonCollection::Add(const Notification& notification,
                                Profile* profile) {
  // Swap in the logging proxy for the purpose of logging calls that
  // would be made into javascript, then pass this down to the
  // balloon collection.
  Notification test_notification(notification.origin_url(),
                                 notification.content_url(),
                                 notification.display_source(),
                                 log_proxy_.get());
  BalloonCollectionImpl::Add(test_notification, profile);
}

bool MockBalloonCollection::Remove(const Notification& notification) {
  Notification test_notification(notification.origin_url(),
                                 notification.content_url(),
                                 notification.display_source(),
                                 log_proxy_.get());
  return BalloonCollectionImpl::Remove(test_notification);
}

Balloon* MockBalloonCollection::MakeBalloon(const Notification& notification,
                                            Profile* profile) {
  // Start with a normal balloon but mock out the view.
  Balloon* balloon = BalloonCollectionImpl::MakeBalloon(notification, profile);
  balloon->set_view(new MockBalloonView(balloon));
  balloons_.insert(balloon);
  return balloon;
}

void MockBalloonCollection::OnBalloonClosed(Balloon* source) {
  balloons_.erase(source);
  BalloonCollectionImpl::OnBalloonClosed(source);
}

int MockBalloonCollection::UppermostVerticalPosition() {
  int min = 0;
  std::set<Balloon*>::iterator iter;
  for (iter = balloons_.begin(); iter != balloons_.end(); ++iter) {
    int pos = (*iter)->position().y();
    if (iter == balloons_.begin() || pos < min)
      min = pos;
  }
  return min;
}

DesktopNotificationsTest::DesktopNotificationsTest()
    : ui_thread_(ChromeThread::UI, &message_loop_) {
}

DesktopNotificationsTest::~DesktopNotificationsTest() {
}

void DesktopNotificationsTest::SetUp() {
  profile_.reset(new TestingProfile());
  balloon_collection_ = new MockBalloonCollection();
  ui_manager_.reset(new NotificationUIManager());
  ui_manager_->Initialize(balloon_collection_);
  balloon_collection_->set_space_change_listener(ui_manager_.get());
  service_.reset(new DesktopNotificationService(profile(), ui_manager_.get()));
  log_output_.clear();
}

void DesktopNotificationsTest::TearDown() {
  profile_.reset(NULL);
  ui_manager_.reset(NULL);
  service_.reset(NULL);
}

TEST_F(DesktopNotificationsTest, TestShow) {
  EXPECT_TRUE(service_->ShowDesktopNotificationText(
      GURL("http://www.google.com"),
      GURL("/icon.png"), ASCIIToUTF16("Title"), ASCIIToUTF16("Text"),
      0, 0, DesktopNotificationService::PageNotification, 1));
  MessageLoopForUI::current()->RunAllPending();
  EXPECT_EQ(1, balloon_collection_->count());

  EXPECT_TRUE(service_->ShowDesktopNotification(
      GURL("http://www.google.com"),
      GURL("http://www.google.com/notification.html"),
      0, 0, DesktopNotificationService::PageNotification, 2));
  MessageLoopForUI::current()->RunAllPending();
  EXPECT_EQ(2, balloon_collection_->count());

  EXPECT_EQ("notification displayed\n"
            "notification displayed\n",
            log_output_);
}

TEST_F(DesktopNotificationsTest, TestClose) {
  // Request a notification; should open a balloon.
  EXPECT_TRUE(service_->ShowDesktopNotificationText(
      GURL("http://www.google.com"),
      GURL("/icon.png"), ASCIIToUTF16("Title"), ASCIIToUTF16("Text"),
      0, 0, DesktopNotificationService::PageNotification, 1));
  MessageLoopForUI::current()->RunAllPending();
  EXPECT_EQ(1, balloon_collection_->count());

  // Close all the open balloons.
  std::set<Balloon*> balloons = balloon_collection_->balloons();
  std::set<Balloon*>::iterator iter;
  for (iter = balloons.begin(); iter != balloons.end(); ++iter) {
    (*iter)->OnClose(true);
  }

  // Verify that the balloon collection is now empty.
  EXPECT_EQ(0, balloon_collection_->count());

  EXPECT_EQ("notification displayed\n"
            "notification closed by user\n",
            log_output_);
}

TEST_F(DesktopNotificationsTest, TestCancel) {
  int process_id = 0;
  int route_id = 0;
  int notification_id = 1;
  // Request a notification; should open a balloon.
  EXPECT_TRUE(service_->ShowDesktopNotificationText(
      GURL("http://www.google.com"),
      GURL("/icon.png"), ASCIIToUTF16("Title"), ASCIIToUTF16("Text"),
      process_id, route_id, DesktopNotificationService::PageNotification,
      notification_id));
  MessageLoopForUI::current()->RunAllPending();
  EXPECT_EQ(1, balloon_collection_->count());

  // Cancel the same notification
  service_->CancelDesktopNotification(process_id,
                                      route_id,
                                      notification_id);
  MessageLoopForUI::current()->RunAllPending();
  // Verify that the balloon collection is now empty.
  EXPECT_EQ(0, balloon_collection_->count());

  EXPECT_EQ("notification displayed\n"
            "notification closed by script\n",
            log_output_);
}

TEST_F(DesktopNotificationsTest, TestManyNotifications) {
  int process_id = 0;
  int route_id = 0;

  // Request lots of identical notifications.
  const int kLotsOfToasts = 20;
  for (int id = 1; id <= kLotsOfToasts; ++id) {
    SCOPED_TRACE(StringPrintf("Creation loop: id=%d", id));
    EXPECT_TRUE(service_->ShowDesktopNotificationText(
        GURL("http://www.google.com"),
        GURL("/icon.png"), ASCIIToUTF16("Title"), ASCIIToUTF16("Text"),
        process_id, route_id,
        DesktopNotificationService::PageNotification, id));
  }
  MessageLoopForUI::current()->RunAllPending();

  // Build up an expected log of what should be happening.
  std::string expected_log;
  for (int i = 0; i < kLotsOfToasts; ++i) {
    expected_log.append("notification displayed\n");
  }

  EXPECT_EQ(kLotsOfToasts, balloon_collection_->count());
  EXPECT_EQ(expected_log, log_output_);

  // Cancel half of the notifications from the start
  int id;
  int cancelled = kLotsOfToasts / 2;
  for (id = 1;
       id <= cancelled;
       ++id) {
    SCOPED_TRACE(StringPrintf("Cancel half of notifications: id=%d", id));
    service_->CancelDesktopNotification(process_id, route_id, id);
    MessageLoopForUI::current()->RunAllPending();
    expected_log.append("notification closed by script\n");
    EXPECT_EQ(kLotsOfToasts - id,
              balloon_collection_->count());
    EXPECT_EQ(expected_log, log_output_);
  }

  // Now cancel the rest.  It should empty the balloon space.
  for (; id <= kLotsOfToasts; ++id) {
    SCOPED_TRACE(StringPrintf("Cancel loop: id=%d", id));
    service_->CancelDesktopNotification(process_id, route_id, id);
    expected_log.append("notification closed by script\n");
    MessageLoopForUI::current()->RunAllPending();
    EXPECT_EQ(expected_log, log_output_);
  }

  // Verify that the balloon collection is now empty.
  EXPECT_EQ(0, balloon_collection_->count());
}

TEST_F(DesktopNotificationsTest, TestEarlyDestruction) {
  // Create some toasts and then prematurely delete the notification service,
  // just to make sure nothing crashes/leaks.
  for (int id = 0; id <= 3; ++id) {
    SCOPED_TRACE(StringPrintf("Show Text loop: id=%d", id));

    EXPECT_TRUE(service_->ShowDesktopNotificationText(
        GURL("http://www.google.com"),
        GURL("/icon.png"), ASCIIToUTF16("Title"), ASCIIToUTF16("Text"),
        0, 0, DesktopNotificationService::PageNotification, id));
  }
  service_.reset(NULL);
}

TEST_F(DesktopNotificationsTest, TestUserInputEscaping) {
  // Create a test script with some HTML; assert that it doesn't get into the
  // data:// URL that's produced for the balloon.
  EXPECT_TRUE(service_->ShowDesktopNotificationText(
      GURL("http://www.google.com"),
      GURL("/icon.png"),
      ASCIIToUTF16("<script>window.alert('uh oh');</script>"),
      ASCIIToUTF16("<i>this text is in italics</i>"),
      0, 0, DesktopNotificationService::PageNotification, 1));

  MessageLoopForUI::current()->RunAllPending();
  EXPECT_EQ(1, balloon_collection_->count());
  Balloon* balloon = (*balloon_collection_->balloons().begin());
  GURL data_url = balloon->notification().content_url();
  EXPECT_EQ(std::string::npos, data_url.spec().find("<script>"));
  EXPECT_EQ(std::string::npos, data_url.spec().find("<i>"));
}

}  // namespace chromeos
