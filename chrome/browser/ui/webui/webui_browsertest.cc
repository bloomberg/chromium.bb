// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "third_party/WebKit/public/platform/WebMouseEvent.h"
#include "third_party/WebKit/public/platform/WebMouseWheelEvent.h"

namespace {

class TestWebUIMessageHandler : public content::WebUIMessageHandler {
 public:
  void RegisterMessages() override {
    web_ui()->RegisterMessageCallback(
        "messageRequiringGesture",
        base::BindRepeating(&TestWebUIMessageHandler::OnMessageRequiringGesture,
                            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        "notifyFinish",
        base::BindRepeating(&TestWebUIMessageHandler::OnNotifyFinish,
                            base::Unretained(this)));
  }

  void set_finish_closure(base::RepeatingClosure closure) {
    finish_closure_ = std::move(closure);
  }

  int message_requiring_gesture_count() const {
    return message_requiring_gesture_count_;
  }

 private:
  void OnMessageRequiringGesture(const base::ListValue* args) {
    ++message_requiring_gesture_count_;
  }

  void OnNotifyFinish(const base::ListValue* args) {
    if (finish_closure_)
      finish_closure_.Run();
  }

  int message_requiring_gesture_count_ = 0;
  base::RepeatingClosure finish_closure_;
};

}  // namespace

using WebUIImplBrowserTest = InProcessBrowserTest;

// Tests that navigating between WebUIs of different types results in
// SiteInstance swap when running in process-per-tab process model.
IN_PROC_BROWSER_TEST_F(WebUIImplBrowserTest, ForceSwapOnDifferenteWebUITypes) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kProcessPerTab);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  const GURL web_ui_url(std::string(content::kChromeUIScheme) + "://" +
                        std::string(chrome::kChromeUIFlagsHost));
  EXPECT_TRUE(ChromeWebUIControllerFactory::GetInstance()->UseWebUIForURL(
      web_contents->GetBrowserContext(), web_ui_url));
  ui_test_utils::NavigateToURL(browser(), web_ui_url);
  EXPECT_TRUE(
      content::ChildProcessSecurityPolicy::GetInstance()->HasWebUIBindings(
          web_contents->GetMainFrame()->GetProcess()->GetID()));

  // Capture the SiteInstance before navigating for later comparison.
  scoped_refptr<content::SiteInstance> orig_site_instance(
      web_contents->GetSiteInstance());

  // Navigate to a different WebUI type and ensure that the SiteInstance
  // has changed and the new process also has WebUI bindings.
  const GURL web_ui_url2(std::string(content::kChromeUIScheme) + "://" +
                         std::string(chrome::kChromeUIVersionHost));
  EXPECT_TRUE(ChromeWebUIControllerFactory::GetInstance()->UseWebUIForURL(
      web_contents->GetBrowserContext(), web_ui_url2));
  ui_test_utils::NavigateToURL(browser(), web_ui_url2);
  EXPECT_NE(orig_site_instance, web_contents->GetSiteInstance());
  EXPECT_TRUE(
      content::ChildProcessSecurityPolicy::GetInstance()->HasWebUIBindings(
          web_contents->GetMainFrame()->GetProcess()->GetID()));
}

IN_PROC_BROWSER_TEST_F(WebUIImplBrowserTest, SameDocumentNavigationsAndReload) {
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUITermsURL));

  content::WebUIMessageHandler* test_handler = new TestWebUIMessageHandler;
  browser()
      ->tab_strip_model()
      ->GetActiveWebContents()
      ->GetWebUI()
      ->AddMessageHandler(base::WrapUnique(test_handler));
  test_handler->AllowJavascriptForTesting();

  // Push onto window.history. Back should now be an in-page navigation.
  ASSERT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.history.pushState({}, '', 'foo.html')"));
  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents());

  // Test handler should still have JavaScript allowed after in-page navigation.
  EXPECT_TRUE(test_handler->IsJavascriptAllowed());

  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents());

  // Verify that after a reload, the test handler has been disallowed.
  EXPECT_FALSE(test_handler->IsJavascriptAllowed());
}

