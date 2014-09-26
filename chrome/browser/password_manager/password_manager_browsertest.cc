// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/password_manager/test_password_store_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/login/login_prompt.h"
#include "chrome/browser/ui/login/login_prompt_test_utils.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/common/password_manager_switches.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/base/filename_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/point.h"


// NavigationObserver ---------------------------------------------------------

namespace {

// Observer that waits for navigation to complete and for the password infobar
// to be shown.
class NavigationObserver : public content::WebContentsObserver {
 public:
  explicit NavigationObserver(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents),
        message_loop_runner_(new content::MessageLoopRunner) {}

  virtual ~NavigationObserver() {}

  // Normally Wait() will not return until a main frame navigation occurs.
  // If a path is set, Wait() will return after this path has been seen,
  // regardless of the frame that navigated. Useful for multi-frame pages.
  void SetPathToWaitFor(const std::string& path) {
    wait_for_path_ = path;
  }

  // content::WebContentsObserver:
  virtual void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                             const GURL& validated_url) OVERRIDE {
    if (!wait_for_path_.empty()) {
      if (validated_url.path() == wait_for_path_)
        message_loop_runner_->Quit();
    } else if (!render_frame_host->GetParent()) {
      message_loop_runner_->Quit();
    }
  }

  void Wait() { message_loop_runner_->Run(); }

 private:
  std::string wait_for_path_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(NavigationObserver);
};

// Observes the save password prompt (bubble or infobar) for a specified
// WebContents, keeps track of whether or not it is currently shown, and allows
// accepting saving passwords through it.
class PromptObserver {
 public:
  virtual ~PromptObserver() {}

  // Checks if the prompt is being currently shown.
  virtual bool IsShowingPrompt() const = 0;

  // Expecting that the prompt is shown, saves the password. Checks that the
  // prompt is no longer visible afterwards.
  void Accept() const {
    EXPECT_TRUE(IsShowingPrompt());
    AcceptImpl();
  }

  // Chooses the right implementation of PromptObserver and creates an instance
  // of it.
  static scoped_ptr<PromptObserver> Create(content::WebContents* web_contents);

 protected:
  PromptObserver() {}

  // Accepts the password. The implementation can assume that the prompt is
  // currently shown, but is required to verify that the prompt is eventually
  // closed.
  virtual void AcceptImpl() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(PromptObserver);
};

class InfoBarObserver : public PromptObserver,
                        public infobars::InfoBarManager::Observer {
 public:
  explicit InfoBarObserver(content::WebContents* web_contents)
      : infobar_is_being_shown_(false),
        infobar_service_(InfoBarService::FromWebContents(web_contents)) {
    infobar_service_->AddObserver(this);
  }

  virtual ~InfoBarObserver() {
    if (infobar_service_)
      infobar_service_->RemoveObserver(this);
  }

 private:
  // PromptObserver:
  virtual bool IsShowingPrompt() const OVERRIDE {
    return infobar_is_being_shown_;
  }

  virtual void AcceptImpl() const OVERRIDE {
    EXPECT_EQ(1u, infobar_service_->infobar_count());
    if (!infobar_service_->infobar_count())
      return;  // Let the test finish to gather possibly more diagnostics.

    // ConfirmInfoBarDelegate::Accept returning true means the infobar is
    // immediately closed. Checking the return value is preferred to testing
    // IsShowingPrompt() here, for it avoids the delay until the closing
    // notification is received.
    EXPECT_TRUE(infobar_service_->infobar_at(0)
                    ->delegate()
                    ->AsConfirmInfoBarDelegate()
                    ->Accept());
  }

  // infobars::InfoBarManager::Observer:
  virtual void OnInfoBarAdded(infobars::InfoBar* infobar) OVERRIDE {
    infobar_is_being_shown_ = true;
  }

  virtual void OnInfoBarRemoved(infobars::InfoBar* infobar,
                                bool animate) OVERRIDE {
    infobar_is_being_shown_ = false;
  }

  virtual void OnManagerShuttingDown(
      infobars::InfoBarManager* manager) OVERRIDE {
    ASSERT_EQ(infobar_service_, manager);
    infobar_service_->RemoveObserver(this);
    infobar_service_ = NULL;
  }

  bool infobar_is_being_shown_;
  InfoBarService* infobar_service_;

  DISALLOW_COPY_AND_ASSIGN(InfoBarObserver);
};

class BubbleObserver : public PromptObserver {
 public:
  explicit BubbleObserver(content::WebContents* web_contents)
      : ui_controller_(
            ManagePasswordsUIController::FromWebContents(web_contents)) {}

  virtual ~BubbleObserver() {}

 private:
  // PromptObserver:
  virtual bool IsShowingPrompt() const OVERRIDE {
    return ui_controller_->PasswordPendingUserDecision();
  }

  virtual void AcceptImpl() const OVERRIDE {
    ui_controller_->SavePassword();
    EXPECT_FALSE(IsShowingPrompt());
  }

  ManagePasswordsUIController* const ui_controller_;

  DISALLOW_COPY_AND_ASSIGN(BubbleObserver);
};

GURL GetFileURL(const char* filename) {
  base::FilePath path;
  PathService::Get(chrome::DIR_TEST_DATA, &path);
  path = path.AppendASCII("password").AppendASCII(filename);
  CHECK(base::PathExists(path));
  return net::FilePathToFileURL(path);
}

