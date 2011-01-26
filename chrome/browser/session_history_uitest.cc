// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "grit/generated_resources.h"
#include "net/base/net_util.h"
#include "net/test/test_server.h"

namespace {

class SessionHistoryTest : public UITest {
 protected:
  SessionHistoryTest()
      : test_server_(net::TestServer::TYPE_HTTP,
                     FilePath(FILE_PATH_LITERAL("chrome/test/data"))) {
    dom_automation_enabled_ = true;
  }

  virtual void SetUp() {
    UITest::SetUp();

    window_ = automation()->GetBrowserWindow(0);
    ASSERT_TRUE(window_.get());

    tab_ = window_->GetActiveTab();
    ASSERT_TRUE(tab_.get());
  }

  // Simulate clicking a link.  Only works on the frames.html testserver page.
  void ClickLink(std::string node_id) {
    GURL url("javascript:clickLink('" + node_id + "')");
    ASSERT_TRUE(tab_->NavigateToURL(url));
  }

  // Simulate filling in form data.  Only works on the frames.html page with
  // subframe = form.html, and on form.html itself.
  void FillForm(std::string node_id, std::string value) {
    GURL url("javascript:fillForm('" + node_id + "', '" + value + "')");
    // This will return immediately, but since the JS executes synchronously
    // on the renderer, it will complete before the next navigate message is
    // processed.
    ASSERT_TRUE(tab_->NavigateToURLAsync(url));
  }

  // Simulate submitting a form.  Only works on the frames.html page with
  // subframe = form.html, and on form.html itself.
  void SubmitForm(std::string node_id) {
    GURL url("javascript:submitForm('" + node_id + "')");
    ASSERT_TRUE(tab_->NavigateToURL(url));
  }

  // Navigate session history using history.go(distance).
  void JavascriptGo(std::string distance) {
    GURL url("javascript:history.go('" + distance + "')");
    ASSERT_TRUE(tab_->NavigateToURL(url));
  }

  std::wstring GetTabTitle() {
    std::wstring title;
    EXPECT_TRUE(tab_->GetTabTitle(&title));
    return title;
  }

  GURL GetTabURL() {
    GURL url;
    EXPECT_TRUE(tab_->GetCurrentURL(&url));
    return url;
  }

 protected:
  scoped_refptr<BrowserProxy> window_;
  scoped_refptr<TabProxy> tab_;

