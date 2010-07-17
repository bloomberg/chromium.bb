// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/network_state_notifier.h"

#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/chromeos/cros/cros_in_process_browser_test.h"
#include "chrome/browser/chromeos/cros/mock_network_library.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_registrar.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "chrome/test/ui_test_utils.h"

namespace chromeos {

using ::testing::Return;
using ::testing::_;

class NetworkStateNotifierTest : public CrosInProcessBrowserTest,
                                 public NotificationObserver {
 public:
  NetworkStateNotifierTest() {
  }

 protected:
  virtual void SetUpInProcessBrowserTestFixture() {
    InitStatusAreaMocks();
    SetStatusAreaMocksExpectations();
    // Initialize network state notifier.
    ASSERT_TRUE(CrosLibrary::Get()->EnsureLoaded());
    ASSERT_TRUE(mock_network_library_);
    EXPECT_CALL(*mock_network_library_, Connected())
        .Times(1)
        .WillRepeatedly((Return(true)))
        .RetiresOnSaturation();
    NetworkStateNotifier::Get();
  }

  // NotificationObserver overrides.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    EXPECT_TRUE(ChromeThread::CurrentlyOn(ChromeThread::UI));
    EXPECT_TRUE(NotificationType::NETWORK_STATE_CHANGED == type);
    chromeos::NetworkStateDetails* state_details =
        Details<chromeos::NetworkStateDetails>(details).ptr();
    state_ = state_details->state();
  }

  void WaitForNotification() {
    ui_test_utils::WaitForNotification(
        NotificationType::NETWORK_STATE_CHANGED);
  }

 protected:
  NetworkStateDetails::State state_;
};

IN_PROC_BROWSER_TEST_F(NetworkStateNotifierTest, TestConnected) {
  // NETWORK_STATE_CHAGNED has to be registered in UI thread.
  NotificationRegistrar registrar;
  registrar.Add(this, NotificationType::NETWORK_STATE_CHANGED,
                 NotificationService::AllSources());
  EXPECT_CALL(*mock_network_library_, Connected())
      .Times(1)
      .WillRepeatedly((Return(true)))
      .RetiresOnSaturation();
  NetworkStateNotifier* notifier = NetworkStateNotifier::Get();
  notifier->NetworkChanged(mock_network_library_);
  WaitForNotification();
  EXPECT_EQ(chromeos::NetworkStateDetails::CONNECTED, state_);
}

IN_PROC_BROWSER_TEST_F(NetworkStateNotifierTest, TestConnecting) {
  NotificationRegistrar registrar;
  registrar.Add(this, NotificationType::NETWORK_STATE_CHANGED,
                 NotificationService::AllSources());
  EXPECT_CALL(*mock_network_library_, Connected())
      .Times(1)
      .WillOnce((Return(false)))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_network_library_, Connecting())
      .Times(1)
      .WillOnce((Return(true)))
      .RetiresOnSaturation();
  NetworkStateNotifier* notifier = NetworkStateNotifier::Get();
  notifier->NetworkChanged(mock_network_library_);
  WaitForNotification();
  EXPECT_EQ(chromeos::NetworkStateDetails::CONNECTING, state_);
}

IN_PROC_BROWSER_TEST_F(NetworkStateNotifierTest, TestDisconnected) {
  NotificationRegistrar registrar;
  registrar.Add(this, NotificationType::NETWORK_STATE_CHANGED,
                 NotificationService::AllSources());
  EXPECT_CALL(*mock_network_library_, Connected())
      .Times(1)
      .WillOnce((Return(false)))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_network_library_, Connecting())
      .Times(1)
      .WillOnce((Return(false)))
      .RetiresOnSaturation();
  NetworkStateNotifier* notifier = NetworkStateNotifier::Get();
  notifier->NetworkChanged(mock_network_library_);
  WaitForNotification();
  EXPECT_EQ(chromeos::NetworkStateDetails::DISCONNECTED, state_);
}

}  // namespace chromeos
