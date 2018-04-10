// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/multi_device_setup/multi_device_notification_presenter.h"

#include <map>
#include <memory>
#include <utility>

#include "ash/strings/grit/ash_strings.h"
#include "base/memory/ptr_util.h"
#include "base/test/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/fake_message_center.h"
#include "ui/message_center/message_center.h"

namespace ash {

namespace {

class TestMessageCenter : public message_center::FakeMessageCenter {
 public:
  TestMessageCenter() = default;
  ~TestMessageCenter() override = default;

  // message_center::FakeMessageCenter:
  void AddNotification(
      std::unique_ptr<message_center::Notification> notification) override {
    EXPECT_FALSE(notification_);
    notification_ = std::move(notification);
  }

  void UpdateNotification(
      const std::string& id,
      std::unique_ptr<message_center::Notification> new_notification) override {
    EXPECT_TRUE(notification_);
    EXPECT_EQ(notification_->id(), id);
    EXPECT_EQ(new_notification->id(), id);
    notification_ = std::move(new_notification);
  }

  void RemoveNotification(const std::string& id, bool by_user) override {
    EXPECT_TRUE(notification_);
    EXPECT_EQ(notification_->id(), id);
    notification_.reset();
  }

  message_center::Notification* FindVisibleNotificationById(
      const std::string& id) override {
    if (notification_) {
      EXPECT_EQ(notification_->id(), id);
      return notification_.get();
    }
    return nullptr;
  }

  void ClickOnNotification(const std::string& id) override {
    EXPECT_TRUE(notification_);
    EXPECT_EQ(id, notification_->id());
    notification_->delegate()->Click(base::nullopt, base::nullopt);
  }

 private:
  std::unique_ptr<message_center::Notification> notification_;

  DISALLOW_COPY_AND_ASSIGN(TestMessageCenter);
};

}  // namespace

class MultiDeviceNotificationPresenterTest : public testing::Test {
 public:
  class TestOpenUiDelegate
      : public MultiDeviceNotificationPresenter::OpenUiDelegate {
   public:
    TestOpenUiDelegate() = default;
    ~TestOpenUiDelegate() override = default;

    int open_multi_device_setup_ui_count() const {
      return open_multi_device_setup_ui_count_;
    }

    int open_change_connected_phone_settings_count() const {
      return open_change_connected_phone_settings_count_;
    }

    int open_connected_devices_settings_count() const {
      return open_connected_devices_settings_count_;
    }

    // MultiDeviceNotificationPresenter::OpenUiDelegate:
    void OpenMultiDeviceSetupUi() override {
      ++open_multi_device_setup_ui_count_;
    }

    void OpenChangeConnectedPhoneSettings() override {
      ++open_change_connected_phone_settings_count_;
    }

    void OpenConnectedDevicesSettings() override {
      ++open_connected_devices_settings_count_;
    }

   private:
    int open_multi_device_setup_ui_count_ = 0;
    int open_change_connected_phone_settings_count_ = 0;
    int open_connected_devices_settings_count_ = 0;
  };

 protected:
  MultiDeviceNotificationPresenterTest() = default;

  void SetUp() override {
    std::unique_ptr<TestOpenUiDelegate> test_open_ui_delegate =
        std::make_unique<TestOpenUiDelegate>();

    test_open_ui_delegate_ = test_open_ui_delegate.get();

    notification_presenter_ =
        std::make_unique<MultiDeviceNotificationPresenter>(
            &test_message_center_);
    notification_presenter_->open_ui_delegate_ =
        std::move(test_open_ui_delegate);
  }

  void ClickNotification() {
    test_message_center_.ClickOnNotification(
        MultiDeviceNotificationPresenter::kNotificationId);
  }

  void VerifyNewUserPotentialHostExistsNotificationIsVisible() {
    VerifyNotificationIsVisible(
        MultiDeviceNotificationPresenter::Status::kNewUserNotificationVisible);
  }

  void VerifyExistingUserHostSwitchedNotificationIsVisible() {
    VerifyNotificationIsVisible(
        MultiDeviceNotificationPresenter::Status::
            kExistingUserHostSwitchedNotificationVisible);
  }

  void VerifyExistingUserNewChromebookAddedNotificationIsVisible() {
    VerifyNotificationIsVisible(
        MultiDeviceNotificationPresenter::Status::
            kExistingUserNewChromebookNotificationVisible);
  }

