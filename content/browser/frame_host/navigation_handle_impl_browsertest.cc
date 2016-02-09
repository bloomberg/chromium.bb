// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/navigation_handle_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/page_transition_types.h"
#include "url/url_constants.h"

namespace content {

namespace {

class NavigationHandleObserver : public WebContentsObserver {
 public:
  NavigationHandleObserver(WebContents* web_contents, const GURL& expected_url)
      : WebContentsObserver(web_contents),
        handle_(nullptr),
        has_committed_(false),
        is_error_(false),
        is_main_frame_(false),
        is_parent_main_frame_(false),
        is_synchronous_(false),
        is_srcdoc_(false),
        was_redirected_(false),
        frame_tree_node_id_(-1),
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

    is_main_frame_ = navigation_handle->IsInMainFrame();
    is_parent_main_frame_ = navigation_handle->IsParentMainFrame();
    is_synchronous_ = navigation_handle->IsSynchronousNavigation();
    is_srcdoc_ = navigation_handle->IsSrcdoc();
    was_redirected_ = navigation_handle->WasServerRedirect();
    frame_tree_node_id_ = navigation_handle->GetFrameTreeNodeId();
  }

  void DidFinishNavigation(NavigationHandle* navigation_handle) override {
    if (navigation_handle != handle_)
      return;

    DCHECK_EQ(is_main_frame_, navigation_handle->IsInMainFrame());
    DCHECK_EQ(is_parent_main_frame_, navigation_handle->IsParentMainFrame());
    DCHECK_EQ(is_synchronous_, navigation_handle->IsSynchronousNavigation());
    DCHECK_EQ(is_srcdoc_, navigation_handle->IsSrcdoc());
    DCHECK_EQ(frame_tree_node_id_, navigation_handle->GetFrameTreeNodeId());

    was_redirected_ = navigation_handle->WasServerRedirect();

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
  bool is_main_frame() { return is_main_frame_; }
  bool is_parent_main_frame() { return is_parent_main_frame_; }
  bool is_synchronous() { return is_synchronous_; }
  bool is_srcdoc() { return is_srcdoc_; }
  bool was_redirected() { return was_redirected_; }
  int frame_tree_node_id() { return frame_tree_node_id_; }

  const GURL& last_committed_url() { return last_committed_url_; }

  ui::PageTransition page_transition() { return page_transition_; }

 private:
  // A reference to the NavigationHandle so this class will track only
  // one navigation at a time. It is set at DidStartNavigation and cleared
  // at DidFinishNavigation before the NavigationHandle is destroyed.
  NavigationHandle* handle_;
  bool has_committed_;
  bool is_error_;
  bool is_main_frame_;
  bool is_parent_main_frame_;
  bool is_synchronous_;
  bool is_srcdoc_;
  bool was_redirected_;
  int frame_tree_node_id_;
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

    EXPECT_TRUE(NavigateToURL(shell(), url));

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

    EXPECT_TRUE(NavigateToURL(shell(), url));

    EXPECT_TRUE(observer.has_committed());
    EXPECT_FALSE(observer.is_error());
    EXPECT_EQ(embedded_test_server()->GetURL("baz.com", "/title1.html"),
              observer.last_committed_url());
    EXPECT_EQ(expected_transition, observer.page_transition());
    EXPECT_FALSE(observer.is_main_frame());
  }
}

// Ensure that the following methods on NavigationHandle behave correctly:
// * IsInMainFrame
// * IsParentMainFrame
IN_PROC_BROWSER_TEST_F(NavigationHandleImplBrowserTest, VerifyFrameTree) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c))"));
  GURL b_url(embedded_test_server()->GetURL(
      "b.com", "/cross_site_iframe_factory.html?b(c())"));
  GURL c_url(embedded_test_server()->GetURL(
      "c.com", "/cross_site_iframe_factory.html?c()"));

  NavigationHandleObserver main_observer(shell()->web_contents(), main_url);
  NavigationHandleObserver b_observer(shell()->web_contents(), b_url);
  NavigationHandleObserver c_observer(shell()->web_contents(), c_url);

  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  // Verify the main frame.
  EXPECT_TRUE(main_observer.has_committed());
  EXPECT_FALSE(main_observer.is_error());
  EXPECT_EQ(main_url, main_observer.last_committed_url());
  EXPECT_TRUE(main_observer.is_main_frame());
  EXPECT_EQ(root->frame_tree_node_id(), main_observer.frame_tree_node_id());

  // Verify the b.com frame.
  EXPECT_TRUE(b_observer.has_committed());
  EXPECT_FALSE(b_observer.is_error());
  EXPECT_EQ(b_url, b_observer.last_committed_url());
  EXPECT_FALSE(b_observer.is_main_frame());
  EXPECT_TRUE(b_observer.is_parent_main_frame());
  EXPECT_EQ(root->child_at(0)->frame_tree_node_id(),
            b_observer.frame_tree_node_id());

  // Verify the c.com frame.
  EXPECT_TRUE(c_observer.has_committed());
  EXPECT_FALSE(c_observer.is_error());
  EXPECT_EQ(c_url, c_observer.last_committed_url());
  EXPECT_FALSE(c_observer.is_main_frame());
  EXPECT_FALSE(c_observer.is_parent_main_frame());
  EXPECT_EQ(root->child_at(0)->child_at(0)->frame_tree_node_id(),
            c_observer.frame_tree_node_id());
}

// Ensure that the WasRedirected() method on NavigationHandle behaves correctly.
IN_PROC_BROWSER_TEST_F(NavigationHandleImplBrowserTest, VerifyRedirect) {
  {
    GURL url(embedded_test_server()->GetURL("/title1.html"));
    NavigationHandleObserver observer(shell()->web_contents(), url);

    EXPECT_TRUE(NavigateToURL(shell(), url));

    EXPECT_TRUE(observer.has_committed());
    EXPECT_FALSE(observer.is_error());
    EXPECT_FALSE(observer.was_redirected());
  }

  {
    GURL url(embedded_test_server()->GetURL("/cross-site/baz.com/title1.html"));
    NavigationHandleObserver observer(shell()->web_contents(), url);

    NavigateToURL(shell(), url);

    EXPECT_TRUE(observer.has_committed());
    EXPECT_FALSE(observer.is_error());
    EXPECT_TRUE(observer.was_redirected());
  }
}

// Ensure that the IsSrcdoc() method on NavigationHandle behaves correctly.
IN_PROC_BROWSER_TEST_F(NavigationHandleImplBrowserTest, VerifySrcdoc) {
  GURL url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_srcdoc_frame.html"));
  NavigationHandleObserver observer(shell()->web_contents(),
                                    GURL(url::kAboutBlankURL));

  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_TRUE(observer.has_committed());
  EXPECT_FALSE(observer.is_error());
  EXPECT_TRUE(observer.is_srcdoc());
}

// Ensure that the IsSynchronousNavigation() method on NavigationHandle behaves
// correctly.
IN_PROC_BROWSER_TEST_F(NavigationHandleImplBrowserTest, VerifySynchronous) {
  GURL url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a())"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  NavigationHandleObserver observer(
      shell()->web_contents(), embedded_test_server()->GetURL("a.com", "/bar"));
  EXPECT_TRUE(ExecuteScript(root->child_at(0)->current_frame_host(),
                            "window.history.pushState({}, '', 'bar');"));

  EXPECT_TRUE(observer.has_committed());
  EXPECT_FALSE(observer.is_error());
  EXPECT_TRUE(observer.is_synchronous());
}

}  // namespace content
