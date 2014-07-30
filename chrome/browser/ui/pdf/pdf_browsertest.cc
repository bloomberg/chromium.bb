// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_enumerator.h"
#include "base/hash.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/pdf/pdf_browsertest_base.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

using content::NavigationController;
using content::WebContents;

// Note: All tests in here require the internal PDF plugin, so they're disabled
// in non-official builds. We still compile them though, to prevent bitrot.

namespace {

// Tests basic PDF rendering.  This can be broken depending on bad merges with
// the vendor, so it's important that we have basic sanity checking.
#if defined(GOOGLE_CHROME_BUILD) && defined(OS_LINUX)
#define MAYBE_Basic Basic
#else
#define MAYBE_Basic DISABLED_Basic
#endif
IN_PROC_BROWSER_TEST_F(PDFBrowserTest, MAYBE_Basic) {
  ASSERT_NO_FATAL_FAILURE(Load());
  ASSERT_NO_FATAL_FAILURE(WaitForResponse());
  // OS X uses CoreText, and FreeType renders slightly different on Linux and
  // Win.
#if defined(OS_MACOSX)
  // The bots render differently than locally, see http://crbug.com/142531.
  ASSERT_TRUE(VerifySnapshot("pdf_browsertest_mac.png") ||
              VerifySnapshot("pdf_browsertest_mac2.png"));
#elif defined(OS_LINUX)
  ASSERT_TRUE(VerifySnapshot("pdf_browsertest_linux.png"));
#else
  ASSERT_TRUE(VerifySnapshot("pdf_browsertest.png"));
#endif
}

#if defined(GOOGLE_CHROME_BUILD) && (defined(OS_WIN) || defined(OS_LINUX))
#define MAYBE_Scroll Scroll
#else
// TODO(thestig): http://crbug.com/79837, http://crbug.com/332778
#define MAYBE_Scroll DISABLED_Scroll
#endif
// Tests that scrolling works.
IN_PROC_BROWSER_TEST_F(PDFBrowserTest, MAYBE_Scroll) {
  ASSERT_NO_FATAL_FAILURE(Load());

  // We use wheel mouse event since that's the only one we can easily push to
  // the renderer.  There's no way to push a cross-platform keyboard event at
  // the moment.
  blink::WebMouseWheelEvent wheel_event;
  wheel_event.type = blink::WebInputEvent::MouseWheel;
  wheel_event.deltaY = -200;
  wheel_event.wheelTicksY = -2;
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  web_contents->GetRenderViewHost()->ForwardWheelEvent(wheel_event);
  ASSERT_NO_FATAL_FAILURE(WaitForResponse());

  int y_offset = 0;
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.domAutomationController.send(plugin.pageYOffset())",
      &y_offset));
  ASSERT_GT(y_offset, 0);
}

const int kLoadingNumberOfParts = 10;

// Tests that loading async pdfs works correctly (i.e. document fully loads).
// This also loads all documents that used to crash, to ensure we don't have
// regressions.
// If it flakes, reopen http://crbug.com/74548.
#if defined(GOOGLE_CHROME_BUILD)
#define MAYBE_Loading Loading
#else
#define MAYBE_Loading DISABLED_Loading
#endif
IN_PROC_BROWSER_TEST_P(PDFBrowserTest, MAYBE_Loading) {
  ASSERT_TRUE(pdf_test_server()->InitializeAndWaitUntilReady());

  NavigationController* controller =
      &(browser()->tab_strip_model()->GetActiveWebContents()->GetController());
  content::NotificationRegistrar registrar;
  registrar.Add(this,
                content::NOTIFICATION_LOAD_STOP,
                content::Source<NavigationController>(controller));
  std::string base_url = std::string("/");

  base::FileEnumerator file_enumerator(
      ui_test_utils::GetTestFilePath(
          base::FilePath(FILE_PATH_LITERAL("pdf_private")), base::FilePath()),
      false,
      base::FileEnumerator::FILES,
      FILE_PATH_LITERAL("*.pdf"));
  for (base::FilePath file_path = file_enumerator.Next();
       !file_path.empty();
       file_path = file_enumerator.Next()) {
    std::string filename = file_path.BaseName().MaybeAsASCII();
    ASSERT_FALSE(filename.empty());

#if defined(OS_POSIX)
    if (filename == "sample.pdf")
      continue;  // Crashes on Mac and Linux.  http://crbug.com/63549
#endif

    // Split the test into smaller sub-tests. Each one only loads
    // every k-th file.
    if (static_cast<int>(base::Hash(filename) % kLoadingNumberOfParts) !=
        GetParam()) {
      continue;
    }

    LOG(WARNING) << "PDFBrowserTest.Loading: " << filename;

    GURL url = pdf_test_server()->GetURL(base_url + filename);
    ui_test_utils::NavigateToURL(browser(), url);

    while (true) {
      int last_count = load_stop_notification_count();
      // We might get extraneous chrome::LOAD_STOP notifications when
      // doing async loading.  This happens when the first loader is cancelled
      // and before creating a byte-range request loader.
      bool complete = false;
      ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
          browser()->tab_strip_model()->GetActiveWebContents(),
          "window.domAutomationController.send(plugin.documentLoadComplete())",
          &complete));
      if (complete)
        break;

      // Check if the LOAD_STOP notification could have come while we run a
      // nested message loop for the JS call.
      if (last_count != load_stop_notification_count())
        continue;
      content::WaitForLoadStop(
          browser()->tab_strip_model()->GetActiveWebContents());
    }
  }
}

INSTANTIATE_TEST_CASE_P(PDFTestFiles,
                        PDFBrowserTest,
                        testing::Range(0, kLoadingNumberOfParts));

#if defined(GOOGLE_CHROME_BUILD) && (defined(OS_WIN) || defined(OS_LINUX))
#define MAYBE_Action Action
#else
// http://crbug.com/315160
#define MAYBE_Action DISABLED_Action
#endif
IN_PROC_BROWSER_TEST_F(PDFBrowserTest, MAYBE_Action) {
  ASSERT_NO_FATAL_FAILURE(Load());

  ASSERT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "document.getElementsByName('plugin')[0].fitToHeight();"));

  std::string zoom1, zoom2;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.domAutomationController.send("
      "    document.getElementsByName('plugin')[0].getZoomLevel().toString())",
      &zoom1));

  ASSERT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "document.getElementsByName('plugin')[0].fitToWidth();"));

  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.domAutomationController.send("
      "    document.getElementsByName('plugin')[0].getZoomLevel().toString())",
      &zoom2));
  ASSERT_NE(zoom1, zoom2);
}

#if defined(GOOGLE_CHROME_BUILD) && defined(OS_LINUX)
#define MAYBE_OnLoadAndReload OnLoadAndReload
#else
// Flaky as per http://crbug.com/74549.
#define MAYBE_OnLoadAndReload DISABLED_OnLoadAndReload
#endif
IN_PROC_BROWSER_TEST_F(PDFBrowserTest, MAYBE_OnLoadAndReload) {
  ASSERT_TRUE(pdf_test_server()->InitializeAndWaitUntilReady());

  GURL url = pdf_test_server()->GetURL("/onload_reload.html");
  ui_test_utils::NavigateToURL(browser(), url);
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();

  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<NavigationController>(
          &contents->GetController()));
  ASSERT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "reloadPDF();"));
  observer.Wait();

  ASSERT_EQ("success", contents->GetURL().query());
}

}  // namespace
