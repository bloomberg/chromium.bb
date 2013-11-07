// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/net_util.h"
#include "url/gurl.h"

class ChromeRenderWidgetHostViewMacDelegateTest : public InProcessBrowserTest {
 public:
  ChromeRenderWidgetHostViewMacDelegateTest() {
    const base::FilePath base_path(FILE_PATH_LITERAL("scroll"));
    url1_ = ui_test_utils::GetTestUrl(
        base_path, base::FilePath(FILE_PATH_LITERAL("text.html")));
    url2_ = ui_test_utils::GetTestUrl(
        base_path, base::FilePath(FILE_PATH_LITERAL("blank.html")));
  }

 protected:
  // Navigates back.
  void GoBack() {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    chrome::GoBack(browser(), CURRENT_TAB);
    observer.Wait();
  }

  // Returns the active web contents.
  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  // Returns the value of |query| from Javascript as an int.
  int GetScriptIntValue(const std::string& query) {
    int value = 0;
    EXPECT_TRUE(content::ExecuteScriptAndExtractInt(
        GetWebContents(),
        "domAutomationController.send(" + query + ")",
        &value));
    return value;
  }

  // Returns the vertical scroll offset of the current page.
  int GetScrollTop() {
    return GetScriptIntValue("document.body.scrollTop");
  }

  // Returns the horizontal scroll offset of the current page.
  int GetScrollLeft() {
    return GetScriptIntValue("document.body.scrollLeft");
  }

  // Simulates a mouse wheel event, forwarding it to the renderer.
  void SendWheelEvent(int dx, int dy, blink::WebMouseWheelEvent::Phase phase) {
    blink::WebMouseWheelEvent event;
    event.type = blink::WebInputEvent::MouseWheel;
    event.phase = phase;
    event.deltaX = dx;
    event.deltaY = dy;
    event.wheelTicksY = -2;
    event.hasPreciseScrollingDeltas = 1;
    GetWebContents()->GetRenderViewHost()->ForwardWheelEvent(event);
  }

  GURL url1_;
  GURL url2_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeRenderWidgetHostViewMacDelegateTest);
};

IN_PROC_BROWSER_TEST_F(ChromeRenderWidgetHostViewMacDelegateTest,
                       GoBackScrollOffset) {
  ui_test_utils::NavigateToURL(browser(), url1_);
  ASSERT_EQ(url1_, GetWebContents()->GetURL());

  SendWheelEvent(0, -200, blink::WebMouseWheelEvent::PhaseNone);
  const int scroll_offset = GetScrollTop();
  ASSERT_NE(0, scroll_offset);

  ui_test_utils::NavigateToURL(browser(), url2_);
  ASSERT_EQ(url2_, GetWebContents()->GetURL());
  ASSERT_EQ(0, GetScrollTop());

  GoBack();
  ASSERT_EQ(url1_, GetWebContents()->GetURL());
  ASSERT_EQ(scroll_offset, GetScrollTop());
}

IN_PROC_BROWSER_TEST_F(ChromeRenderWidgetHostViewMacDelegateTest,
                       GoBackUsingGestureScrollOffset) {
  ui_test_utils::NavigateToURL(browser(), url1_);
  ASSERT_EQ(url1_, GetWebContents()->GetURL());

  SendWheelEvent(0, -200, blink::WebMouseWheelEvent::PhaseNone);
  const int scroll_offset = GetScrollTop();
  ASSERT_NE(0, scroll_offset);

  ui_test_utils::NavigateToURL(browser(), url2_);
  ASSERT_EQ(url2_, GetWebContents()->GetURL());
  ASSERT_EQ(0, GetScrollTop());

  // Send wheel events that shouldn't be handled by the web content since it's
  // not scrollable in the horizontal direction.
  SendWheelEvent(500, 0, blink::WebMouseWheelEvent::PhaseBegan);
  SendWheelEvent(500, 0, blink::WebMouseWheelEvent::PhaseEnded);
  ASSERT_EQ(0, GetScrollLeft());

  // Simulate a back being triggered as a result of the unhandled wheel events.
  // This doesn't invoke the code in ChromeRenderWidgetHostViewMacDelegate
  // because that expects NSEvents which are much harder to fake.
  GoBack();
  ASSERT_EQ(url1_, GetWebContents()->GetURL());
  ASSERT_EQ(scroll_offset, GetScrollTop());
}
