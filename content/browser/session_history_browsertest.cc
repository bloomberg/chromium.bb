// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "net/test/test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

class SessionHistoryTest : public InProcessBrowserTest {
 protected:
  SessionHistoryTest() {
    EnableDOMAutomation();
  }

  virtual void SetUpOnMainThread() {
    ASSERT_TRUE(test_server()->Start());
  }

  // Simulate clicking a link.  Only works on the frames.html testserver page.
  void ClickLink(std::string node_id) {
    GURL url("javascript:clickLink('" + node_id + "')");
    ui_test_utils::NavigateToURL(browser(), url);
  }

  // Simulate filling in form data.  Only works on the frames.html page with
  // subframe = form.html, and on form.html itself.
  void FillForm(std::string node_id, std::string value) {
    GURL url("javascript:fillForm('" + node_id + "', '" + value + "')");
    // This will return immediately, but since the JS executes synchronously
    // on the renderer, it will complete before the next navigate message is
    // processed.
    ui_test_utils::NavigateToURLWithDisposition(browser(), url, CURRENT_TAB, 0);
  }

  // Simulate submitting a form.  Only works on the frames.html page with
  // subframe = form.html, and on form.html itself.
  void SubmitForm(std::string node_id) {
    GURL url("javascript:submitForm('" + node_id + "')");
    ui_test_utils::NavigateToURL(browser(), url);
  }

  // Navigate session history using history.go(distance).
  void JavascriptGo(std::string distance) {
    GURL url("javascript:history.go('" + distance + "')");
    ui_test_utils::NavigateToURL(browser(), url);
  }

  std::string GetTabTitle() {
    return UTF16ToASCII(browser()->GetActiveWebContents()->GetTitle());
  }

  GURL GetTabURL() {
    return browser()->GetActiveWebContents()->GetURL();
  }

  GURL GetURL(const std::string file) {
    return test_server()->GetURL(std::string("files/session_history/") + file);
  }

  void NavigateAndCheckTitle(const char* filename,
                             const std::string& expected_title) {
    string16 expected_title16(ASCIIToUTF16(expected_title));
    ui_test_utils::TitleWatcher title_watcher(
        browser()->GetActiveWebContents(), expected_title16);
    ui_test_utils::NavigateToURL(browser(), GetURL(filename));
    ASSERT_EQ(expected_title16, title_watcher.WaitAndGetTitle());
  }

  void GoBack() {
    ui_test_utils::WindowedNotificationObserver load_stop_observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
    browser()->GoBack(CURRENT_TAB);
    load_stop_observer.Wait();
  }

  void GoForward() {
    ui_test_utils::WindowedNotificationObserver load_stop_observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
    browser()->GoForward(CURRENT_TAB);
    load_stop_observer.Wait();
  }
};

// If this flakes, use http://crbug.com/61619 on windows and
// http://crbug.com/102094 on mac.
IN_PROC_BROWSER_TEST_F(SessionHistoryTest, BasicBackForward) {
  // about:blank should be loaded first.
  ASSERT_FALSE(browser()->CanGoBack());
  EXPECT_EQ("about:blank", GetTabTitle());

  ASSERT_NO_FATAL_FAILURE(NavigateAndCheckTitle("bot1.html", "bot1"));
  ASSERT_NO_FATAL_FAILURE(NavigateAndCheckTitle("bot2.html", "bot2"));
  ASSERT_NO_FATAL_FAILURE(NavigateAndCheckTitle("bot3.html", "bot3"));

  // history is [blank, bot1, bot2, *bot3]

  GoBack();
  EXPECT_EQ("bot2", GetTabTitle());

  GoBack();
  EXPECT_EQ("bot1", GetTabTitle());

  GoForward();
  EXPECT_EQ("bot2", GetTabTitle());

  GoBack();
  EXPECT_EQ("bot1", GetTabTitle());

  ASSERT_NO_FATAL_FAILURE(NavigateAndCheckTitle("bot3.html", "bot3"));

  // history is [blank, bot1, *bot3]

  ASSERT_FALSE(browser()->CanGoForward());
  EXPECT_EQ("bot3", GetTabTitle());

  GoBack();
  EXPECT_EQ("bot1", GetTabTitle());

  GoBack();
  EXPECT_EQ("about:blank", GetTabTitle());

  ASSERT_FALSE(browser()->CanGoBack());
  EXPECT_EQ("about:blank", GetTabTitle());

  GoForward();
  EXPECT_EQ("bot1", GetTabTitle());

  GoForward();
  EXPECT_EQ("bot3", GetTabTitle());
}

