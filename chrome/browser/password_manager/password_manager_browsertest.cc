// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/infobars/confirm_infobar_delegate.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/password_manager/test_password_store.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/keycodes/keyboard_codes.h"


// NavigationObserver ---------------------------------------------------------

namespace {

// Observer that waits for navigation to complete and for the password infobar
// to be shown.
class NavigationObserver : public content::NotificationObserver,
                           public content::WebContentsObserver {
 public:
  explicit NavigationObserver(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents),
        message_loop_runner_(new content::MessageLoopRunner),
        infobar_shown_(false),
        infobar_service_(InfoBarService::FromWebContents(web_contents)) {
    registrar_.Add(this,
                   chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_ADDED,
                   content::Source<InfoBarService>(infobar_service_));
  }

  virtual ~NavigationObserver() {}

  // Normally Wait() will not return until a main frame navigation occurs.
  // If a path is set, Wait() will return after this path has been seen,
  // regardless of the frame that navigated. Useful for multi-frame pages.
  void SetPathToWaitFor(const std::string& path) {
    wait_for_path_ = path;
  }

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    infobar_service_->infobar_at(0)->AsConfirmInfoBarDelegate()->Accept();
    infobar_shown_ = true;
  }

  // content::WebContentsObserver:
  virtual void DidFinishLoad(
      int64 frame_id,
      const GURL& validated_url,
      bool is_main_frame,
      content::RenderViewHost* render_view_host) OVERRIDE {
    if (!wait_for_path_.empty()) {
      if (validated_url.path() == wait_for_path_)
        message_loop_runner_->Quit();
    } else if (is_main_frame) {
      message_loop_runner_->Quit();
    }
  }

  bool infobar_shown() const { return infobar_shown_; }

  void Wait() {
    message_loop_runner_->Run();
  }

 private:
  std::string wait_for_path_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
  bool infobar_shown_;
  content::NotificationRegistrar registrar_;
  InfoBarService* infobar_service_;

  DISALLOW_COPY_AND_ASSIGN(NavigationObserver);
};

}  // namespace


// PasswordManagerBrowserTest -------------------------------------------------

class PasswordManagerBrowserTest : public InProcessBrowserTest {
 public:
  PasswordManagerBrowserTest() {}
  virtual ~PasswordManagerBrowserTest() {}

  // InProcessBrowserTest:
  virtual void SetUpOnMainThread() OVERRIDE {
    // Use TestPasswordStore to remove a possible race. Normally the
    // PasswordStore does its database manipulation on the DB thread, which
    // creates a possible race during navigation. Specifically the
    // PasswordManager will ignore any forms in a page if the load from the
    // PasswordStore has not completed.
    PasswordStoreFactory::GetInstance()->SetTestingFactory(
        browser()->profile(), &TestPasswordStore::Create);
  }

 protected:
  content::WebContents* WebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::RenderViewHost* RenderViewHost() {
    return WebContents()->GetRenderViewHost();
  }

  // Wrapper around ui_test_utils::NavigateToURL that waits until
  // DidFinishLoad() fires. Normally this function returns after
  // DidStopLoading(), which caused flakiness as the NavigationObserver
  // would sometimes see the DidFinishLoad event from a previous navigation and
  // return immediately.
  void NavigateToFile(const std::string& path) {
    ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

    NavigationObserver observer(WebContents());
    GURL url = embedded_test_server()->GetURL(path);
    ui_test_utils::NavigateToURL(browser(), url);
    observer.Wait();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordManagerBrowserTest);
};

