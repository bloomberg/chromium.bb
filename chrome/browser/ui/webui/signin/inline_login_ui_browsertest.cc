// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_chrome_web_ui_controller_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/session_storage_namespace.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/base/url_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace {

struct ContentInfo {
  ContentInfo(int pid, content::StoragePartition* storage_partition) {
    this->pid = pid;
    this->storage_partition = storage_partition;
  }

  int pid;
  content::StoragePartition* storage_partition;
};

ContentInfo NavigateAndGetInfo(
    Browser* browser,
    const GURL& url,
    WindowOpenDisposition disposition) {
  ui_test_utils::NavigateToURLWithDisposition(
      browser, url, disposition,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  content::WebContents* contents =
      browser->tab_strip_model()->GetActiveWebContents();
  content::RenderProcessHost* process = contents->GetRenderProcessHost();
  return ContentInfo(process->GetID(), process->GetStoragePartition());
}

// Returns a new WebUI object for the WebContents from |arg0|.
ACTION(ReturnNewWebUI) {
  return new content::WebUIController(arg0);
}

// Mock the TestChromeWebUIControllerFactory::WebUIProvider to prove that we are
// not called as expected.
class FooWebUIProvider
    : public TestChromeWebUIControllerFactory::WebUIProvider {
 public:
  MOCK_METHOD2(NewWebUI, content::WebUIController*(content::WebUI* web_ui,
                                                   const GURL& url));
};

const char kFooWebUIURL[] = "chrome://foo/";

}  // namespace

class InlineLoginUIBrowserTest : public InProcessBrowserTest {
 public:
  InlineLoginUIBrowserTest() {}
};

IN_PROC_BROWSER_TEST_F(InlineLoginUIBrowserTest, DifferentStorageId) {
  GURL test_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title1.html")));

  ContentInfo info1 =
      NavigateAndGetInfo(browser(), test_url, CURRENT_TAB);
  ContentInfo info2 =
      NavigateAndGetInfo(browser(),
                         signin::GetPromoURL(signin::SOURCE_START_PAGE, false),
                         CURRENT_TAB);
  NavigateAndGetInfo(browser(), test_url, CURRENT_TAB);
  ContentInfo info3 =
      NavigateAndGetInfo(browser(),
                         signin::GetPromoURL( signin::SOURCE_START_PAGE, false),
                         NEW_FOREGROUND_TAB);

  // The info for signin should be the same.
  ASSERT_EQ(info2.storage_partition, info3.storage_partition);
  // The info for test_url and signin should be different.
  ASSERT_NE(info1.storage_partition, info2.storage_partition);
}

IN_PROC_BROWSER_TEST_F(InlineLoginUIBrowserTest, OneProcessLimit) {
  GURL test_url_1 = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title1.html")));
  GURL test_url_2 = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("data:text/html,Hello world!")));

  // Even when the process limit is set to one, the signin process should
  // still be given its own process and storage partition.
  content::RenderProcessHost::SetMaxRendererProcessCount(1);

  ContentInfo info1 =
      NavigateAndGetInfo(browser(), test_url_1, CURRENT_TAB);
  ContentInfo info2 =
      NavigateAndGetInfo(browser(), test_url_2, CURRENT_TAB);
  ContentInfo info3 =
      NavigateAndGetInfo(browser(),
                         signin::GetPromoURL( signin::SOURCE_START_PAGE, false),
                         CURRENT_TAB);

  ASSERT_EQ(info1.pid, info2.pid);
  ASSERT_NE(info1.pid, info3.pid);
}

class InlineLoginUISafeIframeBrowserTest : public InProcessBrowserTest {
 public:
  FooWebUIProvider& foo_provider() { return foo_provider_; }

  void WaitUntilUIReady() {
    content::DOMMessageQueue message_queue;
    ASSERT_TRUE(content::ExecuteScript(
        browser()->tab_strip_model()->GetActiveWebContents(),
        "if (!inline.login.getAuthExtHost())"
        "  inline.login.initialize();"
        "var handler = function() {"
        "  window.domAutomationController.setAutomationId(0);"
        "  window.domAutomationController.send('ready');"
        "};"
        "if (inline.login.isAuthReady())"
        "  handler();"
        "else"
        "  inline.login.getAuthExtHost().addEventListener('ready', handler);"));

    std::string message;
    do {
      ASSERT_TRUE(message_queue.WaitForMessage(&message));
    } while (message != "\"ready\"");
  }