// static
scoped_ptr<PromptObserver> PromptObserver::Create(
    content::WebContents* web_contents) {
  if (ChromePasswordManagerClient::IsTheHotNewBubbleUIEnabled()) {
    return scoped_ptr<PromptObserver>(new BubbleObserver(web_contents));
  } else {
    return scoped_ptr<PromptObserver>(new InfoBarObserver(web_contents));
  }
}

// Handles |request| to "/basic_auth". If "Authorization" header is present,
// responds with a non-empty HTTP 200 page (regardless of its value). Otherwise
// serves a Basic Auth challenge.
scoped_ptr<net::test_server::HttpResponse> HandleTestAuthRequest(
    const net::test_server::HttpRequest& request) {
  if (!StartsWithASCII(request.relative_url, "/basic_auth", true))
    return scoped_ptr<net::test_server::HttpResponse>();

  if (ContainsKey(request.headers, "Authorization")) {
    scoped_ptr<net::test_server::BasicHttpResponse> http_response(
        new net::test_server::BasicHttpResponse);
    http_response->set_code(net::HTTP_OK);
    http_response->set_content("Success!");
    return http_response.PassAs<net::test_server::HttpResponse>();
  } else {
    scoped_ptr<net::test_server::BasicHttpResponse> http_response(
        new net::test_server::BasicHttpResponse);
    http_response->set_code(net::HTTP_UNAUTHORIZED);
    http_response->AddCustomHeader("WWW-Authenticate",
                                   "Basic realm=\"test realm\"");
    return http_response.PassAs<net::test_server::HttpResponse>();
  }
}

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
        browser()->profile(), TestPasswordStoreService::Build);
    ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
    ASSERT_FALSE(CommandLine::ForCurrentProcess()->HasSwitch(
        password_manager::switches::kEnableAutomaticPasswordSaving));
  }

  virtual void TearDownOnMainThread() OVERRIDE {
    ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
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
    NavigationObserver observer(WebContents());
    GURL url = embedded_test_server()->GetURL(path);
    ui_test_utils::NavigateToURL(browser(), url);
    observer.Wait();
  }

  // Waits until the "value" attribute of the HTML element with |element_id| is
  // equal to |expected_value|. If the current value is not as expected, this
  // waits until the "change" event is fired for the element. This also
  // guarantees that once the real value matches the expected, the JavaScript
  // event loop is spun to allow all other possible events to take place.
  void WaitForElementValue(const std::string& element_id,
                           const std::string& expected_value);
  // Checks that the current "value" attribute of the HTML element with
  // |element_id| is equal to |expected_value|.
  void CheckElementValue(const std::string& element_id,
                         const std::string& expected_value);

 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordManagerBrowserTest);
};

void PasswordManagerBrowserTest::WaitForElementValue(
    const std::string& element_id,
    const std::string& expected_value) {
  enum ReturnCodes {  // Possible results of the JavaScript code.
    RETURN_CODE_OK,
    RETURN_CODE_NO_ELEMENT,
    RETURN_CODE_WRONG_VALUE,
    RETURN_CODE_INVALID,
  };
  const std::string value_check_function = base::StringPrintf(
      "function valueCheck() {"
      "  var element = document.getElementById('%s');"
      "  return element && element.value == '%s';"
      "}",
      element_id.c_str(),
      expected_value.c_str());
  const std::string script =
      value_check_function +
      base::StringPrintf(
          "if (valueCheck()) {"
          "  /* Spin the event loop with setTimeout. */"
          "  setTimeout(window.domAutomationController.send(%d), 0);"
          "} else {"
          "  var element = document.getElementById('%s');"
          "  if (!element)"
          "    window.domAutomationController.send(%d);"
          "  element.onchange = function() {"
          "    if (valueCheck()) {"
          "      /* Spin the event loop with setTimeout. */"
          "      setTimeout(window.domAutomationController.send(%d), 0);"
          "    } else {"
          "      window.domAutomationController.send(%d);"
          "    }"
          "  };"
          "}",
          RETURN_CODE_OK,
          element_id.c_str(),
          RETURN_CODE_NO_ELEMENT,
          RETURN_CODE_OK,
          RETURN_CODE_WRONG_VALUE);
  int return_value = RETURN_CODE_INVALID;
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
      RenderViewHost(), script, &return_value));
  EXPECT_EQ(RETURN_CODE_OK, return_value)
      << "element_id = " << element_id
      << ", expected_value = " << expected_value;
}

void PasswordManagerBrowserTest::CheckElementValue(
    const std::string& element_id,
    const std::string& expected_value) {
  const std::string value_check_script = base::StringPrintf(
      "var element = document.getElementById('%s');"
      "window.domAutomationController.send(element && element.value == '%s');",
      element_id.c_str(),
      expected_value.c_str());
  bool return_value = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      RenderViewHost(), value_check_script, &return_value));
  EXPECT_TRUE(return_value) << "element_id = " << element_id
                            << ", expected_value = " << expected_value;
}

