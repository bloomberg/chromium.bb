// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/notification_presenter.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/fake_message_center.h"

namespace chromeos {

namespace tether {

namespace {

const char kDefaultDeviceName[] = "defaultDeviceName";

class TestMessageCenter : public message_center::FakeMessageCenter {
 public:
  TestMessageCenter() : message_center::FakeMessageCenter() {}
  ~TestMessageCenter() override {}

  // message_center::FakeMessageCenter:
  message_center::Notification* FindVisibleNotificationById(
      const std::string& id) override {
    auto iter = std::find_if(
        notifications_.begin(), notifications_.end(),
        [id](const std::shared_ptr<message_center::Notification> notification) {
          return notification->id() == id;
        });
    return iter != notifications_.end() ? iter->get() : nullptr;
  }

  void AddNotification(
      std::unique_ptr<message_center::Notification> notification) override {
    notifications_.push_back(std::move(notification));
  }

  void RemoveNotification(const std::string& id, bool by_user) override {
    notifications_.erase(std::find_if(
        notifications_.begin(), notifications_.end(),
        [id](const std::shared_ptr<message_center::Notification> notification) {
          return notification->id() == id;
        }));
  }

 private:
  std::vector<std::shared_ptr<message_center::Notification>> notifications_;
};

}  // namespace

class NotificationPresenterTest : public testing::Test {
 protected:
  NotificationPresenterTest() {}

  void SetUp() override {
    test_message_center_ = base::WrapUnique(new TestMessageCenter());
    notification_presenter_ =
        base::WrapUnique(new NotificationPresenter(test_message_center_.get()));
  }

  std::string GetActiveHostNotificationId() {
    return std::string(NotificationPresenter::kActiveHostNotificationId);
  }

  std::unique_ptr<TestMessageCenter> test_message_center_;

  std::unique_ptr<NotificationPresenter> notification_presenter_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NotificationPresenterTest);
};

TEST_F(NotificationPresenterTest, TestHostConnectionFailedNotification) {
  EXPECT_FALSE(test_message_center_->FindVisibleNotificationById(
      GetActiveHostNotificationId()));
  notification_presenter_->NotifyConnectionToHostFailed(
      std::string(kDefaultDeviceName));

  message_center::Notification* notification =
      test_message_center_->FindVisibleNotificationById(
          GetActiveHostNotificationId());
  EXPECT_TRUE(notification);
  EXPECT_EQ(GetActiveHostNotificationId(), notification->id());

  notification_presenter_->RemoveConnectionToHostFailedNotification();
  EXPECT_FALSE(test_message_center_->FindVisibleNotificationById(
      GetActiveHostNotificationId()));
}

}  // namespace tether

}  // namespace cryptauth
