// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/net/network_portal_detector_impl.h"
#include "chrome/browser/chromeos/net/network_portal_detector_test_utils.h"
#include "chrome/browser/chromeos/net/network_portal_notification_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_service_client.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"
#include "chromeos/network/portal_detector/network_portal_detector_strategy.h"
#include "components/captive_portal/captive_portal_testing_utils.h"
#include "components/prefs/pref_service.h"
#include "components/syncable_prefs/pref_service_syncable.h"
#include "content/public/test/test_utils.h"
#include "dbus/object_path.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"

using base::MessageLoop;
using message_center::MessageCenter;
using message_center::MessageCenterObserver;

namespace chromeos {

class NetworkPortalWebDialog;

namespace {

const char* const kNotificationId =
    NetworkPortalNotificationController::kNotificationId;
const char* const kNotificationMetric =
    NetworkPortalNotificationController::kNotificationMetric;
const char* const kUserActionMetric =
    NetworkPortalNotificationController::kUserActionMetric;

const char kTestUser[] = "test-user@gmail.com";
const char kWifiServicePath[] = "/service/wifi";
const char kWifiGuid[] = "wifi";

void ErrorCallbackFunction(const std::string& error_name,
                           const std::string& error_message) {
  CHECK(false) << "Shill Error: " << error_name << " : " << error_message;
}

void SetConnected(const std::string& service_path) {
  DBusThreadManager::Get()->GetShillServiceClient()->Connect(
      dbus::ObjectPath(service_path),
      base::Bind(&base::DoNothing),
      base::Bind(&ErrorCallbackFunction));
  base::RunLoop().RunUntilIdle();
}

class TestObserver : public MessageCenterObserver {
 public:
  TestObserver() : run_loop_(new base::RunLoop()) {
    MessageCenter::Get()->AddObserver(this);
  }

  ~TestObserver() override { MessageCenter::Get()->RemoveObserver(this); }

  void WaitAndReset() {
    run_loop_->Run();
    run_loop_.reset(new base::RunLoop());
  }

  void OnNotificationDisplayed(
      const std::string& notification_id,
      const message_center::DisplaySource source) override {
    if (notification_id == kNotificationId)
      MessageLoop::current()->PostTask(FROM_HERE, run_loop_->QuitClosure());
  }

  void OnNotificationRemoved(const std::string& notification_id,
                             bool by_user) override {
    if (notification_id == kNotificationId && by_user)
      MessageLoop::current()->PostTask(FROM_HERE, run_loop_->QuitClosure());
  }

 private:
  scoped_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

}  // namespace

class NetworkPortalDetectorImplBrowserTest
    : public LoginManagerTest,
      public captive_portal::CaptivePortalDetectorTestBase {
 public:
  NetworkPortalDetectorImplBrowserTest()
      : LoginManagerTest(false), network_portal_detector_(NULL) {}
  ~NetworkPortalDetectorImplBrowserTest() override {}

  void SetUpOnMainThread() override {
    LoginManagerTest::SetUpOnMainThread();

    ShillServiceClient::TestInterface* service_test =
        DBusThreadManager::Get()->GetShillServiceClient()->GetTestInterface();
    service_test->ClearServices();
    service_test->AddService(kWifiServicePath,
                             kWifiGuid,
                             "wifi",
                             shill::kTypeEthernet,
                             shill::kStateIdle,
                             true /* add_to_visible */);
    DBusThreadManager::Get()->GetShillServiceClient()->SetProperty(
        dbus::ObjectPath(kWifiServicePath),
        shill::kStateProperty,
        base::StringValue(shill::kStatePortal),
        base::Bind(&base::DoNothing),
        base::Bind(&ErrorCallbackFunction));

    network_portal_detector_ = new NetworkPortalDetectorImpl(
        g_browser_process->system_request_context(),
        true /* create_notification_controller */);
    network_portal_detector::InitializeForTesting(network_portal_detector_);
    network_portal_detector_->Enable(false /* start_detection */);
    set_detector(network_portal_detector_->captive_portal_detector_.get());
    PortalDetectorStrategy::set_delay_till_next_attempt_for_testing(
        base::TimeDelta());
    base::RunLoop().RunUntilIdle();
  }

  void RestartDetection() {
    network_portal_detector_->StopDetection();
    network_portal_detector_->StartDetection();
    base::RunLoop().RunUntilIdle();
  }

  PortalDetectorStrategy* strategy() {
    return network_portal_detector_->strategy_.get();
  }

  MessageCenter* message_center() { return MessageCenter::Get(); }

  void SetIgnoreNoNetworkForTesting() {
    network_portal_detector_->notification_controller_
        ->SetIgnoreNoNetworkForTesting();
  }

  const NetworkPortalWebDialog* GetDialog() const {
    return network_portal_detector_->notification_controller_
        ->GetDialogForTesting();
  }

 private:
  NetworkPortalDetectorImpl* network_portal_detector_;

  DISALLOW_COPY_AND_ASSIGN(NetworkPortalDetectorImplBrowserTest);
};

IN_PROC_BROWSER_TEST_F(NetworkPortalDetectorImplBrowserTest,
                       PRE_InSessionDetection) {
  RegisterUser(kTestUser);
  StartupUtils::MarkOobeCompleted();
  ASSERT_EQ(PortalDetectorStrategy::STRATEGY_ID_LOGIN_SCREEN, strategy()->Id());
}

IN_PROC_BROWSER_TEST_F(NetworkPortalDetectorImplBrowserTest,
                       InSessionDetection) {
  typedef NetworkPortalNotificationController Controller;

  TestObserver observer;

  EnumHistogramChecker ui_checker(
      kNotificationMetric, Controller::NOTIFICATION_METRIC_COUNT, NULL);
  EnumHistogramChecker action_checker(
      kUserActionMetric, Controller::USER_ACTION_METRIC_COUNT, NULL);

  LoginUser(kTestUser);
  content::RunAllPendingInMessageLoop();

  // User connects to wifi.
  SetConnected(kWifiServicePath);

  ASSERT_EQ(PortalDetectorStrategy::STRATEGY_ID_SESSION, strategy()->Id());

  // No notification until portal detection is completed.
  ASSERT_FALSE(message_center()->FindVisibleNotificationById(kNotificationId));
  RestartDetection();
  CompleteURLFetch(net::OK, 200, NULL);

  // Check that wifi is marked as behind the portal and that notification
  // is displayed.
  ASSERT_TRUE(message_center()->FindVisibleNotificationById(kNotificationId));
  ASSERT_EQ(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL,
            network_portal_detector::GetInstance()
                ->GetCaptivePortalState(kWifiGuid)
                .status);

  // Wait until notification is displayed.
  observer.WaitAndReset();

  ASSERT_TRUE(
      ui_checker.Expect(Controller::NOTIFICATION_METRIC_DISPLAYED, 1)->Check());
  ASSERT_TRUE(action_checker.Check());

  // User explicitly closes the notification.
  message_center()->RemoveNotification(kNotificationId, true);

  // Wait until notification is closed.
  observer.WaitAndReset();

  ASSERT_TRUE(ui_checker.Check());
  ASSERT_TRUE(
      action_checker.Expect(Controller::USER_ACTION_METRIC_CLOSED, 1)->Check());
}

class NetworkPortalDetectorImplBrowserTestIgnoreProxy
    : public NetworkPortalDetectorImplBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  NetworkPortalDetectorImplBrowserTestIgnoreProxy()
      : NetworkPortalDetectorImplBrowserTest() {}

