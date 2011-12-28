// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screen_locker_tester.h"

#include <string>
#if defined(TOOLKIT_USES_GTK)
#include <gdk/gdkkeysyms.h>
#endif

#include "base/stringprintf.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/login_status_consumer.h"
#include "chrome/browser/chromeos/login/mock_authenticator.h"
#include "chrome/browser/chromeos/login/screen_locker.h"
#include "chrome/browser/chromeos/login/webui_screen_locker.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/webui/web_ui.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/events/event.h"
#include "ui/views/widget/root_view.h"


#if defined(TOOLKIT_USES_GTK)
#include "chrome/browser/chromeos/login/lock_window_gtk.h"
#include "chrome/browser/chromeos/login/screen_locker_views.h"
#include "chrome/browser/chromeos/login/screen_lock_view.h"
#endif

namespace {

class LoginAttemptObserver : public chromeos::LoginStatusConsumer {
 public:
  explicit LoginAttemptObserver(chromeos::ScreenLocker* locker);
  virtual ~LoginAttemptObserver();

  void WaitForAttempt();

  // Overridden from LoginStatusConsumer:
  virtual void OnLoginFailure(const chromeos::LoginFailure& error) {
    LoginAttempted();
  }

  virtual void OnLoginSuccess(
      const std::string& username,
      const std::string& password,
      const GaiaAuthConsumer::ClientLoginResult& credentials,
      bool pending_requests,
      bool using_oauth) {
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
    ui_test_utils::RunMessageLoop();
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

#if defined(TOOLKIT_USES_GTK)
class ScreenLockerViewsTester : public ScreenLockerTester {
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

  ScreenLockerViewsTester() {}

  // Returns the ScreenLockerViews object.
  ScreenLockerViews* screen_locker_views() const;

  views::Textfield* GetPasswordField() const;

  DISALLOW_COPY_AND_ASSIGN(ScreenLockerViewsTester);
};

void ScreenLockerViewsTester::SetPassword(const std::string& password) {
  DCHECK(ScreenLocker::screen_locker_);
  views::Textfield* pass = GetPasswordField();
  pass->SetText(ASCIIToUTF16(password.c_str()));
}

std::string ScreenLockerViewsTester::GetPassword() {
  DCHECK(ScreenLocker::screen_locker_);
  views::Textfield* pass = GetPasswordField();
  return UTF16ToUTF8(pass->text());
}

void ScreenLockerViewsTester::EnterPassword(const std::string& password) {
  SetPassword(password);
  LoginAttemptObserver login(ScreenLocker::screen_locker_);
  views::Textfield* pass = GetPasswordField();
  GdkEvent* event = gdk_event_new(GDK_KEY_PRESS);
  event->key.keyval = GDK_Return;
  views::KeyEvent key_event(event);
  screen_locker_views()->screen_lock_view_->HandleKeyEvent(pass, key_event);
  gdk_event_free(event);
  // Wait for login attempt.
  login.WaitForAttempt();
}

void ScreenLockerViewsTester::EmulateWindowManagerReady() {
  DCHECK(ScreenLocker::screen_locker_);
  screen_locker_views()->OnWindowManagerReady();
}

views::Textfield* ScreenLockerViewsTester::GetPasswordField() const {
  DCHECK(ScreenLocker::screen_locker_);
  return screen_locker_views()->screen_lock_view_->password_field_;
}

views::Widget* ScreenLockerViewsTester::GetWidget() const {
  DCHECK(ScreenLocker::screen_locker_);
  return screen_locker_views()->lock_window_;
}

views::Widget* ScreenLockerViewsTester::GetChildWidget() const {
  DCHECK(ScreenLocker::screen_locker_);
  return screen_locker_views()->lock_widget_;
}

ScreenLockerViews* ScreenLockerViewsTester::screen_locker_views() const {
  DCHECK(ScreenLocker::screen_locker_);
  DCHECK(!ScreenLocker::UseWebUILockScreen());
  return static_cast<ScreenLockerViews*>(
      ScreenLocker::screen_locker_->delegate_.get());
}
#endif  // TOOLKIT_USES_GTK

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

  base::Value* ExecuteJavascriptAndGetValue(const std::string& js_text);

  // Returns the ScreenLockerWebUI object.
  WebUIScreenLocker* webui_screen_locker() const;

  // Returns the WebUI object from the screen locker.
  WebUI* webui() const;

  DISALLOW_COPY_AND_ASSIGN(WebUIScreenLockerTester);
};

void WebUIScreenLockerTester::SetPassword(const std::string& password) {
  delete ExecuteJavascriptAndGetValue(StringPrintf(
      "$('pod-row').pods[0].passwordElement.value = '%s';"
      "$('pod-row').pods[0].passwordEmpty = false;",
      password.c_str()));
}

std::string WebUIScreenLockerTester::GetPassword() {
  std::string result;
  base::Value* v = ExecuteJavascriptAndGetValue(
      "$('pod-row').pods[0].passwordElement.value;");
  DCHECK(v->GetAsString(&result));
  delete v;
  return result;
}

void WebUIScreenLockerTester::EnterPassword(const std::string& password) {
  bool result;
  SetPassword(password);
  LoginAttemptObserver login(ScreenLocker::screen_locker_);
  base::Value* v = ExecuteJavascriptAndGetValue(
      "$('pod-row').pods[0].activate();");
  ASSERT_TRUE(v->GetAsBoolean(&result));
  ASSERT_TRUE(result);
  delete v;

  // Wait for login attempt.
  login.WaitForAttempt();
}

void WebUIScreenLockerTester::EmulateWindowManagerReady() {
#if !defined(USE_AURA)
  static_cast<LockWindowGtk*>(GetWidget()->native_widget())->
      OnWindowManagerReady();
#endif
}

views::Widget* WebUIScreenLockerTester::GetWidget() const {
  return webui_screen_locker()->lock_window_;
}

views::Widget* WebUIScreenLockerTester::GetChildWidget() const {
  return webui_screen_locker()->lock_window_;
}

base::Value* WebUIScreenLockerTester::ExecuteJavascriptAndGetValue(
    const std::string& js_text) {
  RenderViewHost* rvh = webui()->tab_contents()->GetRenderViewHost();
  return rvh->ExecuteJavascriptAndGetValue(string16(),
                                           ASCIIToUTF16(js_text));
}

WebUIScreenLocker* WebUIScreenLockerTester::webui_screen_locker() const {
  DCHECK(ScreenLocker::screen_locker_);
#if not defined(USE_AURA)
  DCHECK(ScreenLocker::UseWebUILockScreen());
#endif
  return static_cast<WebUIScreenLocker*>(
      ScreenLocker::screen_locker_->delegate_.get());
}

WebUI* WebUIScreenLockerTester::webui() const {
  DCHECK(webui_screen_locker()->webui_ready_);
  WebUI* webui = webui_screen_locker()->GetWebUI();
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
#if defined(TOOLKIT_USES_GTK)
  if (ScreenLocker::UseWebUILockScreen())
    return new test::WebUIScreenLockerTester();
  else
    return new test::ScreenLockerViewsTester();
#else
  return new test::WebUIScreenLockerTester();
#endif
}

}  // namespace chromeos