// Test that back/forward works when navigating in subframes.
// If this flakes, use http://crbug.com/48833
IN_PROC_BROWSER_TEST_F(SessionHistoryTest, FrameBackForward) {
  // about:blank should be loaded first.
  GURL home(chrome::kAboutBlankURL);
  ASSERT_FALSE(browser()->CanGoBack());
  EXPECT_EQ("about:blank", GetTabTitle());
  EXPECT_EQ(GURL(chrome::kAboutBlankURL), GetTabURL());

  ASSERT_NO_FATAL_FAILURE(NavigateAndCheckTitle("frames.html", "bot1"));

  ClickLink("abot2");
  EXPECT_EQ("bot2", GetTabTitle());
  GURL frames(GetURL("frames.html"));
  EXPECT_EQ(frames, GetTabURL());

  ClickLink("abot3");
  EXPECT_EQ("bot3", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  // history is [blank, bot1, bot2, *bot3]

  GoBack();
  EXPECT_EQ("bot2", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  GoBack();
  EXPECT_EQ("bot1", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  GoBack();
  EXPECT_EQ("about:blank", GetTabTitle());
  EXPECT_EQ(home, GetTabURL());

  GoForward();
  EXPECT_EQ("bot1", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  GoForward();
  EXPECT_EQ("bot2", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  ClickLink("abot1");
  EXPECT_EQ("bot1", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  // history is [blank, bot1, bot2, *bot1]

  ASSERT_FALSE(browser()->CanGoForward());
  EXPECT_EQ("bot1", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  GoBack();
  EXPECT_EQ("bot2", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  GoBack();
  EXPECT_EQ("bot1", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());
}

// Test that back/forward preserves POST data and document state in subframes.
// If this flakes use http://crbug.com/61619
IN_PROC_BROWSER_TEST_F(SessionHistoryTest, FrameFormBackForward) {
  // about:blank should be loaded first.
  ASSERT_FALSE(browser()->CanGoBack());
  EXPECT_EQ("about:blank", GetTabTitle());

  ASSERT_NO_FATAL_FAILURE(NavigateAndCheckTitle("frames.html", "bot1"));

  ClickLink("aform");
  EXPECT_EQ("form", GetTabTitle());
  GURL frames(GetURL("frames.html"));
  EXPECT_EQ(frames, GetTabURL());

  SubmitForm("isubmit");
  EXPECT_EQ("text=&select=a", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  GoBack();
  EXPECT_EQ("form", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  // history is [blank, bot1, *form, post]

  ClickLink("abot2");
  EXPECT_EQ("bot2", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  // history is [blank, bot1, form, *bot2]

  GoBack();
  EXPECT_EQ("form", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  SubmitForm("isubmit");
  EXPECT_EQ("text=&select=a", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  // history is [blank, bot1, form, *post]

  // TODO(mpcomplete): reenable this when WebKit bug 10199 is fixed:
  // "returning to a POST result within a frame does a GET instead of a POST"
  ClickLink("abot2");
  EXPECT_EQ("bot2", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  GoBack();
  EXPECT_EQ("text=&select=a", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());
}

// TODO(mpcomplete): enable this when Bug 734372 is fixed:
// "Doing a session history navigation does not restore newly-created subframe
// document state"
// Test that back/forward preserves POST data and document state when navigating
// across frames (ie, from frame -> nonframe).
// Hangs, see http://crbug.com/45058.
IN_PROC_BROWSER_TEST_F(SessionHistoryTest, CrossFrameFormBackForward) {
  // about:blank should be loaded first.
  ASSERT_FALSE(browser()->CanGoBack());
  EXPECT_EQ("about:blank", GetTabTitle());

  GURL frames(GetURL("frames.html"));
  ASSERT_NO_FATAL_FAILURE(NavigateAndCheckTitle("frames.html", "bot1"));

  ClickLink("aform");
  EXPECT_EQ("form", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  SubmitForm("isubmit");
  EXPECT_EQ("text=&select=a", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  GoBack();
  EXPECT_EQ("form", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  // history is [blank, bot1, *form, post]

  ASSERT_NO_FATAL_FAILURE(NavigateAndCheckTitle("bot2.html", "bot2"));

  // history is [blank, bot1, form, *bot2]

  GoBack();
  EXPECT_EQ("bot1", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  SubmitForm("isubmit");
  EXPECT_EQ("text=&select=a", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());
}

// Test that back/forward entries are created for reference fragment
// navigations. Bug 730379.
// If this flakes use http://crbug.com/61619.
IN_PROC_BROWSER_TEST_F(SessionHistoryTest, FragmentBackForward) {
  // about:blank should be loaded first.
  ASSERT_FALSE(browser()->CanGoBack());
  EXPECT_EQ("about:blank", GetTabTitle());

  GURL fragment(GetURL("fragment.html"));
  ASSERT_NO_FATAL_FAILURE(NavigateAndCheckTitle("fragment.html", "fragment"));

  ASSERT_NO_FATAL_FAILURE(NavigateAndCheckTitle("fragment.html#a", "fragment"));
  ASSERT_NO_FATAL_FAILURE(NavigateAndCheckTitle("fragment.html#b", "fragment"));
  ASSERT_NO_FATAL_FAILURE(NavigateAndCheckTitle("fragment.html#c", "fragment"));

  // history is [blank, fragment, fragment#a, fragment#b, *fragment#c]

  GoBack();
  EXPECT_EQ(GetURL("fragment.html#b"), GetTabURL());

  GoBack();
  EXPECT_EQ(GetURL("fragment.html#a"), GetTabURL());

  GoBack();
  EXPECT_EQ(GetURL("fragment.html"), GetTabURL());

  GoForward();
  EXPECT_EQ(GetURL("fragment.html#a"), GetTabURL());

  ASSERT_NO_FATAL_FAILURE(NavigateAndCheckTitle("bot3.html", "bot3"));

  // history is [blank, fragment, fragment#a, bot3]

  ASSERT_FALSE(browser()->CanGoForward());
  EXPECT_EQ(GetURL("bot3.html"), GetTabURL());

  GoBack();
  EXPECT_EQ(GetURL("fragment.html#a"), GetTabURL());

  GoBack();
  EXPECT_EQ(GetURL("fragment.html"), GetTabURL());
}

// Test that the javascript window.history object works.
// NOTE: history.go(N) does not do anything if N is outside the bounds of the
// back/forward list (such as trigger our start/stop loading events).  This
// means the test will hang if it attempts to navigate too far forward or back,
// since we'll be waiting forever for a load stop event.
//
// TODO(brettw) bug 50648: fix flakyness. This test seems like it was failing
// about 1/4 of the time on Vista by failing to execute JavascriptGo (see bug).
IN_PROC_BROWSER_TEST_F(SessionHistoryTest, JavascriptHistory) {
  // about:blank should be loaded first.
  ASSERT_FALSE(browser()->CanGoBack());
  EXPECT_EQ("about:blank", GetTabTitle());

  ASSERT_NO_FATAL_FAILURE(NavigateAndCheckTitle("bot1.html", "bot1"));
  ASSERT_NO_FATAL_FAILURE(NavigateAndCheckTitle("bot2.html", "bot2"));
  ASSERT_NO_FATAL_FAILURE(NavigateAndCheckTitle("bot3.html", "bot3"));

  // history is [blank, bot1, bot2, *bot3]

  JavascriptGo("-1");
  EXPECT_EQ("bot2", GetTabTitle());

  JavascriptGo("-1");
  EXPECT_EQ("bot1", GetTabTitle());

  JavascriptGo("1");
  EXPECT_EQ("bot2", GetTabTitle());

  JavascriptGo("-1");
  EXPECT_EQ("bot1", GetTabTitle());

  JavascriptGo("2");
  EXPECT_EQ("bot3", GetTabTitle());

  // history is [blank, bot1, bot2, *bot3]

  JavascriptGo("-3");
  EXPECT_EQ("about:blank", GetTabTitle());

  ASSERT_FALSE(browser()->CanGoBack());
  EXPECT_EQ("about:blank", GetTabTitle());

  JavascriptGo("1");
  EXPECT_EQ("bot1", GetTabTitle());

  ASSERT_NO_FATAL_FAILURE(NavigateAndCheckTitle("bot3.html", "bot3"));

  // history is [blank, bot1, *bot3]

  ASSERT_FALSE(browser()->CanGoForward());
  EXPECT_EQ("bot3", GetTabTitle());

  JavascriptGo("-1");
  EXPECT_EQ("bot1", GetTabTitle());

  JavascriptGo("-1");
  EXPECT_EQ("about:blank", GetTabTitle());

  ASSERT_FALSE(browser()->CanGoBack());
  EXPECT_EQ("about:blank", GetTabTitle());

  JavascriptGo("1");
  EXPECT_EQ("bot1", GetTabTitle());

  JavascriptGo("1");
  EXPECT_EQ("bot3", GetTabTitle());

  // TODO(creis): Test that JavaScript history navigations work across tab
  // types.  For example, load about:network in a tab, then a real page, then
  // try to go back and forward with JavaScript.  Bug 1136715.
  // (Hard to test right now, because pages like about:network cause the
  // TabProxy to hang.  This is because they do not appear to use the
  // NotificationService.)
}

// This test is failing consistently. See http://crbug.com/22560
IN_PROC_BROWSER_TEST_F(SessionHistoryTest, LocationReplace) {
  // Test that using location.replace doesn't leave the title of the old page
  // visible.
  ASSERT_NO_FATAL_FAILURE(NavigateAndCheckTitle(
      "replace.html?bot1.html", "bot1"));
}

IN_PROC_BROWSER_TEST_F(SessionHistoryTest, LocationChangeInSubframe) {
  ASSERT_NO_FATAL_FAILURE(NavigateAndCheckTitle(
      "location_redirect.html", "Default Title"));

  ui_test_utils::NavigateToURL(
      browser(), GURL("javascript:void(frames[0].navigate())"));
  EXPECT_EQ("foo", GetTabTitle());

  GoBack();
  EXPECT_EQ("Default Title", GetTabTitle());
}

// http://code.google.com/p/chromium/issues/detail?id=56267
IN_PROC_BROWSER_TEST_F(SessionHistoryTest, HistoryLength) {
  int length;
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractInt(
      browser()->GetActiveWebContents()->GetRenderViewHost(),
      L"", L"domAutomationController.send(history.length)", &length));
  EXPECT_EQ(1, length);

  ui_test_utils::NavigateToURL(browser(), GetURL("title1.html"));

  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractInt(
      browser()->GetActiveWebContents()->GetRenderViewHost(),
      L"", L"domAutomationController.send(history.length)", &length));
  EXPECT_EQ(2, length);

  // Now test that history.length is updated when the navigation is committed.
  ui_test_utils::NavigateToURL(browser(), GetURL("record_length.html"));

  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractInt(
      browser()->GetActiveWebContents()->GetRenderViewHost(),
      L"", L"domAutomationController.send(history.length)", &length));
  EXPECT_EQ(3, length);

  GoBack();
  GoBack();

  // Ensure history.length is properly truncated.
  ui_test_utils::NavigateToURL(browser(), GetURL("title2.html"));

  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractInt(
      browser()->GetActiveWebContents()->GetRenderViewHost(),
      L"", L"domAutomationController.send(history.length)", &length));
  EXPECT_EQ(2, length);
}