 protected:
  void TestImpl(const bool preference_value);

  DISALLOW_COPY_AND_ASSIGN(NetworkPortalDetectorImplBrowserTestIgnoreProxy);
};

void NetworkPortalDetectorImplBrowserTestIgnoreProxy::TestImpl(
    const bool preference_value) {
  using Controller = NetworkPortalNotificationController;

  TestObserver observer;

  EnumHistogramChecker ui_checker(
      kNotificationMetric, Controller::NOTIFICATION_METRIC_COUNT, nullptr);
  EnumHistogramChecker action_checker(
      kUserActionMetric, Controller::USER_ACTION_METRIC_COUNT, nullptr);

  LoginUser(kTestUser);
  content::RunAllPendingInMessageLoop();

  SetIgnoreNoNetworkForTesting();

  ProfileManager::GetActiveUserProfile()->GetPrefs()->SetBoolean(
      prefs::kCaptivePortalAuthenticationIgnoresProxy, preference_value);

  // User connects to wifi.
  SetConnected(kWifiServicePath);

  EXPECT_EQ(PortalDetectorStrategy::STRATEGY_ID_SESSION, strategy()->Id());

  // No notification until portal detection is completed.
  EXPECT_FALSE(message_center()->FindVisibleNotificationById(kNotificationId));
  RestartDetection();
  CompleteURLFetch(net::OK, 200, nullptr);

  // Check that WiFi is marked as behind a portal and that a notification
  // is displayed.
  EXPECT_TRUE(message_center()->FindVisibleNotificationById(kNotificationId));
  EXPECT_EQ(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL,
            network_portal_detector::GetInstance()
                ->GetCaptivePortalState(kWifiGuid)
                .status);

  // Wait until notification is displayed.
  observer.WaitAndReset();

  EXPECT_TRUE(
      ui_checker.Expect(Controller::NOTIFICATION_METRIC_DISPLAYED, 1)->Check());
  EXPECT_TRUE(action_checker.Check());

  message_center()->ClickOnNotification(kNotificationId);

  content::RunAllPendingInMessageLoop();

  EXPECT_EQ(preference_value, static_cast<bool>(GetDialog()));
}

IN_PROC_BROWSER_TEST_P(NetworkPortalDetectorImplBrowserTestIgnoreProxy,
                       PRE_TestWithPreference) {
  RegisterUser(kTestUser);
  StartupUtils::MarkOobeCompleted();
  EXPECT_EQ(PortalDetectorStrategy::STRATEGY_ID_LOGIN_SCREEN, strategy()->Id());
}

IN_PROC_BROWSER_TEST_P(NetworkPortalDetectorImplBrowserTestIgnoreProxy,
                       TestWithPreference) {
  TestImpl(GetParam());
}

INSTANTIATE_TEST_CASE_P(CaptivePortalAuthenticationIgnoresProxy,
                        NetworkPortalDetectorImplBrowserTestIgnoreProxy,
                        testing::Bool());

}  // namespace chromeos
