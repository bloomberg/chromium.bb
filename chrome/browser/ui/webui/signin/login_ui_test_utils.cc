// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/signin/signin_tracker_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/signin/get_auth_frame.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"

using content::MessageLoopRunner;

// anonymous namespace for signin with UI helper functions.
namespace {

// The SignInObserver observes the signin manager and blocks until a
// GoogleSigninSucceeded or a GoogleSigninFailed notification is fired.
class SignInObserver : public SigninTracker::Observer {
 public:
  explicit SignInObserver(bool wait_for_account_cookies)
      : seen_(false),
        running_(false),
        signed_in_(false),
        wait_for_account_cookies_(wait_for_account_cookies) {}

  virtual ~SignInObserver() {}

  // Returns whether a GoogleSigninSucceeded event has happened.
  bool DidSignIn() {
    return signed_in_;
  }

  // Blocks and waits until the user signs in. Wait() does not block if a
  // GoogleSigninSucceeded or a GoogleSigninFailed has already occurred.
  void Wait() {
    if (seen_)
      return;

    running_ = true;
    message_loop_runner_ = new MessageLoopRunner;
    message_loop_runner_->Run();
    EXPECT_TRUE(seen_);
  }

  void SigninFailed(const GoogleServiceAuthError& error) override {
    DVLOG(1) << "Google signin failed.";
    QuitLoopRunner();
  }

  void AccountAddedToCookie(const GoogleServiceAuthError& error) override {
    if (!wait_for_account_cookies_)
      return;
    if (error.state() != GoogleServiceAuthError::NONE) {
      DVLOG(1) << "Error signing the account, error " << error.state();
    } else {
      DVLOG(1) << "Account cookies are added to cookie jar.";
      signed_in_ = true;
    }
    QuitLoopRunner();
  }

  void SigninSuccess() override {
    DVLOG(1) << "Google signin succeeded.";
    if (wait_for_account_cookies_)
      return;
    signed_in_ = true;
    QuitLoopRunner();
  }

  void QuitLoopRunner() {
    seen_ = true;
    if (!running_)
      return;
    message_loop_runner_->Quit();
    running_ = false;
  }

 private:
  // Bool to mark an observed event as seen prior to calling Wait(), used to
  // prevent the observer from blocking.
  bool seen_;
  // True is the message loop runner is running.
  bool running_;
  // True if a GoogleSigninSucceeded event has been observed.
  bool signed_in_;
  // Whether we should block until the account cookies are added or not.
  // If false, we only wait until SigninSuccess event is fired which happens
  // prior to adding account to cookie.
  bool wait_for_account_cookies_;
  scoped_refptr<MessageLoopRunner> message_loop_runner_;
};

}  // anonymous namespace


namespace login_ui_test_utils {

void WaitUntilUIReady(Browser* browser) {
  std::string message;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      browser->tab_strip_model()->GetActiveWebContents(),
      "if (!inline.login.getAuthExtHost())"
      "  inline.login.initialize();"
      "var handler = function() {"
      "  window.domAutomationController.send('ready');"
      "};"
      "if (inline.login.isAuthReady())"
      "  handler();"
      "else"
      "  inline.login.getAuthExtHost().addEventListener('ready', handler);",
      &message));
  ASSERT_EQ("ready", message);
}

void WaitUntilElementExistsInSigninFrame(Browser* browser,
                                         const std::string& element_id) {
  std::string message;
  std::string js =
      "function WaitForElementById(elementId) {"
      "  var retries = 10; /* 10 seconds. */"
      "  function CheckelementExists() {"
      "    if (document.getElementById(elementId) != null) {"
      "      window.domAutomationController.send('found');"
      "    } else if (retries > 0) { "
      "      retries--;"
      "      window.setTimeout(CheckelementExists, 1000);"
      "    } else {"
      "      window.domAutomationController.send('failed');"
      "    }"
      "  }"
      "  CheckelementExists();"
      "}"
      "WaitForElementById('" + element_id + "');";
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      signin::GetAuthFrame(web_contents, "signin-frame"), js, &message));

  ASSERT_EQ("found", message) <<
      "Failed to find element with id " << element_id;
}

bool ElementExistsInSigninFrame(Browser* browser,
                                const std::string& element_id) {
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  bool result = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      signin::GetAuthFrame(web_contents, "signin-frame"),
      "window.domAutomationController.send("
      "  document.getElementById('" +
          element_id + "') != null);",
      &result));
  return result;
}

void SigninInNewGaiaFlow(Browser* browser,
                         const std::string& email,
                         const std::string& password) {
  std::string js = "document.getElementById('Email').value = '" + email + "';"
                   "document.getElementById('next').click();";

  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::ExecuteScript(
      signin::GetAuthFrame(web_contents, "signin-frame"), js));

  WaitUntilElementExistsInSigninFrame(browser, "Passwd");
  js = "document.getElementById('Passwd').value = '" + password + "';"
       "document.getElementById('signIn').click();";

  ASSERT_TRUE(content::ExecuteScript(
      signin::GetAuthFrame(web_contents, "signin-frame"), js));
}

void SigninInOldGaiaFlow(Browser* browser,
                         const std::string& email,
                         const std::string& password) {
  std::string js =
      "document.getElementById('Email').value = '" + email + "';"
      "document.getElementById('Passwd').value = '" + password + "';"
      "document.getElementById('signIn').click();";

  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::ExecuteScript(
      signin::GetAuthFrame(web_contents, "signin-frame"), js));
}

void ExecuteJsToSigninInSigninFrame(Browser* browser,
                                    const std::string& email,
                                    const std::string& password) {
  WaitUntilElementExistsInSigninFrame(browser, "Email");
  if (ElementExistsInSigninFrame(browser, "next"))
    SigninInNewGaiaFlow(browser, email, password);
  else
    SigninInOldGaiaFlow(browser, email, password);
}

bool SignInWithUI(Browser* browser,
                  const std::string& username,
                  const std::string& password,
                  bool wait_for_account_cookies,
                  signin_metrics::AccessPoint access_point,
                  signin_metrics::Reason signin_reason) {
  SignInObserver signin_observer(wait_for_account_cookies);
  scoped_ptr<SigninTracker> tracker =
      SigninTrackerFactory::CreateForProfile(browser->profile(),
                                             &signin_observer);

  GURL signin_url = signin::GetPromoURL(access_point, signin_reason, false);
  DVLOG(1) << "Navigating to " << signin_url;
  // For some tests, the window is not shown yet and this might be the first tab
  // navigation, so GetActiveWebContents() for CURRENT_TAB is NULL. That's why
  // we use NEW_FOREGROUND_TAB rather than the CURRENT_TAB used by default in
  // ui_test_utils::NavigateToURL().
  ui_test_utils::NavigateToURLWithDisposition(
        browser,
        signin_url,
        NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  DVLOG(1) << "Wait for login UI to be ready.";
  WaitUntilUIReady(browser);
  DVLOG(1) << "Sign in user: " << username;
  ExecuteJsToSigninInSigninFrame(browser, username, password);
  signin_observer.Wait();
  return signin_observer.DidSignIn();
}

bool SignInWithUI(Browser* browser,
                  const std::string& username,
                  const std::string& password) {
  return SignInWithUI(browser, username, password,
                      false /* wait_for_account_cookies */,
                      signin_metrics::AccessPoint::ACCESS_POINT_START_PAGE,
                      signin_metrics::Reason::REASON_SIGNIN_PRIMARY_ACCOUNT);
}

}  // namespace login_ui_test_utils
