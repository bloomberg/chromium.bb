// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/hats/hats_notification_controller.h"

#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "chrome/browser/notifications/message_center_notification_manager.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/portal_detector/mock_network_portal_detector.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"
#include "components/image_fetcher/core/request_metadata.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/fake_ui_delegate.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"

using testing::_;
using testing::AtLeast;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;
using testing::SaveArg;

namespace chromeos {

class HatsNotificationControllerTest : public BrowserWithTestWindowTest {
 public:
  HatsNotificationControllerTest() {}
  ~HatsNotificationControllerTest() override {}

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    MessageCenterNotificationManager* manager =
        static_cast<MessageCenterNotificationManager*>(
            g_browser_process->notification_ui_manager());
    manager->SetUiDelegateForTest(new message_center::FakeUiDelegate());

    network_portal_detector::InitializeForTesting(
        &mock_network_portal_detector_);
  }

  void TearDown() override {
    g_browser_process->notification_ui_manager()->StartShutdown();
    // The notifications may be deleted async.
    base::RunLoop loop;
    loop.RunUntilIdle();
    network_portal_detector::InitializeForTesting(nullptr);
    BrowserWithTestWindowTest::TearDown();
  }

  scoped_refptr<HatsNotificationController> InstantiateHatsController() {
    // The initialization will fail since the function IsNewDevice() will return
    // true.
    scoped_refptr<HatsNotificationController> hats_notification_controller =
        new HatsNotificationController(profile());

    // HatsController::IsNewDevice() is run on a blocking thread.
    content::RunAllTasksUntilIdle();

    // Send a callback to the observer to simulate internet connectivity is
    // present on device.
    ON_CALL(mock_network_portal_detector_,
            AddAndFireObserver(hats_notification_controller.get()))
        .WillByDefault(Invoke(
            [&hats_notification_controller](NetworkPortalDetector::Observer*) {
              NetworkPortalDetector::CaptivePortalState online_state;
              NetworkState network_state("");
              online_state.status =
                  NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE;
              hats_notification_controller->OnPortalDetectionCompleted(
                  &network_state, online_state);
            }));

    return hats_notification_controller;
  }

  NiceMock<MockNetworkPortalDetector> mock_network_portal_detector_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HatsNotificationControllerTest);
};

TEST_F(HatsNotificationControllerTest, NewDevice_ShouldNotShowNotification) {
  int64_t initial_timestamp = base::Time::Now().ToInternalValue();
  PrefService* pref_service = profile()->GetPrefs();
  pref_service->SetInt64(prefs::kHatsLastInteractionTimestamp,
                         initial_timestamp);

  auto hats_notification_controller = InstantiateHatsController();
  hats_notification_controller->Initialize(true);

  int64_t current_timestamp =
      pref_service->GetInt64(prefs::kHatsLastInteractionTimestamp);

  // When the device is new, the controller does not begin initialization and
  // simply updates the timestamp to Time::Now().
  ASSERT_TRUE(base::Time::FromInternalValue(current_timestamp) >
              base::Time::FromInternalValue(initial_timestamp));

  // Destructor for HatsController removes self from observer list.
  EXPECT_CALL(mock_network_portal_detector_,
              RemoveObserver(hats_notification_controller.get()))
      .Times(1);

  const message_center::Notification* notification =
      g_browser_process->notification_ui_manager()->FindById(
          HatsNotificationController::kNotificationId, profile());
  EXPECT_FALSE(notification);
}

TEST_F(HatsNotificationControllerTest, OldDevice_ShouldShowNotification) {
  auto hats_notification_controller = InstantiateHatsController();

  // On initialization, HatsNotificationController adds itself as an observer to
  // NetworkPortalDetector to detect internet connectivity.
  EXPECT_CALL(mock_network_portal_detector_,
              AddAndFireObserver(hats_notification_controller.get()))
      .Times(1);
  // Observer is removed if an internet connection is detected. It is called
  // a second time when hats_notification_controller is destroyed.
  EXPECT_CALL(mock_network_portal_detector_,
              RemoveObserver(hats_notification_controller.get()))
      .Times(2);

  hats_notification_controller->Initialize(false);

  // Finally check if notification was launched to confirm initialization.
  const message_center::Notification* notification =
      g_browser_process->notification_ui_manager()->FindById(
          HatsNotificationController::kNotificationId, profile());
  EXPECT_TRUE(notification != nullptr);
}

TEST_F(HatsNotificationControllerTest, NoInternet_DoNotShowNotification) {
  auto hats_notification_controller = InstantiateHatsController();

  // Upon destruction HatsNotificationController removes itself as an observer
  // from NetworkPortalDetector. This will only be called once from the
  EXPECT_CALL(mock_network_portal_detector_,
              RemoveObserver(hats_notification_controller.get()))
      .Times(1);

  NetworkState network_state("");
  NetworkPortalDetector::CaptivePortalState online_state;
  hats_notification_controller->OnPortalDetectionCompleted(&network_state,
                                                           online_state);

  online_state.status = NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_OFFLINE;
  hats_notification_controller->OnPortalDetectionCompleted(&network_state,
                                                           online_state);

  online_state.status = NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL;
  hats_notification_controller->OnPortalDetectionCompleted(&network_state,
                                                           online_state);

  online_state.status =
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PROXY_AUTH_REQUIRED;
  hats_notification_controller->OnPortalDetectionCompleted(&network_state,
                                                           online_state);

  const message_center::Notification* notification =
      g_browser_process->notification_ui_manager()->FindById(
          HatsNotificationController::kNotificationId, profile());
  EXPECT_FALSE(notification);
}

TEST_F(HatsNotificationControllerTest, DismissNotification_ShouldUpdatePref) {
  int64_t now_timestamp = base::Time::Now().ToInternalValue();
  PrefService* pref_service = profile()->GetPrefs();
  pref_service->SetInt64(prefs::kHatsLastInteractionTimestamp, now_timestamp);

  auto hats_notification_controller = InstantiateHatsController();

  // Simulate closing notification via user interaction.
  hats_notification_controller->Close(true);

  int64_t new_timestamp =
      pref_service->GetInt64(prefs::kHatsLastInteractionTimestamp);
  // The flag should be updated to a new timestamp.
  ASSERT_TRUE(base::Time::FromInternalValue(new_timestamp) >
              base::Time::FromInternalValue(now_timestamp));

  // Destructor for HatsController removes self from observer list.
  EXPECT_CALL(mock_network_portal_detector_,
              RemoveObserver(hats_notification_controller.get()))
      .Times(1);
}

}  // namespace chromeos
