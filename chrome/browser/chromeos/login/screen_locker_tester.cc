// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screen_locker_tester.h"

#include <string>

#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/login_status_consumer.h"
#include "chrome/browser/chromeos/login/mock_authenticator.h"
#include "chrome/browser/chromeos/login/screen_locker.h"
#include "chrome/browser/chromeos/login/webui_screen_locker.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/test_utils.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/widget/root_view.h"

using content::WebContents;

namespace {

class LoginAttemptObserver : public chromeos::LoginStatusConsumer {
 public:
  explicit LoginAttemptObserver(chromeos::ScreenLocker* locker);
  virtual ~LoginAttemptObserver();

  void WaitForAttempt();

  // Overridden from LoginStatusConsumer:
  virtual void OnLoginFailure(const chromeos::LoginFailure& error) OVERRIDE {
    LoginAttempted();
  }

  virtual void OnLoginSuccess(
      const chromeos::UserContext& credentials,
      bool pending_requests,
      bool using_oauth) OVERRIDE {
    LoginAttempted();
  }

 private:
  void LoginAttempted();

  chromeos::ScreenLocker* locker_;
  bool login_attempted_;
  bool waiting_;

  DISALLOW_COPY_AND_ASSIGN(LoginAttemptObserver);
};

LoginAttemptObserver::LoginAttemptObserver(chromeos::ScreenLocker* locker)
    : chromeos::LoginStatusConsumer(),
      locker_(locker),
      login_attempted_(false),
      waiting_(false) {
  locker_->SetLoginStatusConsumer(this);
}

LoginAttemptObserver::~LoginAttemptObserver() {
  locker_->SetLoginStatusConsumer(NULL);
}

void LoginAttemptObserver::WaitForAttempt() {
  if (!login_attempted_) {
    waiting_ = true;
    content::RunMessageLoop();
    waiting_ = false;
  }
  ASSERT_TRUE(login_attempted_);
}

void LoginAttemptObserver::LoginAttempted() {
  login_attempted_ = true;
  if (waiting_)
    MessageLoopForUI::current()->Quit();
}

}

namespace chromeos {

namespace test {

class WebUIScreenLockerTester : public ScreenLockerTester {
 public:
  // ScreenLockerTester overrides:
  virtual void SetPassword(const std::string& password) OVERRIDE;
  virtual std::string GetPassword() OVERRIDE;
  virtual void EnterPassword(const std::string& password) OVERRIDE;
  virtual void EmulateWindowManagerReady() OVERRIDE;
  virtual views::Widget* GetWidget() const OVERRIDE;
  virtual views::Widget* GetChildWidget() const OVERRIDE;

 private:
  friend class chromeos::ScreenLocker;

  WebUIScreenLockerTester() {}

  content::RenderViewHost* RenderViewHost() const;

  // Returns the ScreenLockerWebUI object.
  WebUIScreenLocker* webui_screen_locker() const;

  // Returns the WebUI object from the screen locker.
  content::WebUI* webui() const;

  DISALLOW_COPY_AND_ASSIGN(WebUIScreenLockerTester);
};

void WebUIScreenLockerTester::SetPassword(const std::string& password) {
  RenderViewHost()->ExecuteJavascriptInWebFrame(
      string16(),
      ASCIIToUTF16(base::StringPrintf(
          "$('pod-row').pods[0].passwordElement.value = '%s';",
          password.c_str())));
}

std::string WebUIScreenLockerTester::GetPassword() {
  std::string result;
  scoped_ptr<base::Value> v = content::ExecuteScriptAndGetValue(
      RenderViewHost(),
      "$('pod-row').pods[0].passwordElement.value;");
  CHECK(v->GetAsString(&result));
  return result;
}

void WebUIScreenLockerTester::EnterPassword(const std::string& password) {
  bool result;
  SetPassword(password);

  // Verify password is set.
  ASSERT_EQ(password, GetPassword());

  // Verify that "signin" button is hidden.
  scoped_ptr<base::Value> v = content::ExecuteScriptAndGetValue(
      RenderViewHost(),
      "$('pod-row').pods[0].signinButtonElement.hidden;");
  ASSERT_TRUE(v->GetAsBoolean(&result));
  ASSERT_TRUE(result);

  // Attempt to sign in.
  LoginAttemptObserver login(ScreenLocker::screen_locker_);
  v = content::ExecuteScriptAndGetValue(
      RenderViewHost(),
      "$('pod-row').pods[0].activate();");
  ASSERT_TRUE(v->GetAsBoolean(&result));
  ASSERT_TRUE(result);

  // Wait for login attempt.
  login.WaitForAttempt();
}

void WebUIScreenLockerTester::EmulateWindowManagerReady() {
}

views::Widget* WebUIScreenLockerTester::GetWidget() const {
  return webui_screen_locker()->lock_window_;
}

views::Widget* WebUIScreenLockerTester::GetChildWidget() const {
  return webui_screen_locker()->lock_window_;
}

content::RenderViewHost* WebUIScreenLockerTester::RenderViewHost() const {
  return webui()->GetWebContents()->GetRenderViewHost();
}

WebUIScreenLocker* WebUIScreenLockerTester::webui_screen_locker() const {
  DCHECK(ScreenLocker::screen_locker_);
  return static_cast<WebUIScreenLocker*>(
      ScreenLocker::screen_locker_->delegate_.get());
}

content::WebUI* WebUIScreenLockerTester::webui() const {
  DCHECK(webui_screen_locker()->webui_ready_);
  content::WebUI* webui = webui_screen_locker()->GetWebUI();
  DCHECK(webui);
  return webui;
}

ScreenLockerTester::ScreenLockerTester() {
}

ScreenLockerTester::~ScreenLockerTester() {
}

bool ScreenLockerTester::IsLocked() {
  return ScreenLocker::screen_locker_ &&
      ScreenLocker::screen_locker_->locked_;
}

void ScreenLockerTester::InjectMockAuthenticator(
    const std::string& user, const std::string& password) {
  DCHECK(ScreenLocker::screen_locker_);
  ScreenLocker::screen_locker_->SetAuthenticator(
      new MockAuthenticator(ScreenLocker::screen_locker_, user, password));
}

}  // namespace test

test::ScreenLockerTester* ScreenLocker::GetTester() {
  return new test::WebUIScreenLockerTester();
}

}  // namespace chromeos
