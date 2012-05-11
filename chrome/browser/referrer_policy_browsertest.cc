// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/tab_contents/render_view_context_menu.h"
#include "chrome/browser/tab_contents/render_view_context_menu_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "net/test/test_server.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"

// GTK requires a X11-level mouse event to open a context menu correctly.
#if defined(TOOLKIT_GTK)
#define MAYBE_ContextMenuOrigin DISABLED_ContextMenuOrigin
#define MAYBE_HttpsContextMenuOrigin DISABLED_HttpsContextMenuOrigin
#define MAYBE_ContextMenuRedirect DISABLED_ContextMenuRedirect
#define MAYBE_HttpsContextMenuRedirect DISABLED_HttpsContextMenuRedirect
#else
#define MAYBE_ContextMenuOrigin ContextMenuOrigin
#define MAYBE_HttpsContextMenuOrigin HttpsContextMenuOrigin
#define MAYBE_ContextMenuRedirect ContextMenuRedirect
// http://crbug.com/124750
#define MAYBE_HttpsContextMenuRedirect FAILS_HttpsContextMenuRedirect
#endif

namespace {

const FilePath::CharType kDocRoot[] =
    FILE_PATH_LITERAL("chrome/test/data/referrer_policy");

}  // namespace

class ReferrerPolicyTest : public InProcessBrowserTest {
 public:
  enum ExpectedReferrer {
    EXPECT_EMPTY_REFERRER,
    EXPECT_FULL_REFERRER,
    EXPECT_ORIGIN_AS_REFERRER
  };

  // Returns the expected title for the tab with the given (full) referrer and
  // the expected modification of it.
  string16 GetExpectedTitle(const GURL& url,
                            ExpectedReferrer expected_referrer) {
    std::string referrer;
    switch (expected_referrer) {
      case EXPECT_EMPTY_REFERRER:
        referrer = "Referrer is empty";
        break;
      case EXPECT_FULL_REFERRER:
        referrer = "Referrer is " + url.spec();
        break;
      case EXPECT_ORIGIN_AS_REFERRER:
        referrer = "Referrer is " + url.GetWithEmptyPath().spec();
        break;
    }
    return ASCIIToUTF16(referrer);
  }

  // Navigates from a page with a given |referrer_policy| and checks that the
  // reported referrer matches the expectation.
  // Parameters:
  //  referrer_policy:   The referrer policy to test ("default", "always",
  //                     "origin", "never")
  //  start_on_https:    True if the test should start on an HTTPS page.
  //  target_blank:      True if the link that is generated should have the
  //                     attribute target=_blank
  //  redirect:          True if the link target should first do a server
  //                     redirect before evaluating the passed referrer.
  //  opens_new_tab:     True if this test opens a new tab.
  //  button:            If not WebMouseEvent::ButtonNone, click on the
  //                     link with the specified mouse button.
  //  expected_referrer: The kind of referrer to expect.
  void RunReferrerTest(const std::string referrer_policy,
                       bool start_on_https,
                       bool target_blank,
                       bool redirect,
                       bool opens_new_tab,
                       WebKit::WebMouseEvent::Button button,
                       ExpectedReferrer expected_referrer) {
    net::TestServer test_server(net::TestServer::TYPE_HTTP,
                                net::TestServer::kLocalhost,
                                FilePath(kDocRoot));
    ASSERT_TRUE(test_server.Start());
    net::TestServer ssl_test_server(net::TestServer::TYPE_HTTPS,
                                    net::TestServer::kLocalhost,
                                    FilePath(kDocRoot));
    ASSERT_TRUE(ssl_test_server.Start());

    GURL start_url;
    net::TestServer* start_server =
        start_on_https ? &ssl_test_server : &test_server;
    start_url = start_server->GetURL(
        std::string("files/referrer-policy-start.html?") +
        "policy=" + referrer_policy +
        "&port=" + base::IntToString(test_server.host_port_pair().port()) +
        "&ssl_port=" +
            base::IntToString(ssl_test_server.host_port_pair().port()) +
        "&redirect=" + (redirect ? "true" : "false") +
        "&link=" +
            (button == WebKit::WebMouseEvent::ButtonNone ? "false" : "true") +
        "&target=" + (target_blank ? "_blank" : ""));

    ui_test_utils::WindowedTabAddedNotificationObserver tab_added_observer(
        content::NotificationService::AllSources());

    string16 expected_title = GetExpectedTitle(start_url, expected_referrer);
    content::WebContents* tab = browser()->GetSelectedWebContents();
    ui_test_utils::TitleWatcher title_watcher(tab, expected_title);

    // Watch for all possible outcomes to avoid timeouts if something breaks.
    title_watcher.AlsoWaitForTitle(
        GetExpectedTitle(start_url, EXPECT_EMPTY_REFERRER));
    title_watcher.AlsoWaitForTitle(
        GetExpectedTitle(start_url, EXPECT_FULL_REFERRER));
    title_watcher.AlsoWaitForTitle(
        GetExpectedTitle(start_url, EXPECT_ORIGIN_AS_REFERRER));

    ui_test_utils::NavigateToURL(browser(), start_url);

    if (button != WebKit::WebMouseEvent::ButtonNone) {
      WebKit::WebMouseEvent mouse_event;
      mouse_event.type = WebKit::WebInputEvent::MouseDown;
      mouse_event.button = button;
      mouse_event.x = 15;
      mouse_event.y = 15;
      mouse_event.clickCount = 1;
      tab->GetRenderViewHost()->ForwardMouseEvent(mouse_event);
      mouse_event.type = WebKit::WebInputEvent::MouseUp;
      tab->GetRenderViewHost()->ForwardMouseEvent(mouse_event);
    }

    if (opens_new_tab) {
      tab_added_observer.Wait();
      tab = tab_added_observer.GetTab();
      ASSERT_TRUE(tab != NULL);
      ui_test_utils::WaitForLoadStop(tab);
      EXPECT_EQ(expected_title, tab->GetTitle());
    } else {
      EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
    }
  }
};