  void VerifyNoNotificationIsVisible() {
    EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
        MultiDeviceNotificationPresenter::kNotificationId));
  }

  void AssertPotentialHostBucketCount(std::string histogram, int count) {
    if (histogram_tester_.GetAllSamples(histogram).empty()) {
      EXPECT_EQ(count, 0);
      return;
    }
    histogram_tester_.ExpectBucketCount(
        histogram,
        MultiDeviceNotificationPresenter::
            kNotificationTypeNewUserPotentialHostExists,
        count);
  }

  void AssertHostSwitchedBucketCount(std::string histogram, int count) {
    if (histogram_tester_.GetAllSamples(histogram).empty()) {
      EXPECT_EQ(count, 0);
      return;
    }
    histogram_tester_.ExpectBucketCount(
        histogram,
        MultiDeviceNotificationPresenter::
            kNotificationTypeExistingUserHostSwitched,
        count);
  }

  void AssertNewChromebookBucketCount(std::string histogram, int count) {
    if (histogram_tester_.GetAllSamples(histogram).empty()) {
      EXPECT_EQ(count, 0);
      return;
    }
    histogram_tester_.ExpectBucketCount(
        histogram,
        MultiDeviceNotificationPresenter::
            kNotificationTypeExistingUserNewChromebookAdded,
        count);
  }

  base::HistogramTester histogram_tester_;
  TestOpenUiDelegate* test_open_ui_delegate_;
  TestMessageCenter test_message_center_;
  std::unique_ptr<MultiDeviceNotificationPresenter> notification_presenter_;

 private:
  void VerifyNotificationIsVisible(
      MultiDeviceNotificationPresenter::Status notification_status) {
    const message_center::Notification* kVisibleNotification =
        test_message_center_.FindVisibleNotificationById(
            MultiDeviceNotificationPresenter::kNotificationId);
    base::string16 title;
    base::string16 message;
    switch (notification_status) {
      case MultiDeviceNotificationPresenter::Status::
          kNewUserNotificationVisible:
        title = l10n_util::GetStringUTF16(
            IDS_ASH_MULTI_DEVICE_SETUP_NEW_USER_POTENTIAL_HOST_EXISTS_TITLE);
        message = l10n_util::GetStringUTF16(
            IDS_ASH_MULTI_DEVICE_SETUP_NEW_USER_POTENTIAL_HOST_EXISTS_MESSAGE);
        break;
      case MultiDeviceNotificationPresenter::Status::
          kExistingUserHostSwitchedNotificationVisible:
        title = l10n_util::GetStringUTF16(
            IDS_ASH_MULTI_DEVICE_SETUP_EXISTING_USER_HOST_SWITCHED_TITLE);
        message = l10n_util::GetStringUTF16(
            IDS_ASH_MULTI_DEVICE_SETUP_EXISTING_USER_HOST_SWITCHED_MESSAGE);
        break;
      case MultiDeviceNotificationPresenter::Status::
          kExistingUserNewChromebookNotificationVisible:
        title = l10n_util::GetStringUTF16(
            IDS_ASH_MULTI_DEVICE_SETUP_EXISTING_USER_NEW_CHROMEBOOK_ADDED_TITLE);
        message = l10n_util::GetStringUTF16(
            IDS_ASH_MULTI_DEVICE_SETUP_EXISTING_USER_NEW_CHROMEBOOK_ADDED_MESSAGE);
        break;
      case MultiDeviceNotificationPresenter::Status::kNoNotificationVisible:
        NOTREACHED();
    }
    EXPECT_EQ(title, kVisibleNotification->title());
    EXPECT_EQ(message, kVisibleNotification->message());
  }

  DISALLOW_COPY_AND_ASSIGN(MultiDeviceNotificationPresenterTest);
};

TEST_F(MultiDeviceNotificationPresenterTest,
       TestHostNewUserPotentialHostExistsNotification_RemoveProgrammatically) {
  notification_presenter_->OnPotentialHostExistsForNewUser();
  VerifyNewUserPotentialHostExistsNotificationIsVisible();

  notification_presenter_->RemoveMultiDeviceSetupNotification();
  VerifyNoNotificationIsVisible();

  EXPECT_EQ(test_open_ui_delegate_->open_multi_device_setup_ui_count(), 0);
  AssertPotentialHostBucketCount("MultiDeviceSetup_NotificationClicked", 0);
  AssertPotentialHostBucketCount("MultiDeviceSetup_NotificationShown", 1);
}

TEST_F(MultiDeviceNotificationPresenterTest,
       TestHostNewUserPotentialHostExistsNotification_TapNotification) {
  notification_presenter_->OnPotentialHostExistsForNewUser();
  VerifyNewUserPotentialHostExistsNotificationIsVisible();

  ClickNotification();
  VerifyNoNotificationIsVisible();

  EXPECT_EQ(test_open_ui_delegate_->open_multi_device_setup_ui_count(), 1);
  AssertPotentialHostBucketCount("MultiDeviceSetup_NotificationClicked", 1);
  AssertPotentialHostBucketCount("MultiDeviceSetup_NotificationShown", 1);
}

