// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/chromeos/net/network_portal_notification_controller.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/network/network_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"

using message_center::MessageCenter;

namespace chromeos {

namespace {

const char* kNotificationId =
    NetworkPortalNotificationController::kNotificationId;

bool HasNotification() {
  MessageCenter* message_center = MessageCenter::Get();
  return message_center->FindVisibleNotificationById(kNotificationId);
}

class NotificationObserver : public message_center::MessageCenterObserver {
 public:
  NotificationObserver() : add_count_(0), remove_count_(0), update_count_(0) {}

  // Overridden from message_center::MessageCenterObserver:
  virtual void OnNotificationAdded(
      const std::string& notification_id) OVERRIDE {
    if (notification_id == kNotificationId)
      ++add_count_;
  }

  virtual void OnNotificationRemoved(const std::string& notification_id,
                                     bool /* by_user */) OVERRIDE {
    if (notification_id == kNotificationId)
      ++remove_count_;
  }

  virtual void OnNotificationUpdated(
      const std::string& notification_id) OVERRIDE {
    if (notification_id == kNotificationId)
      ++update_count_;
  }

  unsigned add_count() const { return add_count_; }
  unsigned remove_count() const { return remove_count_; }
  unsigned update_count() const { return update_count_; }

 private:
  unsigned add_count_;
  unsigned remove_count_;
  unsigned update_count_;

  DISALLOW_COPY_AND_ASSIGN(NotificationObserver);
};

}  // namespace

class NetworkPortalNotificationControllerTest : public testing::Test {
 public:
  NetworkPortalNotificationControllerTest() {}
  virtual ~NetworkPortalNotificationControllerTest() {}

  virtual void SetUp() OVERRIDE {
    CommandLine* cl = CommandLine::ForCurrentProcess();
    cl->AppendSwitch(switches::kEnableNetworkPortalNotification);
    MessageCenter::Initialize();
    MessageCenter::Get()->AddObserver(&observer_);
  }

  virtual void TearDown() OVERRIDE {
    MessageCenter::Get()->RemoveObserver(&observer_);
    MessageCenter::Shutdown();
  }

 protected:
  void OnPortalDetectionCompleted(
      const NetworkState* network,
      const NetworkPortalDetector::CaptivePortalState& state) {
    controller_.OnPortalDetectionCompleted(network, state);
  }

  NotificationObserver& observer() { return observer_; }

 private:
  NetworkPortalNotificationController controller_;
  NotificationObserver observer_;

  DISALLOW_COPY_AND_ASSIGN(NetworkPortalNotificationControllerTest);
};

TEST_F(NetworkPortalNotificationControllerTest, NetworkStateChanged) {
  NetworkState wifi("wifi");
  NetworkPortalDetector::CaptivePortalState wifi_state;

  // Notification is not displayed for online state.
  wifi_state.status = NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE;
  wifi_state.response_code = 204;
  OnPortalDetectionCompleted(&wifi, wifi_state);
  ASSERT_FALSE(HasNotification());

  // Notification is displayed for portal state
  wifi_state.status = NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL;
  wifi_state.response_code = 200;
  OnPortalDetectionCompleted(&wifi, wifi_state);
  ASSERT_TRUE(HasNotification());

  // Notification is closed for online state.
  wifi_state.status = NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE;
  wifi_state.response_code = 204;
  OnPortalDetectionCompleted(&wifi, wifi_state);
  ASSERT_FALSE(HasNotification());
}

TEST_F(NetworkPortalNotificationControllerTest, NetworkChanged) {
  NetworkState wifi1("wifi1");
  NetworkPortalDetector::CaptivePortalState wifi1_state;
  wifi1_state.status = NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL;
  wifi1_state.response_code = 200;
  OnPortalDetectionCompleted(&wifi1, wifi1_state);
  ASSERT_TRUE(HasNotification());

  MessageCenter::Get()->RemoveNotification(kNotificationId, true /* by_user */);
  ASSERT_FALSE(HasNotification());

  // User already closed notification about portal state for this network,
  // so notification shouldn't be displayed second time.
  OnPortalDetectionCompleted(&wifi1, wifi1_state);
  ASSERT_FALSE(HasNotification());

  NetworkState wifi2("wifi2");
  NetworkPortalDetector::CaptivePortalState wifi2_state;
  wifi2_state.status = NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE;
  wifi2_state.response_code = 204;

  // Second network is in online state, so there shouldn't be any
  // notifications.
  OnPortalDetectionCompleted(&wifi2, wifi2_state);
  ASSERT_FALSE(HasNotification());

  // User switches back to the first network, so notification should
  // be displayed.
  OnPortalDetectionCompleted(&wifi1, wifi1_state);
  ASSERT_TRUE(HasNotification());
}

TEST_F(NetworkPortalNotificationControllerTest, NotificationUpdated) {
  NetworkPortalDetector::CaptivePortalState portal_state;
  portal_state.status = NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL;
  portal_state.response_code = 200;

  // First network is behind a captive portal, so notification should
  // be displayed.
  NetworkState wifi1("wifi1");
  OnPortalDetectionCompleted(&wifi1, portal_state);
  ASSERT_TRUE(HasNotification());
  EXPECT_EQ(1u, observer().add_count());
  EXPECT_EQ(0u, observer().remove_count());
  EXPECT_EQ(0u, observer().update_count());

  // Second network is also behind a captive portal, so notification
  // should be updated.
  NetworkState wifi2("wifi2");
  OnPortalDetectionCompleted(&wifi2, portal_state);
  ASSERT_TRUE(HasNotification());
  EXPECT_EQ(1u, observer().add_count());
  EXPECT_EQ(0u, observer().remove_count());
  EXPECT_EQ(1u, observer().update_count());

  // User closes the notification.
  MessageCenter::Get()->RemoveNotification(kNotificationId, true /* by_user */);
  ASSERT_FALSE(HasNotification());
  EXPECT_EQ(1u, observer().add_count());
  EXPECT_EQ(1u, observer().remove_count());
  EXPECT_EQ(1u, observer().update_count());

  // Portal detector notified that second network is still behind captive
  // portal, but user already closed the notification, so there should
  // not be any notifications.
  OnPortalDetectionCompleted(&wifi2, portal_state);
  ASSERT_FALSE(HasNotification());
  EXPECT_EQ(1u, observer().add_count());
  EXPECT_EQ(1u, observer().remove_count());
  EXPECT_EQ(1u, observer().update_count());

  // Network was switched (by shill or by user) to wifi1. Notification
  // should be displayed.
  OnPortalDetectionCompleted(&wifi1, portal_state);
  ASSERT_TRUE(HasNotification());
  EXPECT_EQ(2u, observer().add_count());
  EXPECT_EQ(1u, observer().remove_count());
  EXPECT_EQ(1u, observer().update_count());
}

}  // namespace chromeos
