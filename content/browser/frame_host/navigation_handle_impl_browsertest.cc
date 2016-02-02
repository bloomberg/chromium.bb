// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/navigation_handle_impl.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/page_transition_types.h"

namespace content {

namespace {

class NavigationHandleObserver : public WebContentsObserver {
 public:
  NavigationHandleObserver(WebContents* web_contents, const GURL& expected_url)
      : WebContentsObserver(web_contents),
        handle_(nullptr),
        has_committed_(false),
        is_error_(false),
        page_transition_(ui::PAGE_TRANSITION_LINK),
        expected_url_(expected_url) {}

  void DidStartNavigation(NavigationHandle* navigation_handle) override {
    if (handle_ || navigation_handle->GetURL() != expected_url_)
      return;

    handle_ = navigation_handle;
    has_committed_ = false;
    is_error_ = false;
    page_transition_ = ui::PAGE_TRANSITION_LINK;
    last_committed_url_ = GURL();
  }

  void DidFinishNavigation(NavigationHandle* navigation_handle) override {
    if (navigation_handle != handle_)
      return;

    if (navigation_handle->HasCommitted()) {
      has_committed_ = true;
      if (!navigation_handle->IsErrorPage()) {
        page_transition_ = navigation_handle->GetPageTransition();
        last_committed_url_ = navigation_handle->GetURL();
      } else {
        is_error_ = true;
      }
    } else {
      has_committed_ = false;
      is_error_ = true;
    }

    handle_ = nullptr;
  }

  bool has_committed() { return has_committed_; }

  bool is_error() { return is_error_; }

  const GURL& last_committed_url() { return last_committed_url_; }

  ui::PageTransition page_transition() { return page_transition_; }

 private:
  // A reference to the NavigationHandle so this class will track only
  // one navigation at a time. It is set at DidStartNavigation and cleared
  // at DidFinishNavigation before the NavigationHandle is destroyed.
  NavigationHandle* handle_;
  bool has_committed_;
  bool is_error_;
  ui::PageTransition page_transition_;
  GURL expected_url_;
  GURL last_committed_url_;
};

}  // namespace

class NavigationHandleImplBrowserTest : public ContentBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    SetupCrossSiteRedirector(embedded_test_server());
  }
};

// Ensure that PageTransition is properly set on the NavigationHandle.
IN_PROC_BROWSER_TEST_F(NavigationHandleImplBrowserTest, VerifyPageTransition) {
  {
    // Test browser initiated navigation, which should have a PageTransition as
    // if it came from the omnibox.
    GURL url(embedded_test_server()->GetURL("/title1.html"));
    NavigationHandleObserver observer(shell()->web_contents(), url);
    ui::PageTransition expected_transition = ui::PageTransitionFromInt(
        ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);

    NavigateToURL(shell(), url);
    EXPECT_TRUE(observer.has_committed());
    EXPECT_FALSE(observer.is_error());
    EXPECT_EQ(url, observer.last_committed_url());
    EXPECT_EQ(expected_transition, observer.page_transition());
  }

  {
    // Test navigating to a page with subframe. The subframe will have
    // PageTransition of type AUTO_SUBFRAME.
    GURL url(
        embedded_test_server()->GetURL("/frame_tree/page_with_one_frame.html"));
    NavigationHandleObserver observer(
        shell()->web_contents(),
        embedded_test_server()->GetURL("/cross-site/baz.com/title1.html"));
    ui::PageTransition expected_transition =
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_AUTO_SUBFRAME);

    NavigateToURL(shell(), url);
    EXPECT_TRUE(observer.has_committed());
    EXPECT_FALSE(observer.is_error());
    EXPECT_EQ(embedded_test_server()->GetURL("baz.com", "/title1.html"),
              observer.last_committed_url());
    EXPECT_EQ(expected_transition, observer.page_transition());
  }
}

}  // namespace content
