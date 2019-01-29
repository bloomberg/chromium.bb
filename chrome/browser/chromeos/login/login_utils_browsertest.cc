// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/public/cpp/ash_features.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/screens/gaia_view.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "google_apis/gaia/fake_gaia.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"
#include "rlz/buildflags/buildflags.h"
#include "ui/base/ui_base_features.h"

#if BUILDFLAG(ENABLE_RLZ)
#include "base/task/post_task.h"
#include "components/rlz/rlz_tracker.h"
#endif

#if defined(GOOGLE_CHROME_BUILD)
#include "apps/test/app_window_waiter.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/common/extensions/extension_constants.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window.h"
#endif

namespace chromeos {

namespace {

#if BUILDFLAG(ENABLE_RLZ)
void GetAccessPointRlzInBackgroundThread(rlz_lib::AccessPoint point,
                                         base::string16* rlz) {
  ASSERT_FALSE(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  ASSERT_TRUE(rlz::RLZTracker::GetAccessPointRlz(point, rlz));
}
#endif

}  // namespace

class LoginUtilsTest : public OobeBaseTest {
 public:
  LoginUtilsTest() {}

  void RunUntilIdle() { base::RunLoop().RunUntilIdle(); }

  PrefService* local_state() { return g_browser_process->local_state(); }

  void Login(const std::string& username) {
    content::WindowedNotificationObserver session_started_observer(
        chrome::NOTIFICATION_SESSION_STARTED,
        content::NotificationService::AllSources());

    LoginDisplayHost::default_host()
        ->GetOobeUI()
        ->GetGaiaScreenView()
        ->ShowSigninScreenForTest(username, "password", "[]");

    // Wait for the session to start after submitting the credentials. This
    // will wait until all the background requests are done.
    session_started_observer.Wait();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginUtilsTest);
};

#if defined(GOOGLE_CHROME_BUILD)
class LoginUtilsContainedShellTest : public LoginUtilsTest {
 public:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(ash::features::kContainedShell);
    LoginUtilsTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// This observer is used by LoginUtilsContainedShellTest to keep track of
// whether a Fullscreen window was launched by ash during the test run.
class FullscreenWindowEnvObserver : public aura::EnvObserver,
                                    public aura::WindowObserver {
 public:
  FullscreenWindowEnvObserver() { env_observer_.Add(aura::Env::GetInstance()); }

  bool did_fullscreen_window_launch() const {
    return did_fullscreen_window_launch_;
  }

  // aura::EnvObserver:
  void OnWindowInitialized(aura::Window* window) override {
    window_observer_.Add(window);
  }

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override {
    did_fullscreen_window_launch_ =
        did_fullscreen_window_launch_ ||
        window->GetProperty(aura::client::kShowStateKey) ==
            ui::SHOW_STATE_FULLSCREEN;
  }

 private:
  bool did_fullscreen_window_launch_ = false;

  ScopedObserver<aura::Env, aura::EnvObserver> env_observer_{this};
  ScopedObserver<aura::Window, aura::WindowObserver> window_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(FullscreenWindowEnvObserver);
};

// Checks that the Contained Experience window is launched on sign-in when the
// feature is enabled.
IN_PROC_BROWSER_TEST_F(LoginUtilsContainedShellTest, ContainedShellLaunch) {
  // Enable all component extensions.
  extensions::ComponentLoader::EnableBackgroundExtensionsForTesting();

  WaitForSigninScreen();

  FullscreenWindowEnvObserver fullscreen_observer;

  Login("username");

  base::RunLoop().RunUntilIdle();

  // Wait for the app to launch before verifying it is fullscreen.
  apps::AppWindowWaiter waiter(
      extensions::AppWindowRegistry::Get(ProfileHelper::Get()->GetProfileByUser(
          user_manager::UserManager::Get()->GetActiveUser())),
      extension_misc::kContainedHomeAppId);
  waiter.WaitForShown();

  EXPECT_TRUE(fullscreen_observer.did_fullscreen_window_launch());
}
#endif  // defined(GOOGLE_CHROME_BUILD)

// Exercises login, like the desktopui_MashLogin Chrome OS autotest.
IN_PROC_BROWSER_TEST_F(LoginUtilsTest, MashLogin) {
  // Test is relevant for both SingleProcessMash and MultiProcessMash, but
  // not classic ash.
  if (!features::IsUsingWindowService())
    return;

  WaitForSigninScreen();
  Login("username");
  // Login did not time out and did not crash.
}

#if BUILDFLAG(ENABLE_RLZ)
IN_PROC_BROWSER_TEST_F(LoginUtilsTest, RlzInitialized) {
  WaitForSigninScreen();

  // No RLZ brand code set initially.
  EXPECT_FALSE(local_state()->HasPrefPath(prefs::kRLZBrand));

  // Wait for blocking RLZ tasks to complete.
  {
    base::RunLoop loop;
    PrefChangeRegistrar registrar;
    registrar.Init(local_state());
    registrar.Add(prefs::kRLZBrand, loop.QuitClosure());
    Login("username");
    loop.Run();
  }

  // RLZ brand code has been set to empty string.
  EXPECT_TRUE(local_state()->HasPrefPath(prefs::kRLZBrand));
  EXPECT_EQ(std::string(), local_state()->GetString(prefs::kRLZBrand));

  // RLZ value for homepage access point should have been initialized.
  // This value must be obtained in a background thread.
  {
    base::RunLoop loop;
    base::string16 rlz_string;
    base::PostTaskWithTraitsAndReply(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::Bind(&GetAccessPointRlzInBackgroundThread,
                   rlz::RLZTracker::ChromeHomePage(), &rlz_string),
        loop.QuitClosure());
    loop.Run();
    EXPECT_EQ(base::string16(), rlz_string);
  }
}
#endif

}  // namespace chromeos