 private:
  virtual void SetUpOnMainThread() OVERRIDE {
    content::WebUIControllerFactory::UnregisterFactoryForTesting(
        ChromeWebUIControllerFactory::GetInstance());
    test_factory_.reset(new TestChromeWebUIControllerFactory);
    content::WebUIControllerFactory::RegisterFactory(test_factory_.get());
    test_factory_->AddFactoryOverride(
        GURL(kFooWebUIURL).host(), &foo_provider_);
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    test_factory_->RemoveFactoryOverride(GURL(kFooWebUIURL).host());
    content::WebUIControllerFactory::UnregisterFactoryForTesting(
        test_factory_.get());
    test_factory_.reset();
  }

  FooWebUIProvider foo_provider_;
  scoped_ptr<TestChromeWebUIControllerFactory> test_factory_;
};

// Make sure that the foo webui handler is working properly and that it gets
// created when navigated to normally.
IN_PROC_BROWSER_TEST_F(InlineLoginUISafeIframeBrowserTest, Basic) {
  const GURL kUrl(kFooWebUIURL);
  EXPECT_CALL(foo_provider(), NewWebUI(_, ::testing::Eq(kUrl)))
      .WillOnce(ReturnNewWebUI());
  ui_test_utils::NavigateToURL(browser(), GURL(kFooWebUIURL));
}

// Make sure that the foo webui handler does not get created when we try to
// load it inside the iframe of the login ui.
IN_PROC_BROWSER_TEST_F(InlineLoginUISafeIframeBrowserTest, NoWebUIInIframe) {
  GURL url = signin::GetPromoURL(signin::SOURCE_START_PAGE, false).
      Resolve("?source=0&frameUrl=chrome://foo");
  EXPECT_CALL(foo_provider(), NewWebUI(_, _)).Times(0);
  ui_test_utils::NavigateToURL(browser(), url);
}

// Flaky on CrOS, http://crbug.com/364759.
#if defined(OS_CHROMEOS)
#define MAYBE_TopFrameNavigationDisallowed DISABLED_TopFrameNavigationDisallowed
#else
#define MAYBE_TopFrameNavigationDisallowed TopFrameNavigationDisallowed
#endif

// Make sure that the gaia iframe cannot trigger top-frame navigation.
// TODO(guohui): flaky on trybot crbug/364759.
IN_PROC_BROWSER_TEST_F(InlineLoginUISafeIframeBrowserTest,
    MAYBE_TopFrameNavigationDisallowed) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  // Loads into gaia iframe a web page that attempts to deframe on load.
  GURL deframe_url(embedded_test_server()->GetURL("/login/deframe.html"));
  GURL url(net::AppendOrReplaceQueryParameter(
      signin::GetPromoURL(signin::SOURCE_START_PAGE, false),
      "frameUrl", deframe_url.spec()));
  ui_test_utils::NavigateToURL(browser(), url);
  WaitUntilUIReady();

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(url, contents->GetVisibleURL());

  content::NavigationController& controller = contents->GetController();
  EXPECT_TRUE(controller.GetPendingEntry() == NULL);
}

// Flaky on CrOS, http://crbug.com/364759.
#if defined(OS_CHROMEOS)
#define MAYBE_NavigationToOtherChromeURLDisallowed \
    DISABLED_NavigationToOtherChromeURLDisallowed
#else
#define MAYBE_NavigationToOtherChromeURLDisallowed \
    NavigationToOtherChromeURLDisallowed
#endif

IN_PROC_BROWSER_TEST_F(InlineLoginUISafeIframeBrowserTest,
    MAYBE_NavigationToOtherChromeURLDisallowed) {
  ui_test_utils::NavigateToURL(
      browser(), signin::GetPromoURL(signin::SOURCE_START_PAGE, false));
  WaitUntilUIReady();

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::ExecuteScript(
      contents, "window.location.href = 'chrome://foo'"));

  content::TestNavigationObserver navigation_observer(contents, 1);
  navigation_observer.Wait();

  EXPECT_EQ(GURL("about:blank"), contents->GetVisibleURL());
}
