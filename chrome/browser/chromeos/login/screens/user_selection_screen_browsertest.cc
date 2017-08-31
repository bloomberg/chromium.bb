// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_cryptohome_client.h"

namespace chromeos {

namespace {

// Consumer user according to BrowserPolicyConnector::IsNonEnterpriseUser
// (@gmail.com).
constexpr char kTestUser1[] = "test-user1@gmail.com";
// Consumer user according to BrowserPolicyConnector::IsNonEnterpriseUser
// (@gmail.com).
constexpr char kTestUser2[] = "test-user2@gmail.com";
// No consumer user according to BrowserPolicyConnector::IsNonEnterpriseUser.
constexpr char kManagedTestUser[] = "manager@example.com";

}  // namespace

class UserSelectionScreenTest : public LoginManagerTest {
 public:
  UserSelectionScreenTest()
      : LoginManagerTest(false /* should_launch_browser */) {}
  ~UserSelectionScreenTest() override = default;

  // LoginManagerTest:
  void SetUpInProcessBrowserTestFixture() override {
    auto cryptohome_client = base::MakeUnique<chromeos::FakeCryptohomeClient>();
    fake_cryptohome_client_ = cryptohome_client.get();
    DBusThreadManager::GetSetterForTesting()->SetCryptohomeClient(
        std::move(cryptohome_client));
  }

  FakeCryptohomeClient* fake_cryptohome_client() {
    return fake_cryptohome_client_;
  }

  OobeUI* GetOobeUI() {
    return static_cast<OobeUI*>(web_contents()->GetWebUI()->GetController());
  }

  void FocusUserPod(int pod_id) {
    base::RunLoop pod_focus_wait_loop;
    GetOobeUI()->signin_screen_handler()->SetFocusPODCallbackForTesting(
        pod_focus_wait_loop.QuitClosure());
    js_checker().Evaluate(base::StringPrintf(
        "$('pod-row').focusPod($('pod-row').pods[%d])", pod_id));
    pod_focus_wait_loop.Run();
  }

 private:
  // DBusThreadManager owns this.
  FakeCryptohomeClient* fake_cryptohome_client_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(UserSelectionScreenTest);
};

IN_PROC_BROWSER_TEST_F(UserSelectionScreenTest,
                       PRE_ShowDircryptoMigrationBanner) {
  RegisterUser(kTestUser1);
  RegisterUser(kTestUser2);
  RegisterUser(kManagedTestUser);
  StartupUtils::MarkOobeCompleted();
}

// Test that a banner shows up for known-unmanaged users that need dircrypto
// migration. Also test that no banner shows up for users that may be managed.
IN_PROC_BROWSER_TEST_F(UserSelectionScreenTest, ShowDircryptoMigrationBanner) {
  // Enable ARC. Otherwise, the banner would not show.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kArcAvailability, "officially-supported");

  // No banner for the first user since default is no migration.
  JSExpect("!$('signin-banner').classList.contains('message-set')");

  // Change the needs dircrypto migration response.
  fake_cryptohome_client()->set_needs_dircrypto_migration(true);

  // Focus the 2nd user pod (consumer).
  FocusUserPod(1);

  // Wait for FakeCryptohomeClient to send back the check result.
  base::RunLoop().RunUntilIdle();

  // Banner should be shown for the 2nd user (consumer).
  JSExpect("$('signin-banner').classList.contains('message-set')");

  // Focus to the 3rd user pod (enterprise).
  FocusUserPod(2);

  // Wait for FakeCryptohomeClient to send back the check result.
  base::RunLoop().RunUntilIdle();

  // Banner should not be shown for the enterprise user.
  JSExpect("!$('signin-banner').classList.contains('message-set')");
}

}  // namespace chromeos