// Actual tests ---------------------------------------------------------------
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       PromptForNormalSubmit) {
  NavigateToFile("/password/password_form.html");

  // Fill a form and submit through a <input type="submit"> button. Nothing
  // special.
  NavigationObserver observer(WebContents());
  scoped_ptr<PromptObserver> prompt_observer(
      PromptObserver::Create(WebContents()));
  std::string fill_and_submit =
      "document.getElementById('username_field').value = 'temp';"
      "document.getElementById('password_field').value = 'random';"
      "document.getElementById('input_submit_button').click()";
  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), fill_and_submit));
  observer.Wait();
  EXPECT_TRUE(prompt_observer->IsShowingPrompt());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       PromptForSubmitWithInPageNavigation) {
  NavigateToFile("/password/password_navigate_before_submit.html");

  // Fill a form and submit through a <input type="submit"> button. Nothing
  // special. The form does an in-page navigation before submitting.
  NavigationObserver observer(WebContents());
  scoped_ptr<PromptObserver> prompt_observer(
      PromptObserver::Create(WebContents()));
  std::string fill_and_submit =
      "document.getElementById('username_field').value = 'temp';"
      "document.getElementById('password_field').value = 'random';"
      "document.getElementById('input_submit_button').click()";
  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), fill_and_submit));
  observer.Wait();
  EXPECT_TRUE(prompt_observer->IsShowingPrompt());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       LoginSuccessWithUnrelatedForm) {
  // Log in, see a form on the landing page. That form is not related to the
  // login form (=has a different action), so we should offer saving the
  // password.
  NavigateToFile("/password/password_form.html");

  NavigationObserver observer(WebContents());
  scoped_ptr<PromptObserver> prompt_observer(
      PromptObserver::Create(WebContents()));
  std::string fill_and_submit =
      "document.getElementById('username_unrelated').value = 'temp';"
      "document.getElementById('password_unrelated').value = 'random';"
      "document.getElementById('submit_unrelated').click()";
  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), fill_and_submit));
  observer.Wait();
  EXPECT_TRUE(prompt_observer->IsShowingPrompt());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest, LoginFailed) {
  // Log in, see a form on the landing page. That form is not related to the
  // login form (=has a different action), so we should offer saving the
  // password.
  NavigateToFile("/password/password_form.html");

  NavigationObserver observer(WebContents());
  scoped_ptr<PromptObserver> prompt_observer(
      PromptObserver::Create(WebContents()));
  std::string fill_and_submit =
      "document.getElementById('username_failed').value = 'temp';"
      "document.getElementById('password_failed').value = 'random';"
      "document.getElementById('submit_failed').click()";
  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), fill_and_submit));
  observer.Wait();
  EXPECT_FALSE(prompt_observer->IsShowingPrompt());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest, Redirects) {
  NavigateToFile("/password/password_form.html");

  // Fill a form and submit through a <input type="submit"> button. The form
  // points to a redirection page.
  NavigationObserver observer(WebContents());
  scoped_ptr<PromptObserver> prompt_observer(
      PromptObserver::Create(WebContents()));
  std::string fill_and_submit =
      "document.getElementById('username_redirect').value = 'temp';"
      "document.getElementById('password_redirect').value = 'random';"
      "document.getElementById('submit_redirect').click()";
  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), fill_and_submit));
  observer.Wait();
  EXPECT_TRUE(prompt_observer->IsShowingPrompt());

  // The redirection page now redirects via Javascript. We check that the
  // infobar stays.
  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(),
                                     "window.location.href = 'done.html';"));
  observer.Wait();
  EXPECT_TRUE(prompt_observer->IsShowingPrompt());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       PromptForSubmitUsingJavaScript) {
  NavigateToFile("/password/password_form.html");

  // Fill a form and submit using <button> that calls submit() on the form.
  // This should work regardless of the type of element, as long as submit() is
  // called.
  NavigationObserver observer(WebContents());
  scoped_ptr<PromptObserver> prompt_observer(
      PromptObserver::Create(WebContents()));
  std::string fill_and_submit =
      "document.getElementById('username_field').value = 'temp';"
      "document.getElementById('password_field').value = 'random';"
      "document.getElementById('submit_button').click()";
  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), fill_and_submit));
  observer.Wait();
  EXPECT_TRUE(prompt_observer->IsShowingPrompt());
}

// Flaky: crbug.com/301547, observed on win and mac. Probably happens on all
// platforms.
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       DISABLED_PromptForDynamicForm) {
  NavigateToFile("/password/dynamic_password_form.html");

  // Fill the dynamic password form and submit.
  NavigationObserver observer(WebContents());
  scoped_ptr<PromptObserver> prompt_observer(
      PromptObserver::Create(WebContents()));
  std::string fill_and_submit =
      "document.getElementById('create_form_button').click();"
      "window.setTimeout(function() {"
      "  document.dynamic_form.username.value = 'tempro';"
      "  document.dynamic_form.password.value = 'random';"
      "  document.dynamic_form.submit();"
      "}, 0)";
  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), fill_and_submit));
  observer.Wait();
  EXPECT_TRUE(prompt_observer->IsShowingPrompt());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest, NoPromptForNavigation) {
  NavigateToFile("/password/password_form.html");

  // Don't fill the password form, just navigate away. Shouldn't prompt.
  NavigationObserver observer(WebContents());
  scoped_ptr<PromptObserver> prompt_observer(
      PromptObserver::Create(WebContents()));
  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(),
                                     "window.location.href = 'done.html';"));
  observer.Wait();
  EXPECT_FALSE(prompt_observer->IsShowingPrompt());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       NoPromptForSubFrameNavigation) {
  NavigateToFile("/password/multi_frames.html");

  // If you are filling out a password form in one frame and a different frame
  // navigates, this should not trigger the infobar.
  NavigationObserver observer(WebContents());
  scoped_ptr<PromptObserver> prompt_observer(
      PromptObserver::Create(WebContents()));
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
  EXPECT_FALSE(prompt_observer->IsShowingPrompt());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       PromptAfterSubmitWithSubFrameNavigation) {
  NavigateToFile("/password/multi_frames.html");

  // Make sure that we prompt to save password even if a sub-frame navigation
  // happens first.
  NavigationObserver observer(WebContents());
  scoped_ptr<PromptObserver> prompt_observer(
      PromptObserver::Create(WebContents()));
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
  EXPECT_TRUE(prompt_observer->IsShowingPrompt());
}

