// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/notifications/desktop_notifications_unittest.h"

#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/pref_service.h"
#include "content/public/common/show_desktop_notification_params.h"

#if defined(USE_AURA)
#include "chrome/browser/chromeos/notifications/balloon_collection_impl_aura.h"
#include "ui/aura/root_window.h"
#else
#include "chrome/browser/chromeos/notifications/balloon_collection_impl.h"
#endif

#if defined(USE_AURA)
typedef class chromeos::BalloonCollectionImplAura BalloonCollectionImplType;
#else
typedef class chromeos::BalloonCollectionImpl BalloonCollectionImplType;
#endif

using content::BrowserThread;

namespace chromeos {

// static
std::string DesktopNotificationsTest::log_output_;

class BalloonViewImpl;

#if !defined(USE_AURA)
class MockNotificationUI : public BalloonCollectionImplType::NotificationUI {
 public:
  virtual void Add(Balloon* balloon) {}
  virtual bool Update(Balloon* balloon) { return false; }
  virtual void Remove(Balloon* balloon) {}
  virtual void Show(Balloon* balloon) {}
  virtual void ResizeNotification(Balloon* balloon,
                                  const gfx::Size& size) {}
  virtual void SetActiveView(BalloonViewImpl* view) {}
};
#endif

// Test version of the balloon collection which counts the number
// of notifications that are added to it.
class MockBalloonCollection : public BalloonCollectionImplType {
 public:
  MockBalloonCollection() {
#if !defined(USE_AURA)
    set_notification_ui(new MockNotificationUI());
#endif
  }
  virtual ~MockBalloonCollection() {};

  // BalloonCollectionImplType overrides
  virtual void Add(const Notification& notification, Profile* profile) OVERRIDE;
  virtual Balloon* MakeBalloon(const Notification& notification,
                               Profile* profile) OVERRIDE;
  virtual void OnBalloonClosed(Balloon* source) OVERRIDE;

  // Number of balloons being shown.
  std::set<Balloon*>& balloons() { return balloons_; }
  int count() const { return balloons_.size(); }

 private:
  std::set<Balloon*> balloons_;
};

void MockBalloonCollection::Add(const Notification& notification,
                                Profile* profile) {
  // Swap in a logging proxy for the purpose of logging calls that
  // would be made into javascript, then pass this down to the
  // balloon collection.
  typedef LoggingNotificationDelegate<DesktopNotificationsTest>
      LoggingNotificationProxy;
  Notification test_notification(
      notification.origin_url(),
      notification.content_url(),
      notification.display_source(),
      notification.replace_id(),
      new LoggingNotificationProxy(notification.notification_id()));
  BalloonCollectionImplType::Add(test_notification, profile);
}

Balloon* MockBalloonCollection::MakeBalloon(const Notification& notification,
                                            Profile* profile) {
  // Start with a normal balloon but mock out the view.
  Balloon* balloon =
      BalloonCollectionImplType::MakeBalloon(notification, profile);
  balloon->set_view(new MockBalloonView(balloon));
  balloons_.insert(balloon);
  return balloon;
}

void MockBalloonCollection::OnBalloonClosed(Balloon* source) {
  balloons_.erase(source);
  BalloonCollectionImplType::OnBalloonClosed(source);
}

// DesktopNotificationsTest

DesktopNotificationsTest::DesktopNotificationsTest()
    : ui_thread_(BrowserThread::UI, &message_loop_) {
}

DesktopNotificationsTest::~DesktopNotificationsTest() {
}

void DesktopNotificationsTest::SetUp() {
#if defined(USE_AURA)
  // Make sure a root window has been instantiated.
  aura::RootWindow::GetInstance();
#endif
  browser::RegisterLocalState(&local_state_);
  profile_.reset(new TestingProfile());
  balloon_collection_ = new MockBalloonCollection();
  ui_manager_.reset(NotificationUIManager::Create(&local_state_,
                                                  balloon_collection_));
  service_.reset(new DesktopNotificationService(profile(), ui_manager_.get()));
  log_output_.clear();
}

void DesktopNotificationsTest::TearDown() {
  service_.reset(NULL);
  ui_manager_.reset(NULL);
  profile_.reset(NULL);
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

  MessageLoopForUI::current()->RunAllPending();
  EXPECT_EQ(1, balloon_collection_->count());

  content::ShowDesktopNotificationHostMsgParams params2;
  params2.origin = GURL("http://www.google.com");
  params2.is_html = true;
  params2.contents_url = GURL("http://www.google.com/notification.html");
  params2.notification_id = 2;

  EXPECT_TRUE(service_->ShowDesktopNotification(
      params2, 0, 0, DesktopNotificationService::PageNotification));
  MessageLoopForUI::current()->RunAllPending();
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

  content::ShowDesktopNotificationHostMsgParams params =
      StandardTestNotification();
  params.notification_id = notification_id;

  // Request a notification; should open a balloon.
  EXPECT_TRUE(service_->ShowDesktopNotification(
      params, process_id, route_id,
      DesktopNotificationService::PageNotification));
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
#if defined(USE_AURA)
  // Aura is using the non-chromeos notification system which has a limit
  // of 4 visible toasts.
  const int kLotsOfToasts = 4;
#else
  const int kLotsOfToasts = 20;
#endif
  for (int id = 1; id <= kLotsOfToasts; ++id) {
    SCOPED_TRACE(base::StringPrintf("Creation loop: id=%d", id));
    content::ShowDesktopNotificationHostMsgParams params =
        StandardTestNotification();
    params.notification_id = id;
    EXPECT_TRUE(service_->ShowDesktopNotification(
        params, process_id, route_id,
        DesktopNotificationService::PageNotification));
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
    SCOPED_TRACE(base::StringPrintf("Cancel half of notifications: id=%d", id));
    service_->CancelDesktopNotification(process_id, route_id, id);
    MessageLoopForUI::current()->RunAllPending();
    expected_log.append("notification closed by script\n");
    EXPECT_EQ(kLotsOfToasts - id,
              balloon_collection_->count());
    EXPECT_EQ(expected_log, log_output_);
  }

  // Now cancel the rest.  It should empty the balloon space.
  for (; id <= kLotsOfToasts; ++id) {
    SCOPED_TRACE(base::StringPrintf("Cancel loop: id=%d", id));
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
    SCOPED_TRACE(base::StringPrintf("Show Text loop: id=%d", id));

    EXPECT_TRUE(service_->ShowDesktopNotification(
        StandardTestNotification(), 0, 0,
        DesktopNotificationService::PageNotification));
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

  MessageLoopForUI::current()->RunAllPending();
  EXPECT_EQ(1, balloon_collection_->count());
  Balloon* balloon = (*balloon_collection_->balloons().begin());
  GURL data_url = balloon->notification().content_url();
  EXPECT_EQ(std::string::npos, data_url.spec().find("<script>"));
  EXPECT_EQ(std::string::npos, data_url.spec().find("<i>"));
}

}  // namespace chromeos