// A WebUI message that should require a user gesture is ignored if there is no
// recent input event.
IN_PROC_BROWSER_TEST_F(WebUIImplBrowserTest,
                       MessageRequiringGestureIgnoredIfNoGesture) {
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIFlagsURL));

  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  auto* test_handler = new TestWebUIMessageHandler();
  web_contents->GetWebUI()->AddMessageHandler(base::WrapUnique(test_handler));

  auto* main_rfh = web_contents->GetMainFrame();

  main_rfh->ExecuteJavaScriptForTests(
      base::UTF8ToUTF16("chrome.send('messageRequiringGesture');"
                        "chrome.send('notifyFinish');"));
  base::RunLoop run_loop;
  test_handler->set_finish_closure(run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(0, test_handler->message_requiring_gesture_count());
}

IN_PROC_BROWSER_TEST_F(WebUIImplBrowserTest,
                       MessageRequiringGestureIgnoresRendererOnlyGesture) {
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIFlagsURL));

  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  auto* test_handler = new TestWebUIMessageHandler();
  web_contents->GetWebUI()->AddMessageHandler(base::WrapUnique(test_handler));

  auto* main_rfh = web_contents->GetMainFrame();

  main_rfh->ExecuteJavaScriptWithUserGestureForTests(
      base::UTF8ToUTF16("chrome.send('messageRequiringGesture');"
                        "chrome.send('notifyFinish');"));
  base::RunLoop run_loop;
  test_handler->set_finish_closure(run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(0, test_handler->message_requiring_gesture_count());
}

IN_PROC_BROWSER_TEST_F(WebUIImplBrowserTest,
                       MessageRequiringGestureIgnoresNonInteractiveEvents) {
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIFlagsURL));

  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  auto* test_handler = new TestWebUIMessageHandler();
  web_contents->GetWebUI()->AddMessageHandler(base::WrapUnique(test_handler));

  auto* main_rfh = web_contents->GetMainFrame();

  // Mouse enter / mouse move / mouse leave should not be considered input
  // events that interact with the page.
  content::SimulateMouseEvent(web_contents, blink::WebInputEvent::kMouseEnter,
                              gfx::Point(50, 50));
  content::SimulateMouseEvent(web_contents, blink::WebInputEvent::kMouseMove,
                              gfx::Point(50, 50));
  content::SimulateMouseEvent(web_contents, blink::WebInputEvent::kMouseLeave,
                              gfx::Point(50, 50));
  // Nor should mouse wheel.
  content::SimulateMouseWheelEvent(web_contents, gfx::Point(50, 50),
                                   gfx::Vector2d(0, 100),
                                   blink::WebMouseWheelEvent::kPhaseBegan);
  main_rfh->ExecuteJavaScriptForTests(
      base::UTF8ToUTF16("chrome.send('messageRequiringGesture');"
                        "chrome.send('notifyFinish');"));
  base::RunLoop run_loop;
  test_handler->set_finish_closure(run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(0, test_handler->message_requiring_gesture_count());
}

IN_PROC_BROWSER_TEST_F(WebUIImplBrowserTest,
                       MessageRequiringGestureAllowedWithInteractiveEvent) {
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIFlagsURL));

  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  auto* test_handler = new TestWebUIMessageHandler();
  web_contents->GetWebUI()->AddMessageHandler(base::WrapUnique(test_handler));

  auto* main_rfh = web_contents->GetMainFrame();

  content::SimulateMouseClick(web_contents, 0,
                              blink::WebMouseEvent::Button::kLeft);
  main_rfh->ExecuteJavaScriptForTests(
      base::UTF8ToUTF16("chrome.send('messageRequiringGesture');"
                        "chrome.send('notifyFinish');"));
  base::RunLoop run_loop;
  test_handler->set_finish_closure(run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(1, test_handler->message_requiring_gesture_count());
}