IN_PROC_BROWSER_TEST_F(
    PasswordManagerBrowserTest,
    NoPromptForFailedLoginFromMainFrameWithMultiFramesInPage) {
  NavigateToFile("/password/multi_frames.html");

  // Make sure that we don't prompt to save the password for a failed login
  // from the main frame with multiple frames in the same page.
  NavigationObserver observer(WebContents());
  scoped_ptr<PromptObserver> prompt_observer(
      PromptObserver::Create(WebContents()));
  std::string fill_and_submit =
      "document.getElementById('username_failed').value = 'temp';"
      "document.getElementById('password_failed').value = 'random';"
      "document.getElementById('submit_failed').click();";

  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), fill_and_submit));
  observer.Wait();
  EXPECT_FALSE(prompt_observer->IsShowingPrompt());
}

IN_PROC_BROWSER_TEST_F(
    PasswordManagerBrowserTest,
    NoPromptForFailedLoginFromSubFrameWithMultiFramesInPage) {
  NavigateToFile("/password/multi_frames.html");

  // Make sure that we don't prompt to save the password for a failed login
  // from a sub-frame with multiple frames in the same page.
  NavigationObserver observer(WebContents());
  scoped_ptr<PromptObserver> prompt_observer(
      PromptObserver::Create(WebContents()));
  std::string fill_and_submit =
      "var first_frame = document.getElementById('first_frame');"
      "var frame_doc = first_frame.contentDocument;"
      "frame_doc.getElementById('username_failed').value = 'temp';"
      "frame_doc.getElementById('password_failed').value = 'random';"
      "frame_doc.getElementById('submit_failed').click();"
      "window.parent.location.href = 'multi_frames.html';";

  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), fill_and_submit));
  observer.Wait();
  EXPECT_FALSE(prompt_observer->IsShowingPrompt());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest, PromptForXHRSubmit) {
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
  scoped_ptr<PromptObserver> prompt_observer(
      PromptObserver::Create(WebContents()));
  std::string fill_and_submit =
      "document.getElementById('username_field').value = 'temp';"
      "document.getElementById('password_field').value = 'random';"
      "document.getElementById('submit_button').click()";
  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), fill_and_submit));
  observer.Wait();
  EXPECT_TRUE(prompt_observer->IsShowingPrompt());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       PromptForXHRWithoutOnSubmit) {
  NavigateToFile("/password/password_xhr_submit.html");

  // Verify that if XHR navigation occurs and the form is properly filled out,
  // we try and save the password even though onsubmit hasn't been called.
  NavigationObserver observer(WebContents());
  scoped_ptr<PromptObserver> prompt_observer(
      PromptObserver::Create(WebContents()));
  std::string fill_and_navigate =
      "document.getElementById('username_field').value = 'temp';"
      "document.getElementById('password_field').value = 'random';"
      "send_xhr()";
  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), fill_and_navigate));
  observer.Wait();
  EXPECT_TRUE(prompt_observer->IsShowingPrompt());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest, NoPromptIfLinkClicked) {
  NavigateToFile("/password/password_form.html");

  // Verify that if the user takes a direct action to leave the page, we don't
  // prompt to save the password even if the form is already filled out.
  NavigationObserver observer(WebContents());
  scoped_ptr<PromptObserver> prompt_observer(
      PromptObserver::Create(WebContents()));
  std::string fill_and_click_link =
      "document.getElementById('username_field').value = 'temp';"
      "document.getElementById('password_field').value = 'random';"
      "document.getElementById('link').click();";
  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), fill_and_click_link));
  observer.Wait();
  EXPECT_FALSE(prompt_observer->IsShowingPrompt());
}

