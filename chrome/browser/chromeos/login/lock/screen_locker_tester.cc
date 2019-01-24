// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/lock/screen_locker_tester.h"

#include <string>

#include "ash/public/cpp/ash_switches.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "ash/public/interfaces/login_screen_test_api.test-mojom-test-utils.h"
#include "ash/public/interfaces/login_screen_test_api.test-mojom.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/lock/screen_locker.h"
#include "chrome/browser/chromeos/login/lock/webui_screen_locker.h"
#include "chromeos/login/auth/auth_status_consumer.h"
#include "chromeos/login/auth/fake_extended_authenticator.h"
#include "chromeos/login/auth/key.h"
#include "chromeos/login/auth/stub_authenticator.h"
#include "chromeos/login/auth/user_context.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/session_manager_types.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/test/test_utils.h"
#include "services/service_manager/public/cpp/connector.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace {

bool IsScreenLockerLocked() {
  return ScreenLocker::default_screen_locker() &&
         ScreenLocker::default_screen_locker()->locked();
}

// This class is used to observe state of the global ScreenLocker instance,
// which can go away as a result of a successful authentication. As such,
// it needs to directly reference the global ScreenLocker.
class LoginAttemptObserver : public AuthStatusConsumer {
 public:
  LoginAttemptObserver() : AuthStatusConsumer() {
    ScreenLocker::default_screen_locker()->SetLoginStatusConsumer(this);
  }
  ~LoginAttemptObserver() override {
    if (ScreenLocker::default_screen_locker())
      ScreenLocker::default_screen_locker()->SetLoginStatusConsumer(nullptr);
  }

  void WaitForAttempt() {
    if (!login_attempted_) {
      run_loop_ = std::make_unique<base::RunLoop>();
      run_loop_->Run();
      run_loop_.release();
    }
    ASSERT_TRUE(login_attempted_);
  }

  // AuthStatusConsumer:
  void OnAuthFailure(const AuthFailure& error) override { LoginAttempted(); }
  void OnAuthSuccess(const UserContext& credentials) override {
    LoginAttempted();
  }

 private:
  void LoginAttempted() {
    login_attempted_ = true;
    if (run_loop_)
      run_loop_->QuitWhenIdle();
  }

  bool login_attempted_ = false;
  std::unique_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(LoginAttemptObserver);
};

class WebUIScreenLockerTester : public ScreenLockerTester {
 public:
  WebUIScreenLockerTester() = default;
  ~WebUIScreenLockerTester() override = default;

  // ScreenLockerTester:
  bool IsLocked() override { return IsScreenLockerLocked(); }
  void UnlockWithPassword(const AccountId& account_id,
                          const std::string& password) override {
    bool result;

    SetPassword(password);

    // Verify password is set.
    ASSERT_EQ(password, GetPassword());

    // Verify that "reauth" warning is hidden.
    std::unique_ptr<base::Value> v = content::ExecuteScriptAndGetValue(
        GetMainFrame(),
        "window.getComputedStyle("
        "    $('pod-row').pods[0].querySelector('.reauth-hint-container'))"
        "        .display == 'none'");
    ASSERT_TRUE(v->GetAsBoolean(&result));
    ASSERT_TRUE(result);

    // Attempt to sign in.
    LoginAttemptObserver login;
    v = content::ExecuteScriptAndGetValue(GetMainFrame(),
                                          "$('pod-row').pods[0].activate();");
    ASSERT_TRUE(v->GetAsBoolean(&result));
    ASSERT_TRUE(result);

    // Wait for login attempt.
    login.WaitForAttempt();
  }

 private:
  void SetPassword(const std::string& password) {
    GetMainFrame()->ExecuteJavaScriptForTests(base::ASCIIToUTF16(
        base::StringPrintf("$('pod-row').pods[0].passwordElement.value = '%s';",
                           password.c_str())));
  }

  std::string GetPassword() {
    std::string result;
    std::unique_ptr<base::Value> v = content::ExecuteScriptAndGetValue(
        GetMainFrame(), "$('pod-row').pods[0].passwordElement.value;");
    CHECK(v->GetAsString(&result));
    return result;
  }

  content::RenderFrameHost* GetMainFrame() {
    return webui()->GetWebContents()->GetMainFrame();
  }

  // Returns the ScreenLockerWebUI object.
  WebUIScreenLocker* webui_screen_locker() {
    DCHECK(ScreenLocker::default_screen_locker());
    return ScreenLocker::default_screen_locker()->web_ui_for_testing();
  }

  // Returns the WebUI object from the screen locker.
  content::WebUI* webui() {
    DCHECK(webui_screen_locker()->webui_ready_for_testing());
    content::WebUI* webui = webui_screen_locker()->GetWebUI();
    DCHECK(webui);
    return webui;
  }

  DISALLOW_COPY_AND_ASSIGN(WebUIScreenLockerTester);
};

class MojoScreenLockerTester : public ScreenLockerTester {
 public:
  MojoScreenLockerTester() {
    content::ServiceManagerConnection::GetForProcess()
        ->GetConnector()
        ->BindInterface(ash::mojom::kServiceName, &test_api_);
  }
  ~MojoScreenLockerTester() override = default;

  // ScreenLockerTester:
  bool IsLocked() override {
    // Check from ash's perspective.
    ash::mojom::LoginScreenTestApiAsyncWaiter login_screen(test_api_.get());
    bool is_ui_shown;
    login_screen.IsLockShown(&is_ui_shown);
    return IsScreenLockerLocked() && is_ui_shown;
  }
  void UnlockWithPassword(const AccountId& account_id,
                          const std::string& password) override {
    ash::mojom::LoginScreenTestApiAsyncWaiter login_screen(test_api_.get());
    login_screen.SubmitPassword(account_id, password);
    base::RunLoop().RunUntilIdle();
  }

 private:
  ash::mojom::LoginScreenTestApiPtr test_api_;

  DISALLOW_COPY_AND_ASSIGN(MojoScreenLockerTester);
};

}  // namespace

// static
std::unique_ptr<ScreenLockerTester> ScreenLockerTester::Create() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kShowWebUiLock)) {
    return std::make_unique<WebUIScreenLockerTester>();
  }
  return std::make_unique<MojoScreenLockerTester>();
}

ScreenLockerTester::ScreenLockerTester() = default;

ScreenLockerTester::~ScreenLockerTester() = default;

void ScreenLockerTester::Lock() {
  content::WindowedNotificationObserver lock_state_observer(
      chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED,
      content::NotificationService::AllSources());
  ScreenLocker::Show();
  if (!IsLocked())
    lock_state_observer.Wait();
  ASSERT_TRUE(IsLocked());
  ASSERT_EQ(session_manager::SessionState::LOCKED,
            session_manager::SessionManager::Get()->session_state());
  base::RunLoop().RunUntilIdle();
}

void ScreenLockerTester::SetUnlockPassword(const AccountId& account_id,
                                           const std::string& password) {
  UserContext user_context(user_manager::UserType::USER_TYPE_REGULAR,
                           account_id);
  user_context.SetKey(Key(password));

  auto* locker = ScreenLocker::default_screen_locker();
  locker->SetAuthenticatorsForTesting(
      base::MakeRefCounted<StubAuthenticator>(locker, user_context),
      base::MakeRefCounted<FakeExtendedAuthenticator>(locker, user_context));
}

}  // namespace chromeos