  net::TestServer test_server_;
};

#if defined(OS_WIN)
// See http://crbug.com/61619
#define MAYBE_BasicBackForward FLAKY_BasicBackForward
#else
#define MAYBE_BasicBackForward BasicBackForward
#endif

TEST_F(SessionHistoryTest, MAYBE_BasicBackForward) {
  ASSERT_TRUE(test_server_.Start());

  // about:blank should be loaded first.
  ASSERT_FALSE(tab_->GoBack());
  EXPECT_EQ(L"", GetTabTitle());

  ASSERT_TRUE(tab_->NavigateToURL(
      test_server_.GetURL("files/session_history/bot1.html")));
  EXPECT_EQ(L"bot1", GetTabTitle());

  ASSERT_TRUE(tab_->NavigateToURL(
      test_server_.GetURL("files/session_history/bot2.html")));
  EXPECT_EQ(L"bot2", GetTabTitle());

  ASSERT_TRUE(tab_->NavigateToURL(
      test_server_.GetURL("files/session_history/bot3.html")));
  EXPECT_EQ(L"bot3", GetTabTitle());

  // history is [blank, bot1, bot2, *bot3]

  ASSERT_TRUE(tab_->GoBack());
  EXPECT_EQ(L"bot2", GetTabTitle());

  ASSERT_TRUE(tab_->GoBack());
  EXPECT_EQ(L"bot1", GetTabTitle());

  ASSERT_TRUE(tab_->GoForward());
  EXPECT_EQ(L"bot2", GetTabTitle());

  ASSERT_TRUE(tab_->GoBack());
  EXPECT_EQ(L"bot1", GetTabTitle());

  ASSERT_TRUE(tab_->NavigateToURL(
      test_server_.GetURL("files/session_history/bot3.html")));
  EXPECT_EQ(L"bot3", GetTabTitle());

  // history is [blank, bot1, *bot3]

  ASSERT_FALSE(tab_->GoForward());
  EXPECT_EQ(L"bot3", GetTabTitle());

  ASSERT_TRUE(tab_->GoBack());
  EXPECT_EQ(L"bot1", GetTabTitle());

  ASSERT_TRUE(tab_->GoBack());
  EXPECT_EQ(L"", GetTabTitle());

  ASSERT_FALSE(tab_->GoBack());
  EXPECT_EQ(L"", GetTabTitle());

  ASSERT_TRUE(tab_->GoForward());
  EXPECT_EQ(L"bot1", GetTabTitle());

  ASSERT_TRUE(tab_->GoForward());
  EXPECT_EQ(L"bot3", GetTabTitle());
}

// Test that back/forward works when navigating in subframes.
// Flaky, http://crbug.com/61619 and http://crbug.com/70883.
TEST_F(SessionHistoryTest, FLAKY_FrameBackForward) {
  ASSERT_TRUE(test_server_.Start());

  // about:blank should be loaded first.
  GURL home(homepage());
  ASSERT_FALSE(tab_->GoBack());
  EXPECT_EQ(L"", GetTabTitle());
  EXPECT_EQ(home, GetTabURL());

  GURL frames(test_server_.GetURL("files/session_history/frames.html"));
  ASSERT_TRUE(tab_->NavigateToURL(frames));
  EXPECT_EQ(L"bot1", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  ClickLink("abot2");
  EXPECT_EQ(L"bot2", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  ClickLink("abot3");
  EXPECT_EQ(L"bot3", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  // history is [blank, bot1, bot2, *bot3]

  ASSERT_TRUE(tab_->GoBack());
  EXPECT_EQ(L"bot2", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  ASSERT_TRUE(tab_->GoBack());
  EXPECT_EQ(L"bot1", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  ASSERT_TRUE(tab_->GoBack());
  EXPECT_EQ(L"", GetTabTitle());
  EXPECT_EQ(home, GetTabURL());

  ASSERT_TRUE(tab_->GoForward());
  EXPECT_EQ(L"bot1", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  ASSERT_TRUE(tab_->GoForward());
  EXPECT_EQ(L"bot2", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  ClickLink("abot1");
  EXPECT_EQ(L"bot1", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  // history is [blank, bot1, bot2, *bot1]

  ASSERT_FALSE(tab_->GoForward());
  EXPECT_EQ(L"bot1", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  ASSERT_TRUE(tab_->GoBack());
  EXPECT_EQ(L"bot2", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  ASSERT_TRUE(tab_->GoBack());
  EXPECT_EQ(L"bot1", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());
}

#if defined(OS_WIN)
// See http://crbug.com/61619
#define MAYBE_FrameFormBackForward FLAKY_FrameFormBackForward
#else
#define MAYBE_FrameFormBackForward FrameFormBackForward
#endif

// Test that back/forward preserves POST data and document state in subframes.
TEST_F(SessionHistoryTest, MAYBE_FrameFormBackForward) {
  ASSERT_TRUE(test_server_.Start());

  // about:blank should be loaded first.
  ASSERT_FALSE(tab_->GoBack());
  EXPECT_EQ(L"", GetTabTitle());

  GURL frames(test_server_.GetURL("files/session_history/frames.html"));
  ASSERT_TRUE(tab_->NavigateToURL(frames));
  EXPECT_EQ(L"bot1", GetTabTitle());

  ClickLink("aform");
  EXPECT_EQ(L"form", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  SubmitForm("isubmit");
  EXPECT_EQ(L"text=&select=a", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  ASSERT_TRUE(tab_->GoBack());
  EXPECT_EQ(L"form", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  // history is [blank, bot1, *form, post]

  ClickLink("abot2");
  EXPECT_EQ(L"bot2", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  // history is [blank, bot1, form, *bot2]

  ASSERT_TRUE(tab_->GoBack());
  EXPECT_EQ(L"form", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  SubmitForm("isubmit");
  EXPECT_EQ(L"text=&select=a", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  // history is [blank, bot1, form, *post]

  if (false) {
    // TODO(mpcomplete): reenable this when WebKit bug 10199 is fixed:
    // "returning to a POST result within a frame does a GET instead of a POST"
    ClickLink("abot2");
    EXPECT_EQ(L"bot2", GetTabTitle());
    EXPECT_EQ(frames, GetTabURL());

    ASSERT_TRUE(tab_->GoBack());
    EXPECT_EQ(L"text=&select=a", GetTabTitle());
    EXPECT_EQ(frames, GetTabURL());
  }
}

// TODO(mpcomplete): enable this when Bug 734372 is fixed:
// "Doing a session history navigation does not restore newly-created subframe
// document state"
// Test that back/forward preserves POST data and document state when navigating
// across frames (ie, from frame -> nonframe).
// Hangs, see http://crbug.com/45058.
TEST_F(SessionHistoryTest, DISABLED_CrossFrameFormBackForward) {
  ASSERT_TRUE(test_server_.Start());

  // about:blank should be loaded first.
  ASSERT_FALSE(tab_->GoBack());
  EXPECT_EQ(L"", GetTabTitle());

  GURL frames(test_server_.GetURL("files/session_history/frames.html"));
  ASSERT_TRUE(tab_->NavigateToURL(frames));
  EXPECT_EQ(L"bot1", GetTabTitle());

  ClickLink("aform");
  EXPECT_EQ(L"form", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  SubmitForm("isubmit");
  EXPECT_EQ(L"text=&select=a", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  ASSERT_TRUE(tab_->GoBack());
  EXPECT_EQ(L"form", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  // history is [blank, bot1, *form, post]

  // ClickLink(L"abot2");
  GURL bot2("files/session_history/bot2.html");
  ASSERT_TRUE(tab_->NavigateToURL(bot2));
  EXPECT_EQ(L"bot2", GetTabTitle());
  EXPECT_EQ(bot2, GetTabURL());

  // history is [blank, bot1, form, *bot2]

  ASSERT_TRUE(tab_->GoBack());
  EXPECT_EQ(L"form", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  SubmitForm("isubmit");
  EXPECT_EQ(L"text=&select=a", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());
}


#if defined(OS_WIN)
// See http://crbug.com/61619
#define MAYBE_FragmentBackForward FLAKY_FragmentBackForward
#else
#define MAYBE_FragmentBackForward FragmentBackForward
#endif

// Test that back/forward entries are created for reference fragment
// navigations. Bug 730379.
TEST_F(SessionHistoryTest, MAYBE_FragmentBackForward) {
  ASSERT_TRUE(test_server_.Start());

  // about:blank should be loaded first.
  ASSERT_FALSE(tab_->GoBack());
  EXPECT_EQ(L"", GetTabTitle());

  GURL fragment(test_server_.GetURL("files/session_history/fragment.html"));
  ASSERT_TRUE(tab_->NavigateToURL(fragment));
  EXPECT_EQ(L"fragment", GetTabTitle());
  EXPECT_EQ(fragment, GetTabURL());

  GURL::Replacements ref_params;

  ref_params.SetRef("a", url_parse::Component(0, 1));
  GURL fragment_a(fragment.ReplaceComponents(ref_params));
  ASSERT_TRUE(tab_->NavigateToURL(fragment_a));
  EXPECT_EQ(L"fragment", GetTabTitle());
  EXPECT_EQ(fragment_a, GetTabURL());

  ref_params.SetRef("b", url_parse::Component(0, 1));
  GURL fragment_b(fragment.ReplaceComponents(ref_params));
  ASSERT_TRUE(tab_->NavigateToURL(fragment_b));
  EXPECT_EQ(L"fragment", GetTabTitle());
  EXPECT_EQ(fragment_b, GetTabURL());

  ref_params.SetRef("c", url_parse::Component(0, 1));
  GURL fragment_c(fragment.ReplaceComponents(ref_params));
  ASSERT_TRUE(tab_->NavigateToURL(fragment_c));
  EXPECT_EQ(L"fragment", GetTabTitle());
  EXPECT_EQ(fragment_c, GetTabURL());

  // history is [blank, fragment, fragment#a, fragment#b, *fragment#c]

  ASSERT_TRUE(tab_->GoBack());
  EXPECT_EQ(fragment_b, GetTabURL());

  ASSERT_TRUE(tab_->GoBack());
  EXPECT_EQ(fragment_a, GetTabURL());

  ASSERT_TRUE(tab_->GoBack());
  EXPECT_EQ(fragment, GetTabURL());

  ASSERT_TRUE(tab_->GoForward());
  EXPECT_EQ(fragment_a, GetTabURL());

  GURL bot3(test_server_.GetURL("files/session_history/bot3.html"));
  ASSERT_TRUE(tab_->NavigateToURL(bot3));
  EXPECT_EQ(L"bot3", GetTabTitle());
  EXPECT_EQ(bot3, GetTabURL());

  // history is [blank, fragment, fragment#a, bot3]

  ASSERT_FALSE(tab_->GoForward());
  EXPECT_EQ(bot3, GetTabURL());

  ASSERT_TRUE(tab_->GoBack());
  EXPECT_EQ(fragment_a, GetTabURL());

  ASSERT_TRUE(tab_->GoBack());
  EXPECT_EQ(fragment, GetTabURL());
}

// Test that the javascript window.history object works.
// NOTE: history.go(N) does not do anything if N is outside the bounds of the
// back/forward list (such as trigger our start/stop loading events).  This
// means the test will hang if it attempts to navigate too far forward or back,
// since we'll be waiting forever for a load stop event.
//
// TODO(brettw) bug 50648: fix flakyness. This test seems like it was failing
// about 1/4 of the time on Vista by failing to execute JavascriptGo (see bug).
TEST_F(SessionHistoryTest, FLAKY_JavascriptHistory) {
  ASSERT_TRUE(test_server_.Start());

  // about:blank should be loaded first.
  ASSERT_FALSE(tab_->GoBack());
  EXPECT_EQ(L"", GetTabTitle());

  ASSERT_TRUE(tab_->NavigateToURL(
      test_server_.GetURL("files/session_history/bot1.html")));
  EXPECT_EQ(L"bot1", GetTabTitle());

  ASSERT_TRUE(tab_->NavigateToURL(
      test_server_.GetURL("files/session_history/bot2.html")));
  EXPECT_EQ(L"bot2", GetTabTitle());

  ASSERT_TRUE(tab_->NavigateToURL(
      test_server_.GetURL("files/session_history/bot3.html")));
  EXPECT_EQ(L"bot3", GetTabTitle());

  // history is [blank, bot1, bot2, *bot3]

  JavascriptGo("-1");
  EXPECT_EQ(L"bot2", GetTabTitle());

  JavascriptGo("-1");
  EXPECT_EQ(L"bot1", GetTabTitle());

  JavascriptGo("1");
  EXPECT_EQ(L"bot2", GetTabTitle());

  JavascriptGo("-1");
  EXPECT_EQ(L"bot1", GetTabTitle());

  JavascriptGo("2");
  EXPECT_EQ(L"bot3", GetTabTitle());

  // history is [blank, bot1, bot2, *bot3]

  JavascriptGo("-3");
  EXPECT_EQ(L"", GetTabTitle());

  ASSERT_FALSE(tab_->GoBack());
  EXPECT_EQ(L"", GetTabTitle());

  JavascriptGo("1");
  EXPECT_EQ(L"bot1", GetTabTitle());

  ASSERT_TRUE(tab_->NavigateToURL(
      test_server_.GetURL("files/session_history/bot3.html")));
  EXPECT_EQ(L"bot3", GetTabTitle());

  // history is [blank, bot1, *bot3]

  ASSERT_FALSE(tab_->GoForward());
  EXPECT_EQ(L"bot3", GetTabTitle());

  JavascriptGo("-1");
  EXPECT_EQ(L"bot1", GetTabTitle());

  JavascriptGo("-1");
  EXPECT_EQ(L"", GetTabTitle());

  ASSERT_FALSE(tab_->GoBack());
  EXPECT_EQ(L"", GetTabTitle());

  JavascriptGo("1");
  EXPECT_EQ(L"bot1", GetTabTitle());

  JavascriptGo("1");
  EXPECT_EQ(L"bot3", GetTabTitle());

  // TODO(creis): Test that JavaScript history navigations work across tab
  // types.  For example, load about:network in a tab, then a real page, then
  // try to go back and forward with JavaScript.  Bug 1136715.
  // (Hard to test right now, because pages like about:network cause the
  // TabProxy to hang.  This is because they do not appear to use the
  // NotificationService.)
}

// This test is failing consistently. See http://crbug.com/22560
TEST_F(SessionHistoryTest, FAILS_LocationReplace) {
  ASSERT_TRUE(test_server_.Start());

  // Test that using location.replace doesn't leave the title of the old page
  // visible.
  ASSERT_TRUE(tab_->NavigateToURL(test_server_.GetURL(
      "files/session_history/replace.html?no-title.html")));
  EXPECT_EQ(L"", GetTabTitle());
}

// This test is flaky. See bug 22111.
TEST_F(SessionHistoryTest, FLAKY_HistorySearchXSS) {
  // about:blank should be loaded first.
  ASSERT_FALSE(tab_->GoBack());
  EXPECT_EQ(L"", GetTabTitle());

  GURL url(std::string(chrome::kChromeUIHistoryURL) +
      "#q=%3Cimg%20src%3Dx%3Ax%20onerror%3D%22document.title%3D'XSS'%22%3E");
  ASSERT_TRUE(tab_->NavigateToURL(url));
  // Mainly, this is to ensure we send a synchronous message to the renderer
  // so that we're not susceptible (less susceptible?) to a race condition.
  // Should a race condition ever trigger, it won't result in flakiness.
  int num = tab_->FindInPage(L"<img", FWD, CASE_SENSITIVE, false, NULL);
  EXPECT_GT(num, 0);
  EXPECT_EQ(L"History", GetTabTitle());
}

#if defined(OS_WIN)
// See http://crbug.com/61619
#define MAYBE_LocationChangeInSubframe FLAKY_LocationChangeInSubframe
#else
#define MAYBE_LocationChangeInSubframe LocationChangeInSubframe
#endif

TEST_F(SessionHistoryTest, MAYBE_LocationChangeInSubframe) {
  ASSERT_TRUE(test_server_.Start());

  ASSERT_TRUE(tab_->NavigateToURL(test_server_.GetURL(
      "files/session_history/location_redirect.html")));
  EXPECT_EQ(L"Default Title", GetTabTitle());

  ASSERT_TRUE(tab_->NavigateToURL(GURL(
      "javascript:void(frames[0].navigate())")));
  EXPECT_EQ(L"foo", GetTabTitle());

  ASSERT_TRUE(tab_->GoBack());
  EXPECT_EQ(L"Default Title", GetTabTitle());
}

// http://code.google.com/p/chromium/issues/detail?id=56267
TEST_F(SessionHistoryTest, DISABLED_HistoryLength) {
  ASSERT_TRUE(test_server_.Start());

  int length;
  ASSERT_TRUE(tab_->ExecuteAndExtractInt(
      L"", L"domAutomationController.send(history.length)", &length));
  EXPECT_EQ(1, length);

  ASSERT_TRUE(tab_->NavigateToURL(test_server_.GetURL("files/title1.html")));

  ASSERT_TRUE(tab_->ExecuteAndExtractInt(
      L"", L"domAutomationController.send(history.length)", &length));
  EXPECT_EQ(2, length);

  // Now test that history.length is updated when the navigation is committed.
  ASSERT_TRUE(tab_->NavigateToURL(test_server_.GetURL(
      "files/session_history/record_length.html")));
  ASSERT_TRUE(tab_->ExecuteAndExtractInt(
      L"", L"domAutomationController.send(history.length)", &length));
  EXPECT_EQ(3, length);
  ASSERT_TRUE(tab_->ExecuteAndExtractInt(
      L"", L"domAutomationController.send(history_length)", &length));
  EXPECT_EQ(3, length);

  ASSERT_TRUE(tab_->GoBack());
  ASSERT_TRUE(tab_->GoBack());

  // Ensure history.length is properly truncated.
  ASSERT_TRUE(tab_->NavigateToURL(test_server_.GetURL("files/title2.html")));
  ASSERT_TRUE(tab_->ExecuteAndExtractInt(
      L"", L"domAutomationController.send(history.length)", &length));
  EXPECT_EQ(2, length);
}

}  // namespace
