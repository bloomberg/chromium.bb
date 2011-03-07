// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/network_state_notifier.h"

#include "chrome/browser/chromeos/cros/cros_in_process_browser_test.h"
#include "chrome/browser/chromeos/cros/mock_network_library.h"
#include "chrome/test/ui_test_utils.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_registrar.h"
#include "content/common/notification_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

using ::testing::Return;
using ::testing::_;

class NetworkStateNotifierTest : public CrosInProcessBrowserTest,
                                 public NotificationObserver {
 public:
  NetworkStateNotifierTest() : mock_network_library_(NULL) {
  }

 protected:
  virtual void SetUpInProcessBrowserTestFixture() {
    cros_mock_->InitStatusAreaMocks();
    cros_mock_->SetStatusAreaMocksExpectations();
    // Initialize network state notifier.
    ASSERT_TRUE(CrosLibrary::Get()->EnsureLoaded());
    mock_network_library_ = cros_mock_->mock_network_library();
    ASSERT_TRUE(mock_network_library_);
    EXPECT_CALL(*mock_network_library_, Connected())
        .Times(1)
        .WillRepeatedly((Return(true)))
        .RetiresOnSaturation();
    NetworkStateNotifier::GetInstance();
  }

  // NotificationObserver overrides.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
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
  MockNetworkLibrary *mock_network_library_;
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
  NetworkStateNotifier* notifier = NetworkStateNotifier::GetInstance();
  notifier->OnNetworkManagerChanged(mock_network_library_);
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
  NetworkStateNotifier* notifier = NetworkStateNotifier::GetInstance();
  notifier->OnNetworkManagerChanged(mock_network_library_);
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
  NetworkStateNotifier* notifier = NetworkStateNotifier::GetInstance();
  notifier->OnNetworkManagerChanged(mock_network_library_);
  WaitForNotification();
  EXPECT_EQ(chromeos::NetworkStateDetails::DISCONNECTED, state_);
}

}  // namespace chromeos
