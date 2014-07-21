// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "base/timer/timer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/screenshot_tester.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "ui/compositor/compositor_switches.h"

namespace chromeos {

namespace {

const char kTestUser1[] = "test-user1@gmail.com";
const char kTestUser2[] = "test-user2@gmail.com";

// A class that provides a way to wait until all the animation
// has loaded and is properly shown on the screen.
class AnimationDelayHandler : public content::NotificationObserver {
 public:
  AnimationDelayHandler();

  // Should be run as early as possible on order not to miss notifications.
  // It seems though that it can't be moved to constructor(?).
  void Initialize();

  // Override from content::NotificationObserver.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // This method checks if animation is loaded, and, if not,
  // waits until it is loaded and properly shown on the screen.
  void WaitUntilAnimationLoads();

 private:
  void InitializeForWaiting(const base::Closure& quitter);

  // It turns out that it takes some more time for the animation
  // to finish loading even after all the notifications have been sent.
  // That happens due to some properties of compositor.
  // This method should be used after getting all the necessary notifications
  // to wait for the actual load of animation.
  void SynchronizeAnimationLoadWithCompositor();

  // This method exists only because of the current implementation of
  // SynchronizeAnimationLoadWithCompositor.
  void HandleAnimationLoad();

  // Returns true if, according to the notificatons received, animation has
  // finished loading by now.
  bool IsAnimationLoaded();

  base::OneShotTimer<AnimationDelayHandler> timer_;
  bool waiter_loop_is_on_;
  bool login_or_lock_webui_visible_;
  base::Closure animation_waiter_quitter_;
  content::NotificationRegistrar registrar_;
};

}  // anonymous namespace

AnimationDelayHandler::AnimationDelayHandler()
    : waiter_loop_is_on_(false), login_or_lock_webui_visible_(false) {
}

void AnimationDelayHandler::Initialize() {
  waiter_loop_is_on_ = false;
  registrar_.Add(this,
                 chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
                 content::NotificationService::AllSources());
}

bool AnimationDelayHandler::IsAnimationLoaded() {
  return login_or_lock_webui_visible_;
}

void AnimationDelayHandler::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE == type) {
    login_or_lock_webui_visible_ = true;
    registrar_.Remove(this,
                      chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
                      content::NotificationService::AllSources());
  }
  if (waiter_loop_is_on_ && IsAnimationLoaded()) {
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE, animation_waiter_quitter_);
  }
}

void AnimationDelayHandler::InitializeForWaiting(const base::Closure& quitter) {
  waiter_loop_is_on_ = true;
  animation_waiter_quitter_ = quitter;
}

void AnimationDelayHandler::HandleAnimationLoad() {
  timer_.Stop();
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE, animation_waiter_quitter_);
}

// Current implementation is a mockup.
// It simply waits for 5 seconds, assuming that this time is enough for
// animation to load completely.
// TODO(elizavetai): Replace this temporary hack with getting a
// valid notification from compositor.
void AnimationDelayHandler::SynchronizeAnimationLoadWithCompositor() {
  base::RunLoop waiter;
  animation_waiter_quitter_ = waiter.QuitClosure();
  timer_.Start(FROM_HERE,
               base::TimeDelta::FromSeconds(5),
               this,
               &AnimationDelayHandler::HandleAnimationLoad);
  waiter.Run();
}

void AnimationDelayHandler::WaitUntilAnimationLoads() {
  if (!IsAnimationLoaded()) {
    base::RunLoop animation_waiter;
    InitializeForWaiting(animation_waiter.QuitClosure());
    animation_waiter.Run();
  }
  SynchronizeAnimationLoadWithCompositor();
}

class LoginUITest : public chromeos::LoginManagerTest {
 public:
  bool enable_test_screenshots_;
  LoginUITest() : LoginManagerTest(false) {}
  virtual ~LoginUITest() {}
  virtual void SetUpOnMainThread() OVERRIDE {
    enable_test_screenshots_ = screenshot_tester.TryInitialize();
    if (enable_test_screenshots_) {
      animation_delay_handler.Initialize();
    } else {
      LOG(WARNING) << "Screenshots will not be taken";
    }
    LoginManagerTest::SetUpOnMainThread();
  }

 protected:
  AnimationDelayHandler animation_delay_handler;
  ScreenshotTester screenshot_tester;
};

IN_PROC_BROWSER_TEST_F(LoginUITest, PRE_LoginUIVisible) {
  RegisterUser(kTestUser1);
  RegisterUser(kTestUser2);
  StartupUtils::MarkOobeCompleted();
}

// Verifies basic login UI properties.
IN_PROC_BROWSER_TEST_F(LoginUITest, LoginUIVisible) {
  JSExpect("!!document.querySelector('#account-picker')");
  JSExpect("!!document.querySelector('#pod-row')");
  JSExpect(
      "document.querySelectorAll('.pod:not(#user-pod-template)').length == 2");

  JSExpect("document.querySelectorAll('.pod:not(#user-pod-template)')[0]"
           ".user.emailAddress == '" + std::string(kTestUser1) + "'");
  JSExpect("document.querySelectorAll('.pod:not(#user-pod-template)')[1]"
           ".user.emailAddress == '" + std::string(kTestUser2) + "'");
  if (enable_test_screenshots_) {
    animation_delay_handler.WaitUntilAnimationLoads();
    screenshot_tester.Run("LoginUITest-LoginUIVisible");
  }
}

IN_PROC_BROWSER_TEST_F(LoginUITest, PRE_InterruptedAutoStartEnrollment) {
  StartupUtils::MarkOobeCompleted();
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kDeviceEnrollmentAutoStart, true);
}

// Tests that the default first screen is the network screen after OOBE
// when auto enrollment is enabled and device is not yet enrolled.
IN_PROC_BROWSER_TEST_F(LoginUITest, InterruptedAutoStartEnrollment) {
  OobeScreenWaiter(OobeDisplay::SCREEN_OOBE_NETWORK).Wait();
}

}  // namespace chromeos