// TODO(jam): http://crbug.com/350550
#if !defined(OS_WIN)
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       VerifyPasswordGenerationUpload) {
  // Prevent Autofill requests from actually going over the wire.
  net::TestURLFetcherFactory factory;
  // Disable Autofill requesting access to AddressBook data. This causes
  // the test to hang on Mac.
  autofill::test::DisableSystemServices(browser()->profile()->GetPrefs());

  // Visit a signup form.
  NavigateToFile("/password/signup_form.html");

  // Enter a password and save it.
  NavigationObserver first_observer(WebContents());
  scoped_ptr<PromptObserver> prompt_observer(
      PromptObserver::Create(WebContents()));
  std::string fill_and_submit =
      "document.getElementById('other_info').value = 'stuff';"
      "document.getElementById('username_field').value = 'my_username';"
      "document.getElementById('password_field').value = 'password';"
      "document.getElementById('input_submit_button').click()";
  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), fill_and_submit));

  first_observer.Wait();
  EXPECT_TRUE(prompt_observer->IsShowingPrompt());
  prompt_observer->Accept();

  // Now navigate to a login form that has similar HTML markup.
  NavigateToFile("/password/password_form.html");

  // Simulate a user click to force an autofill of the form's DOM value, not
  // just the suggested value.
  content::SimulateMouseClick(
      WebContents(), 0, blink::WebMouseEvent::ButtonLeft);

  // The form should be filled with the previously submitted username.
  std::string get_username =
      "window.domAutomationController.send("
      "document.getElementById('username_field').value);";
  std::string actual_username;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(RenderViewHost(),
                                                     get_username,
                                                     &actual_username));
  ASSERT_EQ("my_username", actual_username);

  // Submit the form and verify that there is no infobar (as the password
  // has already been saved).
  NavigationObserver second_observer(WebContents());
  scoped_ptr<PromptObserver> second_prompt_observer(
      PromptObserver::Create(WebContents()));
  std::string submit_form =
      "document.getElementById('input_submit_button').click()";
  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), submit_form));
  second_observer.Wait();
  EXPECT_FALSE(second_prompt_observer->IsShowingPrompt());

  // Verify that we sent two pings to Autofill. One vote for of PASSWORD for
  // the current form, and one vote for ACCOUNT_CREATION_PASSWORD on the
  // original form since it has more than 2 text input fields and was used for
  // the first time on a different form.
  base::HistogramBase* upload_histogram =
      base::StatisticsRecorder::FindHistogram(
          "PasswordGeneration.UploadStarted");
  ASSERT_TRUE(upload_histogram);
  scoped_ptr<base::HistogramSamples> snapshot =
      upload_histogram->SnapshotSamples();
  EXPECT_EQ(0, snapshot->GetCount(0 /* failure */));
  EXPECT_EQ(2, snapshot->GetCount(1 /* success */));
}
#endif

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest, PromptForSubmitFromIframe) {
  NavigateToFile("/password/password_submit_from_iframe.html");

  // Submit a form in an iframe, then cause the whole page to navigate without a
  // user gesture. We expect the save password prompt to be shown here, because
  // some pages use such iframes for login forms.
  NavigationObserver observer(WebContents());
  scoped_ptr<PromptObserver> prompt_observer(
      PromptObserver::Create(WebContents()));
  std::string fill_and_submit =
      "var iframe = document.getElementById('test_iframe');"
      "var iframe_doc = iframe.contentDocument;"
      "iframe_doc.getElementById('username_field').value = 'temp';"
      "iframe_doc.getElementById('password_field').value = 'random';"
      "iframe_doc.getElementById('submit_button').click()";

  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), fill_and_submit));
  observer.Wait();
  EXPECT_TRUE(prompt_observer->IsShowingPrompt());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       PromptForInputElementWithoutName) {
  // Check that the prompt is shown for forms where input elements lack the
  // "name" attribute but the "id" is present.
  NavigateToFile("/password/password_form.html");

  NavigationObserver observer(WebContents());
  scoped_ptr<PromptObserver> prompt_observer(
      PromptObserver::Create(WebContents()));
  std::string fill_and_submit =
      "document.getElementById('username_field_no_name').value = 'temp';"
      "document.getElementById('password_field_no_name').value = 'random';"
      "document.getElementById('input_submit_button_no_name').click()";
  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), fill_and_submit));
  observer.Wait();
  EXPECT_TRUE(prompt_observer->IsShowingPrompt());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       PromptForInputElementWithoutId) {
  // Check that the prompt is shown for forms where input elements lack the
  // "id" attribute but the "name" attribute is present.
  NavigateToFile("/password/password_form.html");

  NavigationObserver observer(WebContents());
  scoped_ptr<PromptObserver> prompt_observer(
      PromptObserver::Create(WebContents()));
  std::string fill_and_submit =
      "document.getElementsByName('username_field_no_id')[0].value = 'temp';"
      "document.getElementsByName('password_field_no_id')[0].value = 'random';"
      "document.getElementsByName('input_submit_button_no_id')[0].click()";
  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), fill_and_submit));
  observer.Wait();
  EXPECT_TRUE(prompt_observer->IsShowingPrompt());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       NoPromptForInputElementWithoutIdAndName) {
  // Check that no prompt is shown for forms where the input fields lack both
  // the "id" and the "name" attributes.
  NavigateToFile("/password/password_form.html");

  NavigationObserver observer(WebContents());
  scoped_ptr<PromptObserver> prompt_observer(
      PromptObserver::Create(WebContents()));
  std::string fill_and_submit =
      "var form = document.getElementById('testform_elements_no_id_no_name');"
      "var username = form.children[0];"
      "username.value = 'temp';"
      "var password = form.children[1];"
      "password.value = 'random';"
      "form.children[2].click()";  // form.children[2] is the submit button.
  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), fill_and_submit));
  observer.Wait();
  EXPECT_FALSE(prompt_observer->IsShowingPrompt());
}