// The basic behavior of referrer policies is covered by layout tests in
// http/tests/security/referrer-policy-*. These tests cover (hopefully) all
// code paths chrome uses to navigate. To keep the number of combinations down,
// we only test the "origin" policy here.
//
// Some tests are marked as FAILS, see http://crbug.com/124750

// Content initiated navigation, from HTTP to HTTP.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, Origin) {
  RunReferrerTest("origin", false, false, false, false,
                  WebKit::WebMouseEvent::ButtonNone,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// Content initiated navigation, from HTTPS to HTTP.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, HttpsDefault) {
  RunReferrerTest("origin", true, false, false, false,
                  WebKit::WebMouseEvent::ButtonNone,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// User initiated navigation, from HTTP to HTTP.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, LeftClickOrigin) {
  RunReferrerTest("origin", false, false, false, false,
                  WebKit::WebMouseEvent::ButtonLeft,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// User initiated navigation, from HTTPS to HTTP.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, HttpsLeftClickOrigin) {
  RunReferrerTest("origin", true, false, false, false,
                  WebKit::WebMouseEvent::ButtonLeft,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// User initiated navigation, middle click, from HTTP to HTTP.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, MiddleClickOrigin) {
  RunReferrerTest("origin", false, false, false, true,
                  WebKit::WebMouseEvent::ButtonMiddle,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// User initiated navigation, middle click, from HTTPS to HTTP.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, FAILS_HttpsMiddleClickOrigin) {
  RunReferrerTest("origin", true, false, false, true,
                  WebKit::WebMouseEvent::ButtonMiddle,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// User initiated navigation, target blank, from HTTP to HTTP.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, TargetBlankOrigin) {
  RunReferrerTest("origin", false, true, false, true,
                  WebKit::WebMouseEvent::ButtonLeft,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// User initiated navigation, target blank, from HTTPS to HTTP.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, HttpsTargetBlankOrigin) {
  RunReferrerTest("origin", true, true, false, true,
                  WebKit::WebMouseEvent::ButtonLeft,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// User initiated navigation, middle click, target blank, from HTTP to HTTP.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, MiddleClickTargetBlankOrigin) {
  RunReferrerTest("origin", false, true, false, true,
                  WebKit::WebMouseEvent::ButtonMiddle,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// User initiated navigation, middle click, target blank, from HTTPS to HTTP.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, HttpsMiddleClickTargetBlankOrigin) {
  RunReferrerTest("origin", true, true, false, true,
                  WebKit::WebMouseEvent::ButtonMiddle,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// Context menu, from HTTP to HTTP.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, MAYBE_ContextMenuOrigin) {
  ContextMenuNotificationObserver context_menu_observer(
      IDC_CONTENT_CONTEXT_OPENLINKNEWTAB,
      chrome::NOTIFICATION_TAB_ADDED);
  RunReferrerTest("origin", false, false, false, true,
                  WebKit::WebMouseEvent::ButtonRight,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// Context menu, from HTTPS to HTTP.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, MAYBE_HttpsContextMenuOrigin) {
  ContextMenuNotificationObserver context_menu_observer(
      IDC_CONTENT_CONTEXT_OPENLINKNEWTAB,
      chrome::NOTIFICATION_TAB_ADDED);
  RunReferrerTest("origin", true, false, false, true,
                  WebKit::WebMouseEvent::ButtonRight,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// Content initiated navigation, from HTTP to HTTP via server redirect.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, Redirect) {
  RunReferrerTest("origin", false, false, true, false,
                  WebKit::WebMouseEvent::ButtonNone,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// Content initiated navigation, from HTTPS to HTTP via server redirect.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, HttpsRedirect) {
  RunReferrerTest("origin", true, false, true, false,
                  WebKit::WebMouseEvent::ButtonNone,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// User initiated navigation, from HTTP to HTTP via server redirect.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, LeftClickRedirect) {
  RunReferrerTest("origin", false, false, true, false,
                  WebKit::WebMouseEvent::ButtonLeft,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// User initiated navigation, from HTTPS to HTTP via server redirect.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, HttpsLeftClickRedirect) {
  RunReferrerTest("origin", true, false, true, false,
                  WebKit::WebMouseEvent::ButtonLeft,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// User initiated navigation, middle click, from HTTP to HTTP via server
// redirect.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, MiddleClickRedirect) {
  RunReferrerTest("origin", false, false, true, true,
                  WebKit::WebMouseEvent::ButtonMiddle,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// User initiated navigation, middle click, from HTTPS to HTTP via server
// redirect.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, FAILS_HttpsMiddleClickRedirect) {
  RunReferrerTest("origin", true, false, true, true,
                  WebKit::WebMouseEvent::ButtonMiddle,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// User initiated navigation, target blank, from HTTP to HTTP via server
// redirect.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, TargetBlankRedirect) {
  RunReferrerTest("origin", false, true, true, true,
                  WebKit::WebMouseEvent::ButtonLeft,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// User initiated navigation, target blank, from HTTPS to HTTP via server
// redirect.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, HttpsTargetBlankRedirect) {
  RunReferrerTest("origin", true, true, true, true,
                  WebKit::WebMouseEvent::ButtonLeft,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// User initiated navigation, middle click, target blank, from HTTP to HTTP via
// server redirect.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, MiddleClickTargetBlankRedirect) {
  RunReferrerTest("origin", false, true, true, true,
                  WebKit::WebMouseEvent::ButtonMiddle,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// User initiated navigation, middle click, target blank, from HTTPS to HTTP
// via server redirect.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest,
                       HttpsMiddleClickTargetBlankRedirect) {
  RunReferrerTest("origin", true, true, true, true,
                  WebKit::WebMouseEvent::ButtonMiddle,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// Context menu, from HTTP to HTTP via server redirect.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, MAYBE_ContextMenuRedirect) {
  ContextMenuNotificationObserver context_menu_observer(
      IDC_CONTENT_CONTEXT_OPENLINKNEWTAB,
      chrome::NOTIFICATION_TAB_ADDED);
  RunReferrerTest("origin", false, false, true, true,
                  WebKit::WebMouseEvent::ButtonRight,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// Context menu, from HTTPS to HTTP via server redirect.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, MAYBE_HttpsContextMenuRedirect) {
  ContextMenuNotificationObserver context_menu_observer(
      IDC_CONTENT_CONTEXT_OPENLINKNEWTAB,
      chrome::NOTIFICATION_TAB_ADDED);
  RunReferrerTest("origin", true, false, true, true,
                  WebKit::WebMouseEvent::ButtonRight,
                  EXPECT_ORIGIN_AS_REFERRER);
}