TEST_F(MultiDeviceNotificationPresenterTest,
       TestHostExistingUserHostSwitchedNotification_RemoveProgrammatically) {
  notification_presenter_->OnConnectedHostSwitchedForExistingUser();
  VerifyExistingUserHostSwitchedNotificationIsVisible();

  notification_presenter_->RemoveMultiDeviceSetupNotification();
  VerifyNoNotificationIsVisible();

  EXPECT_EQ(
      test_open_ui_delegate_->open_change_connected_phone_settings_count(), 0);
  AssertHostSwitchedBucketCount("MultiDeviceSetup_NotificationClicked", 0);
  AssertHostSwitchedBucketCount("MultiDeviceSetup_NotificationShown", 1);
}

TEST_F(MultiDeviceNotificationPresenterTest,
       TestHostExistingUserHostSwitchedNotification_TapNotification) {
  notification_presenter_->OnConnectedHostSwitchedForExistingUser();
  VerifyExistingUserHostSwitchedNotificationIsVisible();

  ClickNotification();
  VerifyNoNotificationIsVisible();

  EXPECT_EQ(
      test_open_ui_delegate_->open_change_connected_phone_settings_count(), 1);
  AssertHostSwitchedBucketCount("MultiDeviceSetup_NotificationClicked", 1);
  AssertHostSwitchedBucketCount("MultiDeviceSetup_NotificationShown", 1);
}

TEST_F(
    MultiDeviceNotificationPresenterTest,
    TestHostExistingUserNewChromebookAddedNotification_RemoveProgrammatically) {
  notification_presenter_->OnNewChromebookAddedForExistingUser();
  VerifyExistingUserNewChromebookAddedNotificationIsVisible();

  notification_presenter_->RemoveMultiDeviceSetupNotification();
  VerifyNoNotificationIsVisible();

  EXPECT_EQ(test_open_ui_delegate_->open_connected_devices_settings_count(), 0);
  AssertNewChromebookBucketCount("MultiDeviceSetup_NotificationClicked", 0);
  AssertNewChromebookBucketCount("MultiDeviceSetup_NotificationShown", 1);
}

TEST_F(MultiDeviceNotificationPresenterTest,
       TestHostExistingUserNewChromebookAddedNotification_TapNotification) {
  notification_presenter_->OnNewChromebookAddedForExistingUser();
  VerifyExistingUserNewChromebookAddedNotificationIsVisible();

  ClickNotification();
  VerifyNoNotificationIsVisible();

  EXPECT_EQ(test_open_ui_delegate_->open_connected_devices_settings_count(), 1);
  AssertNewChromebookBucketCount("MultiDeviceSetup_NotificationClicked", 1);
  AssertNewChromebookBucketCount("MultiDeviceSetup_NotificationShown", 1);
}

TEST_F(MultiDeviceNotificationPresenterTest, NotificationsReplaceOneAnother) {
  notification_presenter_->OnPotentialHostExistsForNewUser();
  VerifyNewUserPotentialHostExistsNotificationIsVisible();

  notification_presenter_->OnConnectedHostSwitchedForExistingUser();
  VerifyExistingUserHostSwitchedNotificationIsVisible();

  notification_presenter_->OnNewChromebookAddedForExistingUser();
  VerifyExistingUserNewChromebookAddedNotificationIsVisible();

  ClickNotification();
  VerifyNoNotificationIsVisible();

  AssertPotentialHostBucketCount("MultiDeviceSetup_NotificationShown", 1);
  AssertHostSwitchedBucketCount("MultiDeviceSetup_NotificationShown", 1);
  AssertNewChromebookBucketCount("MultiDeviceSetup_NotificationShown", 1);
}

TEST_F(MultiDeviceNotificationPresenterTest, NotificationsReplaceThemselves) {
  notification_presenter_->OnPotentialHostExistsForNewUser();
  VerifyNewUserPotentialHostExistsNotificationIsVisible();
  notification_presenter_->OnPotentialHostExistsForNewUser();
  VerifyNewUserPotentialHostExistsNotificationIsVisible();
  notification_presenter_->RemoveMultiDeviceSetupNotification();

  notification_presenter_->OnConnectedHostSwitchedForExistingUser();
  VerifyExistingUserHostSwitchedNotificationIsVisible();
  notification_presenter_->OnConnectedHostSwitchedForExistingUser();
  VerifyExistingUserHostSwitchedNotificationIsVisible();
  notification_presenter_->RemoveMultiDeviceSetupNotification();

  notification_presenter_->OnNewChromebookAddedForExistingUser();
  VerifyExistingUserNewChromebookAddedNotificationIsVisible();
  notification_presenter_->OnNewChromebookAddedForExistingUser();
  VerifyExistingUserNewChromebookAddedNotificationIsVisible();
  notification_presenter_->RemoveMultiDeviceSetupNotification();

  AssertPotentialHostBucketCount("MultiDeviceSetup_NotificationShown", 2);
  AssertHostSwitchedBucketCount("MultiDeviceSetup_NotificationShown", 2);
  AssertNewChromebookBucketCount("MultiDeviceSetup_NotificationShown", 2);
}

}  // namespace ash
