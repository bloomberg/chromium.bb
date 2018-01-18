// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/login_ui_test_utils.h"

#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/signin/signin_tracker_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/signin_view_controller_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
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

// Synchronously waits for the Sync confirmation to be closed.
class SyncConfirmationClosedObserver : public LoginUIService::Observer {
 public:
  void WaitForConfirmationClosed() {
    if (sync_confirmation_closed_)
      return;

    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 private:
  void OnSyncConfirmationUIClosed(
      LoginUIService::SyncConfirmationUIClosedResult result) override {
    sync_confirmation_closed_ = true;
    if (quit_closure_)
      std::move(quit_closure_).Run();
  }

  bool sync_confirmation_closed_ = false;
  base::OnceClosure quit_closure_;
};

void RunLoopFor(base::TimeDelta duration) {
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), duration);
  run_loop.Run();
}

}  // anonymous namespace


namespace login_ui_test_utils {
class SigninViewControllerTestUtil {
 public:
  static bool TryDismissSyncConfirmationDialog(Browser* browser) {
#if defined(OS_CHROMEOS)
    NOTREACHED();
    return false;
#else
    SigninViewController* signin_view_controller =
        browser->signin_view_controller();
    DCHECK_NE(signin_view_controller, nullptr);
    if (!signin_view_controller->ShowsModalDialog())
      return false;
    content::WebContents* dialog_web_contents =
        signin_view_controller->GetModalDialogWebContentsForTesting();
    DCHECK_NE(dialog_web_contents, nullptr);
    std::string message;
    std::string find_button_js =
        "if (document.readyState != 'complete') {"
        "  window.domAutomationController.send('DocumentNotReady');"
        "} else if (document.getElementById('confirmButton') == null) {"
        "  window.domAutomationController.send('NotFound');"
        "} else {"
        "  window.domAutomationController.send('Ok');"
        "}";
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        dialog_web_contents, find_button_js, &message));
    if (message != "Ok")
      return false;

    // This cannot be a synchronous call, because it closes the window as a side
    // effect, which may cause the javascript execution to never finish.
    content::ExecuteScriptAsync(
        dialog_web_contents,
        "document.getElementById('confirmButton').click();");
    return true;
#endif
  }
};

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

void WaitUntilElementExistsInSigninFrame(
    Browser* browser,
    const std::vector<std::string>& element_ids) {
  for (int attempt = 0; attempt < 10; ++attempt) {
    for (const std::string& element_id : element_ids) {
      if (ElementExistsInSigninFrame(browser, element_id)) {
        return;
      }
    }
    RunLoopFor(base::TimeDelta::FromMilliseconds(1000));
  }

  FAIL();
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
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();

  WaitUntilElementExistsInSigninFrame(browser, {"identifierId"});
  std::string js = "document.getElementById('identifierId').value = '" + email +
                   "'; document.getElementById('identifierNext').click();";
  ASSERT_TRUE(content::ExecuteScript(
      signin::GetAuthFrame(web_contents, "signin-frame"), js));

  WaitUntilElementExistsInSigninFrame(browser, {"password"});
  js = "document.getElementById('password').value = '" + password + "';" +
       "document.getElementById('passwordNext').click();";
  ASSERT_TRUE(content::ExecuteScript(
      signin::GetAuthFrame(web_contents, "signin-frame"), js));
}

void SigninInOldGaiaFlow(Browser* browser,
                         const std::string& email,
                         const std::string& password) {
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();

  WaitUntilElementExistsInSigninFrame(browser, {"Email"});
  std::string js = "document.getElementById('Email').value = '" + email + ";" +
                   "document.getElementById('next').click();";
  ASSERT_TRUE(content::ExecuteScript(
      signin::GetAuthFrame(web_contents, "signin-frame"), js));

  WaitUntilElementExistsInSigninFrame(browser, {"Passwd"});
  js = "document.getElementById('Passwd').value = '" + password + "';" +
       "document.getElementById('signIn').click();";
  ASSERT_TRUE(content::ExecuteScript(
      signin::GetAuthFrame(web_contents, "signin-frame"), js));
}

void ExecuteJsToSigninInSigninFrame(Browser* browser,
                                    const std::string& email,
                                    const std::string& password) {
  WaitUntilElementExistsInSigninFrame(browser, {"identifierNext", "next"});
  if (ElementExistsInSigninFrame(browser, "identifierNext"))
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
  std::unique_ptr<SigninTracker> tracker =
      SigninTrackerFactory::CreateForProfile(browser->profile(),
                                             &signin_observer);

  GURL signin_url =
      signin::GetPromoURLForTab(access_point, signin_reason, false);
  DVLOG(1) << "Navigating to " << signin_url;
  // For some tests, the window is not shown yet and this might be the first tab
  // navigation, so GetActiveWebContents() for CURRENT_TAB is NULL. That's why
  // we use NEW_FOREGROUND_TAB rather than the CURRENT_TAB used by default in
  // ui_test_utils::NavigateToURL().
  ui_test_utils::NavigateToURLWithDisposition(
      browser, signin_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
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

bool DismissSyncConfirmationDialog(Browser* browser, base::TimeDelta timeout) {
  SyncConfirmationClosedObserver confirmation_closed_observer;
  ScopedObserver<LoginUIService, LoginUIService::Observer>
      scoped_confirmation_closed_observer(&confirmation_closed_observer);
  scoped_confirmation_closed_observer.Add(
      LoginUIServiceFactory::GetForProfile(browser->profile()));

  const base::Time expire_time = base::Time::Now() + timeout;
  while (base::Time::Now() <= expire_time) {
    if (SigninViewControllerTestUtil::TryDismissSyncConfirmationDialog(
            browser)) {
      confirmation_closed_observer.WaitForConfirmationClosed();
      return true;
    }
    RunLoopFor(base::TimeDelta::FromMilliseconds(1000));
  }
  return false;
}

}  // namespace login_ui_test_utils
