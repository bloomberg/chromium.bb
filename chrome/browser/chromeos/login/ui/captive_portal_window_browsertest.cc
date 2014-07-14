// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/ui/captive_portal_window_proxy.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/ui/webui_login_view.h"
#include "chrome/browser/chromeos/net/network_portal_detector_test_impl.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/fake_shill_manager_client.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"

namespace chromeos {

namespace {

// Stub implementation of CaptivePortalWindowProxyDelegate, does
// nothing and used to instantiate CaptivePortalWindowProxy.
class CaptivePortalWindowProxyStubDelegate
    : public CaptivePortalWindowProxyDelegate {
 public:
  CaptivePortalWindowProxyStubDelegate(): num_portal_notifications_(0) {
  }

  virtual ~CaptivePortalWindowProxyStubDelegate() {
  }

  virtual void OnPortalDetected() OVERRIDE {
    ++num_portal_notifications_;
  }

  int num_portal_notifications() const { return num_portal_notifications_; }

 private:
  int num_portal_notifications_;
};

}  // namespace

class CaptivePortalWindowTest : public InProcessBrowserTest {
 protected:
  void ShowIfRedirected() {
    captive_portal_window_proxy_->ShowIfRedirected();
  }

  void Show() {
    captive_portal_window_proxy_->Show();
  }

  void Close() {
    captive_portal_window_proxy_->Close();
  }

  void OnRedirected() {
    captive_portal_window_proxy_->OnRedirected();
  }

  void OnOriginalURLLoaded() {
    captive_portal_window_proxy_->OnOriginalURLLoaded();
  }

  void CheckState(bool is_shown, int num_portal_notifications) {
    bool actual_is_shown = (CaptivePortalWindowProxy::STATE_DISPLAYED ==
                            captive_portal_window_proxy_->GetState());
    ASSERT_EQ(is_shown, actual_is_shown);
    ASSERT_EQ(num_portal_notifications, delegate_.num_portal_notifications());
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(chromeos::switches::kForceLoginManagerInTests);
    command_line->AppendSwitch(chromeos::switches::kLoginManager);
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    host_ = LoginDisplayHostImpl::default_host();
    CHECK(host_);
    content::WebContents* web_contents =
        LoginDisplayHostImpl::default_host()->GetWebUILoginView()->
            GetWebContents();
    captive_portal_window_proxy_.reset(
        new CaptivePortalWindowProxy(&delegate_, web_contents));
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    captive_portal_window_proxy_.reset();
    base::MessageLoopForUI::current()->DeleteSoon(FROM_HERE, host_);
    base::MessageLoopForUI::current()->RunUntilIdle();
  }

 private:
  scoped_ptr<CaptivePortalWindowProxy> captive_portal_window_proxy_;
  CaptivePortalWindowProxyStubDelegate delegate_;

  LoginDisplayHost* host_;
};

IN_PROC_BROWSER_TEST_F(CaptivePortalWindowTest, Show) {
  Show();
}

IN_PROC_BROWSER_TEST_F(CaptivePortalWindowTest, ShowClose) {
  CheckState(false, 0);

  Show();
  CheckState(true, 0);

  Close();
  CheckState(false, 0);
}

IN_PROC_BROWSER_TEST_F(CaptivePortalWindowTest, OnRedirected) {
  CheckState(false, 0);

  ShowIfRedirected();
  CheckState(false, 0);

  OnRedirected();
  CheckState(true, 1);

  Close();
  CheckState(false, 1);
}

IN_PROC_BROWSER_TEST_F(CaptivePortalWindowTest, OnOriginalURLLoaded) {
  CheckState(false, 0);

  ShowIfRedirected();
  CheckState(false, 0);

  OnRedirected();
  CheckState(true, 1);

  OnOriginalURLLoaded();
  CheckState(false, 1);
}

IN_PROC_BROWSER_TEST_F(CaptivePortalWindowTest, MultipleCalls) {
  CheckState(false, 0);

  ShowIfRedirected();
  CheckState(false, 0);

  Show();
  CheckState(true, 0);

  Close();
  CheckState(false, 0);

  OnRedirected();
  CheckState(false, 1);

  OnOriginalURLLoaded();
  CheckState(false, 1);

  Show();
  CheckState(true, 1);

  OnRedirected();
  CheckState(true, 2);

  Close();
  CheckState(false, 2);

  OnOriginalURLLoaded();
  CheckState(false, 2);
}

class CaptivePortalWindowCtorDtorTest : public LoginManagerTest {
 public:
  CaptivePortalWindowCtorDtorTest()
      : LoginManagerTest(false) {}
  virtual ~CaptivePortalWindowCtorDtorTest() {}

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    LoginManagerTest::SetUpInProcessBrowserTestFixture();

    network_portal_detector_ = new NetworkPortalDetectorTestImpl();
    NetworkPortalDetector::InitializeForTesting(network_portal_detector_);
    NetworkPortalDetector::CaptivePortalState portal_state;
    portal_state.status = NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL;
    portal_state.response_code = 200;
    network_portal_detector_->SetDefaultNetworkForTesting(
        FakeShillManagerClient::kFakeEthernetNetworkGuid);
    network_portal_detector_->SetDetectionResultsForTesting(
        FakeShillManagerClient::kFakeEthernetNetworkGuid,
        portal_state);
  }

 protected:
  NetworkPortalDetectorTestImpl* network_portal_detector() {
    return network_portal_detector_;
  }

  PortalDetectorStrategy::StrategyId strategy_id() {
    return network_portal_detector_->strategy_id();
  }

 private:
  NetworkPortalDetectorTestImpl* network_portal_detector_;

  DISALLOW_COPY_AND_ASSIGN(CaptivePortalWindowCtorDtorTest);
};

IN_PROC_BROWSER_TEST_F(CaptivePortalWindowCtorDtorTest, PRE_OpenPortalDialog) {
  StartupUtils::MarkOobeCompleted();
}

IN_PROC_BROWSER_TEST_F(CaptivePortalWindowCtorDtorTest, OpenPortalDialog) {
  LoginDisplayHostImpl* host =
      static_cast<LoginDisplayHostImpl*>(LoginDisplayHostImpl::default_host());
  ASSERT_TRUE(host);
  OobeUI* oobe = host->GetOobeUI();
  ASSERT_TRUE(oobe);
  ErrorScreenActor* actor = oobe->GetErrorScreenActor();
  ASSERT_TRUE(actor);

  // Error screen asks portal detector to change detection strategy.
  ErrorScreen error_screen(NULL, actor);

  ASSERT_EQ(PortalDetectorStrategy::STRATEGY_ID_LOGIN_SCREEN, strategy_id());
  network_portal_detector()->NotifyObserversForTesting();
  OobeScreenWaiter(OobeDisplay::SCREEN_ERROR_MESSAGE).Wait();
  ASSERT_EQ(PortalDetectorStrategy::STRATEGY_ID_ERROR_SCREEN, strategy_id());

  actor->ShowCaptivePortal();
}

}  // namespace chromeos