// Actual tests ---------------------------------------------------------------
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       PromptForNormalSubmit) {
  NavigateToFile("/password/password_form.html");

  // Fill a form and submit through a <input type="submit"> button. Nothing
  // special.
  NavigationObserver observer(WebContents());
  std::string fill_and_submit =
      "document.getElementById('username_field').value = 'temp';"
      "document.getElementById('password_field').value = 'random';"
      "document.getElementById('input_submit_button').click()";
  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), fill_and_submit));
  observer.Wait();
  EXPECT_TRUE(observer.infobar_shown());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       PromptForSubmitUsingJavaScript) {
  NavigateToFile("/password/password_form.html");

  // Fill a form and submit using <button> that calls submit() on the form.
  // This should work regardless of the type of element, as long as submit() is
  // called.
  NavigationObserver observer(WebContents());
  std::string fill_and_submit =
      "document.getElementById('username_field').value = 'temp';"
      "document.getElementById('password_field').value = 'random';"
      "document.getElementById('submit_button').click()";
  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), fill_and_submit));
  observer.Wait();
  EXPECT_TRUE(observer.infobar_shown());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       PromptForDynamicForm) {
  NavigateToFile("/password/dynamic_password_form.html");

  // Fill the dynamic password form and submit.
  NavigationObserver observer(WebContents());
  std::string fill_and_submit =
      "document.getElementById('create_form_button').click();"
      "window.setTimeout(function() {"
      "  document.dynamic_form.username.value = 'tempro';"
      "  document.dynamic_form.password.value = 'random';"
      "  document.dynamic_form.submit();"
      "}, 0)";
  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), fill_and_submit));
  observer.Wait();
  EXPECT_TRUE(observer.infobar_shown());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest, NoPromptForNavigation) {
  NavigateToFile("/password/password_form.html");

  // Don't fill the password form, just navigate away. Shouldn't prompt.
  NavigationObserver observer(WebContents());
  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(),
                                     "window.location.href = 'done.html';"));
  observer.Wait();
  EXPECT_FALSE(observer.infobar_shown());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       NoPromptForSubFrameNavigation) {
  NavigateToFile("/password/multi_frames.html");

  // If you are filling out a password form in one frame and a different frame
  // navigates, this should not trigger the infobar.
  NavigationObserver observer(WebContents());
  observer.SetPathToWaitFor("/password/done.html");
  std::string fill =
      "var first_frame = document.getElementById('first_frame');"
      "var frame_doc = first_frame.contentDocument;"
      "frame_doc.getElementById('username_field').value = 'temp';"
      "frame_doc.getElementById('password_field').value = 'random';";
  std::string navigate_frame =
      "var second_iframe = document.getElementById('second_frame');"
      "second_iframe.contentWindow.location.href = 'done.html';";

  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), fill));
  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), navigate_frame));
  observer.Wait();
  EXPECT_FALSE(observer.infobar_shown());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       PromptAfterSubmitWithSubFrameNavigation) {
  NavigateToFile("/password/multi_frames.html");

  // Make sure that we prompt to save password even if a sub-frame navigation
  // happens first.
  NavigationObserver observer(WebContents());
  observer.SetPathToWaitFor("/password/done.html");
  std::string navigate_frame =
      "var second_iframe = document.getElementById('second_frame');"
      "second_iframe.contentWindow.location.href = 'other.html';";
  std::string fill_and_submit =
      "var first_frame = document.getElementById('first_frame');"
      "var frame_doc = first_frame.contentDocument;"
      "frame_doc.getElementById('username_field').value = 'temp';"
      "frame_doc.getElementById('password_field').value = 'random';"
      "frame_doc.getElementById('input_submit_button').click();";

  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), navigate_frame));
  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), fill_and_submit));
  observer.Wait();
  EXPECT_TRUE(observer.infobar_shown());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       PromptForXHRSubmit) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshBrowserTests))
    return;
#endif
  NavigateToFile("/password/password_xhr_submit.html");

  // Verify that we show the save password prompt if a form returns false
  // in its onsubmit handler but instead logs in/navigates via XHR.
  // Note that calling 'submit()' on a form with javascript doesn't call
  // the onsubmit handler, so we click the submit button instead.
  NavigationObserver observer(WebContents());
  std::string fill_and_submit =
      "document.getElementById('username_field').value = 'temp';"
      "document.getElementById('password_field').value = 'random';"
      "document.getElementById('submit_button').click()";
  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), fill_and_submit));
  observer.Wait();
  EXPECT_TRUE(observer.infobar_shown());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest, NoPromptForOtherXHR) {
  NavigateToFile("/password/password_xhr_submit.html");

  // Verify that if random XHR navigation occurs, we don't try and save the
  // password.
  //
  // We may want to change this functionality in the future to account for
  // cases where the element that users click on isn't a submit button.
  NavigationObserver observer(WebContents());
  std::string fill_and_navigate =
      "document.getElementById('username_field').value = 'temp';"
      "document.getElementById('password_field').value = 'random';"
      "send_xhr()";
  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), fill_and_navigate));
  observer.Wait();
  EXPECT_FALSE(observer.infobar_shown());
}
