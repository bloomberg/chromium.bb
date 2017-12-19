// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/ui/low_disk_notification.h"

#include <stdint.h>

#include <utility>

#include "base/test/scoped_task_environment.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_cryptohome_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/message_center/fake_message_center.h"
#include "ui/message_center/message_center.h"

namespace {

// Copied from low_disk_notification.cc
const uint64_t kMediumNotification = (1 << 30) - 1;
const uint64_t kHighNotification = (512 << 20) - 1;

}  // namespace

namespace chromeos {

class LowDiskNotificationTest : public testing::Test,
                                public message_center::FakeMessageCenter {
 public:
  LowDiskNotificationTest() {}

  void SetUp() override {
    DBusThreadManager::GetSetterForTesting()->SetCryptohomeClient(
        std::unique_ptr<CryptohomeClient>(new FakeCryptohomeClient));
    message_center::MessageCenter::Initialize();
    low_disk_notification_.reset(new LowDiskNotification());
    low_disk_notification_->SetMessageCenterForTest(this);
    low_disk_notification_->SetNotificationIntervalForTest(
        base::TimeDelta::FromMilliseconds(10));
    notification_count_ = 0;
  }

  void TearDown() override {
    low_disk_notification_.reset();
    last_notification_.reset();
    message_center::MessageCenter::Shutdown();
    DBusThreadManager::Shutdown();
  }

  void AddNotification(
      std::unique_ptr<message_center::Notification> notification) override {
    last_notification_ = std::move(notification);
    notification_count_++;
  }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<LowDiskNotification> low_disk_notification_;
  std::unique_ptr<message_center::Notification> last_notification_;
  int notification_count_;
};

TEST_F(LowDiskNotificationTest, MediumLevelNotification) {
  base::string16 expected_title =
      l10n_util::GetStringUTF16(IDS_LOW_DISK_NOTIFICATION_TITLE);
  low_disk_notification_->LowDiskSpace(kMediumNotification);
  EXPECT_NE(nullptr, last_notification_);
  EXPECT_EQ(expected_title, last_notification_->title());
  EXPECT_EQ(1, notification_count_);
}

TEST_F(LowDiskNotificationTest, HighLevelReplacesMedium) {
  base::string16 expected_title =
      l10n_util::GetStringUTF16(IDS_CRITICALLY_LOW_DISK_NOTIFICATION_TITLE);
  low_disk_notification_->LowDiskSpace(kMediumNotification);
  low_disk_notification_->LowDiskSpace(kHighNotification);
  EXPECT_NE(nullptr, last_notification_);
  EXPECT_EQ(expected_title, last_notification_->title());
  EXPECT_EQ(2, notification_count_);
}

TEST_F(LowDiskNotificationTest, NotificationsAreThrottled) {
  low_disk_notification_->LowDiskSpace(kHighNotification);
  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(5));
  low_disk_notification_->LowDiskSpace(kHighNotification);
  EXPECT_EQ(1, notification_count_);
}

TEST_F(LowDiskNotificationTest, HighNotificationsAreShownAfterThrottling) {
  low_disk_notification_->LowDiskSpace(kHighNotification);
  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(15));
  low_disk_notification_->LowDiskSpace(kHighNotification);
  EXPECT_EQ(2, notification_count_);
}

TEST_F(LowDiskNotificationTest, MediumNotificationsAreNotShownAfterThrottling) {
  low_disk_notification_->LowDiskSpace(kMediumNotification);
  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(15));
  low_disk_notification_->LowDiskSpace(kMediumNotification);
  EXPECT_EQ(1, notification_count_);
}

}  // namespace chromeos
