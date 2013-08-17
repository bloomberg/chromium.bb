// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/login/captive_portal_window_proxy.h"
#include "chrome/browser/chromeos/login/login_display_host_impl.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/chromeos_switches.h"

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
    CHECK(LoginDisplayHostImpl::default_host());
    gfx::NativeWindow native_window =
        LoginDisplayHostImpl::default_host()->GetNativeWindow();
    captive_portal_window_proxy_.reset(
        new CaptivePortalWindowProxy(&delegate_, native_window));
  }

 private:
  scoped_ptr<CaptivePortalWindowProxy> captive_portal_window_proxy_;
  CaptivePortalWindowProxyStubDelegate delegate_;
};

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

}  // namespace chromeos