// Test for checking that no prompt is shown for URLs with file: scheme.
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       NoPromptForFileSchemeURLs) {
  GURL url = GetFileURL("password_form.html");
  ui_test_utils::NavigateToURL(browser(), url);

  NavigationObserver observer(WebContents());
  scoped_ptr<PromptObserver> prompt_observer(
      PromptObserver::Create(WebContents()));
  std::string fill_and_submit =
      "document.getElementById('username_field').value = 'temp';"
      "document.getElementById('password_field').value = 'random';"
      "document.getElementById('input_submit_button').click();";
  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), fill_and_submit));
  observer.Wait();
  EXPECT_FALSE(prompt_observer->IsShowingPrompt());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest, DeleteFrameBeforeSubmit) {
  NavigateToFile("/password/multi_frames.html");

  NavigationObserver observer(WebContents());
  // Make sure we save some password info from an iframe and then destroy it.
  std::string save_and_remove =
      "var first_frame = document.getElementById('first_frame');"
      "var frame_doc = first_frame.contentDocument;"
      "frame_doc.getElementById('username_field').value = 'temp';"
      "frame_doc.getElementById('password_field').value = 'random';"
      "frame_doc.getElementById('input_submit_button').click();"
      "first_frame.parentNode.removeChild(first_frame);";
  // Submit from the main frame, but without navigating through the onsubmit
  // handler.
  std::string navigate_frame =
      "document.getElementById('username_field').value = 'temp';"
      "document.getElementById('password_field').value = 'random';"
      "document.getElementById('input_submit_button').click();"
      "window.location.href = 'done.html';";

  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), save_and_remove));
  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), navigate_frame));
  observer.Wait();
  // The only thing we check here is that there is no use-after-free reported.
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest, PasswordValueAccessible) {
  NavigateToFile("/password/form_and_link.html");

  // Click on a link to open a new tab, then switch back to the first one.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  std::string click =
      "document.getElementById('testlink').click();";
  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), click));
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  browser()->tab_strip_model()->ActivateTabAt(0, false);

  // Fill in the credentials, and make sure they are saved.
  NavigationObserver form_submit_observer(WebContents());
  scoped_ptr<PromptObserver> prompt_observer(
      PromptObserver::Create(WebContents()));
  std::string fill_and_submit =
      "document.getElementById('username_field').value = 'temp';"
      "document.getElementById('password_field').value = 'random';"
      "document.getElementById('input_submit_button').click();";
  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), fill_and_submit));
  form_submit_observer.Wait();
  EXPECT_TRUE(prompt_observer->IsShowingPrompt());
  prompt_observer->Accept();

  // Reload the original page to have the saved credentials autofilled.
  NavigationObserver reload_observer(WebContents());
  NavigateToFile("/password/form_and_link.html");
  reload_observer.Wait();

  // Wait until the username is filled, to make sure autofill kicked in.
  WaitForElementValue("username_field", "temp");
  // Now check that the password is not accessible yet.
  CheckElementValue("password_field", "");
  // Let the user interact with the page.
  content::SimulateMouseClickAt(
      WebContents(), 0, blink::WebMouseEvent::ButtonLeft, gfx::Point(1, 1));
  // Wait until that interaction causes the password value to be revealed.
  WaitForElementValue("password_field", "random");
  // And check that after the side-effects of the interaction took place, the
  // username value stays the same.
  CheckElementValue("username_field", "temp");
}

// The following test is limited to Aura, because
// RenderWidgetHostViewGuest::ProcessAckedTouchEvent is, and
// ProcessAckedTouchEvent is what triggers the translation of touch events to
// gesture events.
#if defined(USE_AURA)
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       PasswordValueAccessibleOnSubmit) {
  NavigateToFile("/password/form_and_link.html");

  // Fill in the credentials, and make sure they are saved.
  NavigationObserver form_submit_observer(WebContents());
  scoped_ptr<PromptObserver> prompt_observer(
      PromptObserver::Create(WebContents()));
  std::string fill_and_submit =
      "document.getElementById('username_field').value = 'temp';"
      "document.getElementById('password_field').value = 'random_secret';"
      "document.getElementById('input_submit_button').click();";
  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), fill_and_submit));
  form_submit_observer.Wait();
  EXPECT_TRUE(prompt_observer->IsShowingPrompt());
  prompt_observer->Accept();

  // Reload the original page to have the saved credentials autofilled.
  NavigationObserver reload_observer(WebContents());
  NavigateToFile("/password/form_and_link.html");
  reload_observer.Wait();

  NavigationObserver submit_observer(WebContents());
  // Submit the form via a tap on the submit button. The button is placed at 0,
  // 100, and has height 300 and width 700.
  content::SimulateTapAt(WebContents(), gfx::Point(350, 250));
  submit_observer.Wait();
  std::string query = WebContents()->GetURL().query();
  EXPECT_NE(std::string::npos, query.find("random_secret")) << query;
}
#endif

// Test fix for crbug.com/338650.
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       DontPromptForPasswordFormWithDefaultValue) {
  NavigateToFile("/password/password_form_with_default_value.html");

  // Don't prompt if we navigate away even if there is a password value since
  // it's not coming from the user.
  NavigationObserver observer(WebContents());
  scoped_ptr<PromptObserver> prompt_observer(
      PromptObserver::Create(WebContents()));
  NavigateToFile("/password/done.html");
  observer.Wait();
  EXPECT_FALSE(prompt_observer->IsShowingPrompt());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       PromptWhenEnableAutomaticPasswordSavingSwitchIsNotSet) {
  NavigateToFile("/password/password_form.html");

  // Fill a form and submit through a <input type="submit"> button.
  NavigationObserver observer(WebContents());
  scoped_ptr<PromptObserver> prompt_observer(
      PromptObserver::Create(WebContents()));
  std::string fill_and_submit =
      "document.getElementById('username_field').value = 'temp';"
      "document.getElementById('password_field').value = 'random';"
      "document.getElementById('input_submit_button').click()";
  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), fill_and_submit));
  observer.Wait();
  EXPECT_TRUE(prompt_observer->IsShowingPrompt());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       DontPromptWhenEnableAutomaticPasswordSavingSwitchIsSet) {
  password_manager::TestPasswordStore* password_store =
      static_cast<password_manager::TestPasswordStore*>(
          PasswordStoreFactory::GetForProfile(browser()->profile(),
                                              Profile::IMPLICIT_ACCESS).get());

  EXPECT_TRUE(password_store->IsEmpty());

  NavigateToFile("/password/password_form.html");

  // Add the enable-automatic-password-saving switch.
  CommandLine::ForCurrentProcess()->AppendSwitch(
      password_manager::switches::kEnableAutomaticPasswordSaving);

  // Fill a form and submit through a <input type="submit"> button.
  NavigationObserver observer(WebContents());
  scoped_ptr<PromptObserver> prompt_observer(
      PromptObserver::Create(WebContents()));
  // Make sure that the only passwords saved are the auto-saved ones.
  std::string fill_and_submit =
      "document.getElementById('username_field').value = 'temp';"
      "document.getElementById('password_field').value = 'random';"
      "document.getElementById('input_submit_button').click()";
  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), fill_and_submit));
  observer.Wait();
  if (chrome::VersionInfo::GetChannel() ==
      chrome::VersionInfo::CHANNEL_UNKNOWN) {
    // Passwords getting auto-saved, no prompt.
    EXPECT_FALSE(prompt_observer->IsShowingPrompt());
    EXPECT_FALSE(password_store->IsEmpty());
  } else {
    // Prompt shown, and no passwords saved automatically.
    EXPECT_TRUE(prompt_observer->IsShowingPrompt());
    EXPECT_TRUE(password_store->IsEmpty());
  }
}

