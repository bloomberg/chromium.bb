// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/captive_portal/testing_utils.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/net/network_portal_detector.h"
#include "chrome/browser/chromeos/net/network_portal_detector_impl.h"
#include "chrome/browser/chromeos/net/network_portal_detector_strategy.h"
#include "chrome/browser/chromeos/net/network_portal_detector_test_utils.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_service_client.h"
#include "content/public/test/test_utils.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/message_center/message_center.h"

namespace chromeos {

namespace {

const char kTestUser[] = "test-user@gmail.com";
const char kWifi[] = "wifi";

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

}  // namespace

class NetworkPortalDetectorImplBrowserTest
    : public LoginManagerTest,
      public captive_portal::CaptivePortalDetectorTestBase {
 public:
  NetworkPortalDetectorImplBrowserTest()
      : LoginManagerTest(false), network_portal_detector_(NULL) {}
  virtual ~NetworkPortalDetectorImplBrowserTest() {}

  virtual void SetUpOnMainThread() OVERRIDE {
    LoginManagerTest::SetUpOnMainThread();

    ShillServiceClient::TestInterface* service_test =
        DBusThreadManager::Get()->GetShillServiceClient()->GetTestInterface();
    service_test->ClearServices();
    service_test->AddService(kWifi,
                             kWifi,
                             shill::kTypeEthernet,
                             shill::kStateIdle,
                             true /* add_to_visible */,
                             true /* add_to_watchlist */);
    DBusThreadManager::Get()->GetShillServiceClient()->SetProperty(
        dbus::ObjectPath(kWifi),
        shill::kStateProperty,
        base::StringValue(shill::kStatePortal),
        base::Bind(&base::DoNothing),
        base::Bind(&ErrorCallbackFunction));

    network_portal_detector_ = new NetworkPortalDetectorImpl(
        g_browser_process->system_request_context());
    NetworkPortalDetector::InitializeForTesting(network_portal_detector_);
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

  message_center::MessageCenter* message_center() {
    return message_center::MessageCenter::Get();
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
  const char* kNotificationId = Controller::kNotificationId;
  const char* kNotificationMetric = Controller::kNotificationMetric;
  const char* kUserActionMetric = Controller::kUserActionMetric;

  EnumHistogramChecker ui_checker(
      kNotificationMetric, Controller::NOTIFICATION_METRIC_COUNT, NULL);
  EnumHistogramChecker action_checker(
      kUserActionMetric, Controller::USER_ACTION_METRIC_COUNT, NULL);

  LoginUser(kTestUser);
  content::RunAllPendingInMessageLoop();

  // User connects to wifi.
  SetConnected(kWifi);

  ASSERT_EQ(PortalDetectorStrategy::STRATEGY_ID_SESSION, strategy()->Id());

  // No notification until portal detection is completed.
  ASSERT_FALSE(message_center()->HasNotification(kNotificationId));
  RestartDetection();
  CompleteURLFetch(net::OK, 200, NULL);

  // Check that wifi is marked as behind the portal and that notification
  // is displayed.
  ASSERT_TRUE(message_center()->HasNotification(kNotificationId));
  ASSERT_EQ(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL,
            NetworkPortalDetector::Get()->GetCaptivePortalState(kWifi).status);

  ASSERT_TRUE(
      ui_checker.Expect(Controller::NOTIFICATION_METRIC_DISPLAYED, 1)->Check());
  ASSERT_TRUE(action_checker.Check());

  // User explicitly closes the notification.
  message_center()->RemoveNotification(kNotificationId, true /* by_user */);

  ASSERT_TRUE(ui_checker.Check());
  ASSERT_TRUE(
      action_checker.Expect(Controller::USER_ACTION_METRIC_CLOSED, 1)->Check());
}

}  // namespace chromeos
