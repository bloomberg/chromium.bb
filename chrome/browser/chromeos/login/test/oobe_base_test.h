// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_TEST_OOBE_BASE_TEST_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_TEST_OOBE_BASE_TEST_H_

#include <string>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/login/mixin_based_in_process_browser_test.h"
#include "chrome/browser/chromeos/login/test/https_forwarder.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/test_utils.h"
#include "google_apis/gaia/fake_gaia.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace content {
class WebUI;
class WindowedNotificationObserver;
}  // namespace content

namespace chromeos {

class NetworkPortalDetectorTestImpl;

// Base class for OOBE, login, SAML and Kiosk tests.
class OobeBaseTest : public extensions::ExtensionApiTest {
 public:
  // Default fake user email and password, may be used by tests.

  static const char kFakeUserEmail[];
  static const char kFakeUserPassword[];
  static const char kFakeUserGaiaId[];
  static const char kEmptyUserServices[];

  // FakeGaia is configured to return these cookies for kFakeUserEmail.
  static const char kFakeSIDCookie[];
  static const char kFakeLSIDCookie[];

  OobeBaseTest();
  ~OobeBaseTest() override;

  // Subclasses may register their own custom request handlers that will
  // process requests prior it gets handled by FakeGaia instance.
  virtual void RegisterAdditionalRequestHandlers();

 protected:
  // InProcessBrowserTest:
  void SetUp() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override;
  void SetUpInProcessBrowserTestFixture() override;
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;
  void TearDownInProcessBrowserTestFixture() override;
  void TearDown() override;

  virtual void InitHttpsForwarders();

  // If this returns true (default), the |ash::switches::kShowWebUiLogin|
  // command-line switch is passed to force the Web Ui Login.
  // If this returns false, the switch is omitted so the views-based login may
  // be used.
  virtual bool ShouldForceWebUiLogin();

  // Network status control functions.
  void SimulateNetworkOffline();
  void SimulateNetworkOnline();
  void SimulateNetworkPortal();

  base::Closure SimulateNetworkOfflineClosure();
  base::Closure SimulateNetworkOnlineClosure();
  base::Closure SimulateNetworkPortalClosure();

  bool initialize_fake_merge_session() {
    return initialize_fake_merge_session_;
  }
  void set_initialize_fake_merge_session(bool value) {
    initialize_fake_merge_session_ = value;
  }

  // Returns chrome://oobe WebUI.
  content::WebUI* GetLoginUI();

  void WaitForGaiaPageLoad();
  void WaitForGaiaPageLoadAndPropertyUpdate();
  void WaitForGaiaPageReload();
  void WaitForGaiaPageBackButtonUpdate();
  void WaitForGaiaPageEvent(const std::string& event);
  void WaitForSigninScreen();
  void ExecuteJsInSigninFrame(const std::string& js);
  void SetSignFormField(const std::string& field_id,
                        const std::string& field_value);

  // Sets up fake gaia for the login code:
  // - Maps |user_email| to |gaia_id|. If |gaia_id| is empty, |user_email| will
  //   be mapped to kDefaultGaiaId in FakeGaia;
  // - Issues a special all-scope access token associated with the test refresh
  //   token;
  void SetupFakeGaiaForLogin(const std::string& user_email,
                             const std::string& gaia_id,
                             const std::string& refresh_token);

  InProcessBrowserTestMixinHost mixin_host_;

  std::unique_ptr<FakeGaia> fake_gaia_;
  NetworkPortalDetectorTestImpl* network_portal_detector_ = nullptr;

  // Whether to use background networking. Note this is only effective when it
  // is set before SetUpCommandLine is invoked.
  bool needs_background_networking_ = false;

  std::unique_ptr<content::WindowedNotificationObserver>
      login_screen_load_observer_;
  HTTPSForwarder gaia_https_forwarder_;
  std::string gaia_frame_parent_ = "signin-frame";
  bool initialize_fake_merge_session_ = true;

  DISALLOW_COPY_AND_ASSIGN(OobeBaseTest);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_TEST_OOBE_BASE_TEST_H_