// Test fix for crbug.com/368690.
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest, NoPromptWhenReloading) {
  NavigateToFile("/password/password_form.html");

  std::string fill =
      "document.getElementById('username_redirect').value = 'temp';"
      "document.getElementById('password_redirect').value = 'random';";
  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), fill));

  NavigationObserver observer(WebContents());
  scoped_ptr<PromptObserver> prompt_observer(
      PromptObserver::Create(WebContents()));
  GURL url = embedded_test_server()->GetURL("/password/password_form.html");
  chrome::NavigateParams params(browser(), url,
                                ui::PAGE_TRANSITION_RELOAD);
  ui_test_utils::NavigateToURL(&params);
  observer.Wait();
  EXPECT_FALSE(prompt_observer->IsShowingPrompt());
}

// Test that if a form gets dynamically added between the form parsing and
// rendering, and while the main frame still loads, it still is registered, and
// thus saving passwords from it works.
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       FormsAddedBetweenParsingAndRendering) {
  NavigateToFile("/password/between_parsing_and_rendering.html");

  NavigationObserver observer(WebContents());
  scoped_ptr<PromptObserver> prompt_observer(
      PromptObserver::Create(WebContents()));
  std::string submit =
      "document.getElementById('username').value = 'temp';"
      "document.getElementById('password').value = 'random';"
      "document.getElementById('submit-button').click();";
  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), submit));
  observer.Wait();

  EXPECT_TRUE(prompt_observer->IsShowingPrompt());
}

// Test that if there was no previous page load then the PasswordManagerDriver
// does not think that there were SSL errors on the current page. The test opens
// a new tab with a URL for which the embedded test server issues a basic auth
// challenge.
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest, NoLastLoadGoodLastLoad) {
  // Teach the embedded server to handle requests by issuing the basic auth
  // challenge.
  embedded_test_server()->RegisterRequestHandler(
      base::Bind(&HandleTestAuthRequest));

  LoginPromptBrowserTestObserver login_observer;
  // We need to register to all sources, because the navigation observer we are
  // interested in is for a new tab to be opened, and thus does not exist yet.
  login_observer.Register(content::NotificationService::AllSources());

  password_manager::TestPasswordStore* password_store =
      static_cast<password_manager::TestPasswordStore*>(
          PasswordStoreFactory::GetForProfile(browser()->profile(),
                                              Profile::IMPLICIT_ACCESS).get());
  EXPECT_TRUE(password_store->IsEmpty());

  // Navigate to a page requiring HTTP auth. Wait for the tab to get the correct
  // WebContents, but don't wait for navigation, which only finishes after
  // authentication.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      embedded_test_server()->GetURL("/basic_auth"),
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);

  content::NavigationController* nav_controller =
      &WebContents()->GetController();
  NavigationObserver nav_observer(WebContents());
  scoped_ptr<PromptObserver> prompt_observer(
      PromptObserver::Create(WebContents()));
  WindowedAuthNeededObserver auth_needed_observer(nav_controller);
  auth_needed_observer.Wait();

  WindowedAuthSuppliedObserver auth_supplied_observer(nav_controller);
  // Offer valid credentials on the auth challenge.
  ASSERT_EQ(1u, login_observer.handlers().size());
  LoginHandler* handler = *login_observer.handlers().begin();
  ASSERT_TRUE(handler);
  // Any username/password will work.
  handler->SetAuth(base::UTF8ToUTF16("user"), base::UTF8ToUTF16("pwd"));
  auth_supplied_observer.Wait();

  // The password manager should be working correctly.
  nav_observer.Wait();
  EXPECT_TRUE(prompt_observer->IsShowingPrompt());
  prompt_observer->Accept();

  // Spin the message loop to make sure the password store had a chance to save
  // the password.
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
  EXPECT_FALSE(password_store->IsEmpty());
}

// In some situations, multiple PasswordFormManager instances from
// PasswordManager::pending_login_managers_ would match (via DoesManage) a form
// to be provisionally saved. One of them might be a complete match, the other
// all-but-action match. Normally, the former should be preferred, but if the
// former has not finished matching, and the latter has, the latter should be
// used (otherwise we'd give up even though we could have saved the password).
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       PreferPasswordFormManagerWhichFinishedMatching) {
  NavigateToFile("/password/create_form_copy_on_submit.html");

  NavigationObserver observer(WebContents());
  scoped_ptr<PromptObserver> prompt_observer(
      PromptObserver::Create(WebContents()));
  std::string submit =
      "document.getElementById('username').value = 'overwrite_me';"
      "document.getElementById('password').value = 'random';"
      "document.getElementById('non-form-button').click();";
  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), submit));
  observer.Wait();

  EXPECT_TRUE(prompt_observer->IsShowingPrompt());
}

// Test that if login fails and content server pushes a different login form
// with action URL having different schemes. Heuristic shall be able
// identify such cases and *shall not* prompt to save incorrect password.
IN_PROC_BROWSER_TEST_F(
    PasswordManagerBrowserTest,
    NoPromptForLoginFailedAndServerPushSeperateLoginForm_HttpToHttps) {
  std::string path =
      "/password/separate_login_form_with_onload_submit_script.html";
  GURL http_url(embedded_test_server()->GetURL(path));
  ASSERT_TRUE(http_url.SchemeIs(url::kHttpScheme));

  NavigationObserver observer(WebContents());
  scoped_ptr<PromptObserver> prompt_observer(
      PromptObserver::Create(WebContents()));
  ui_test_utils::NavigateToURL(browser(), http_url);

  observer.SetPathToWaitFor("/password/done_and_separate_login_form.html");
  observer.Wait();

  EXPECT_FALSE(prompt_observer->IsShowingPrompt());
}

IN_PROC_BROWSER_TEST_F(
    PasswordManagerBrowserTest,
    NoPromptForLoginFailedAndServerPushSeperateLoginForm_HttpsToHttp) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kAllowRunningInsecureContent);
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kIgnoreCertificateErrors);
  const base::FilePath::CharType kDocRoot[] =
      FILE_PATH_LITERAL("chrome/test/data");
  net::SpawnedTestServer https_test_server(
      net::SpawnedTestServer::TYPE_HTTPS,
      net::SpawnedTestServer::SSLOptions(
          net::SpawnedTestServer::SSLOptions::CERT_OK),
      base::FilePath(kDocRoot));
  ASSERT_TRUE(https_test_server.Start());

  // This test case cannot inject the scripts via content::ExecuteScript() in
  // files served through HTTPS. Therefore the scripts are made part of the HTML
  // site and executed on load.
  std::string path =
      "password/separate_login_form_with_onload_submit_script.html";
  GURL https_url(https_test_server.GetURL(path));
  ASSERT_TRUE(https_url.SchemeIs(url::kHttpsScheme));

  NavigationObserver observer(WebContents());
  scoped_ptr<PromptObserver> prompt_observer(
      PromptObserver::Create(WebContents()));
  ui_test_utils::NavigateToURL(browser(), https_url);

  observer.SetPathToWaitFor("/password/done_and_separate_login_form.html");
  observer.Wait();

  EXPECT_FALSE(prompt_observer->IsShowingPrompt());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       PromptWhenPasswordFormWithoutUsernameFieldSubmitted) {
  password_manager::TestPasswordStore* password_store =
      static_cast<password_manager::TestPasswordStore*>(
          PasswordStoreFactory::GetForProfile(browser()->profile(),
                                              Profile::IMPLICIT_ACCESS).get());

  EXPECT_TRUE(password_store->IsEmpty());

  NavigateToFile("/password/form_with_only_password_field.html");

  NavigationObserver observer(WebContents());
  scoped_ptr<PromptObserver> prompt_observer(
      PromptObserver::Create(WebContents()));
  std::string submit =
      "document.getElementById('password').value = 'password';"
      "document.getElementById('submit-button').click();";
  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), submit));
  observer.Wait();

  EXPECT_TRUE(prompt_observer->IsShowingPrompt());
  prompt_observer->Accept();

  // Spin the message loop to make sure the password store had a chance to save
  // the password.
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
  EXPECT_FALSE(password_store->IsEmpty());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       AutofillSuggetionsForPasswordFormWithoutUsernameField) {
  password_manager::TestPasswordStore* password_store =
      static_cast<password_manager::TestPasswordStore*>(
          PasswordStoreFactory::GetForProfile(browser()->profile(),
                                              Profile::IMPLICIT_ACCESS).get());

  EXPECT_TRUE(password_store->IsEmpty());

  // Password form without username-field.
  NavigateToFile("/password/form_with_only_password_field.html");

  NavigationObserver observer(WebContents());
  scoped_ptr<PromptObserver> prompt_observer(
      PromptObserver::Create(WebContents()));
  std::string submit =
      "document.getElementById('password').value = 'mypassword';"
      "document.getElementById('submit-button').click();";
  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), submit));
  observer.Wait();

  prompt_observer->Accept();

  // Spin the message loop to make sure the password store had a chance to save
  // the password.
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
  EXPECT_FALSE(password_store->IsEmpty());

  // Now, navigate to same html password form and verify whether password is
  // autofilled.
  NavigateToFile("/password/form_with_only_password_field.html");

  // Let the user interact with the page, so that DOM gets modification events,
  // needed for autofilling fields.
  content::SimulateMouseClickAt(
      WebContents(), 0, blink::WebMouseEvent::ButtonLeft, gfx::Point(1, 1));

  // Wait until that interaction causes the password value to be revealed.
  WaitForElementValue("password", "mypassword");
}
