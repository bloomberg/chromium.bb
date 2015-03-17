// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/navigation_controller_impl.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace content {

class NavigationControllerBrowserTest : public ContentBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  }
};

IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest, LoadDataWithBaseURL) {
  const GURL base_url("http://baseurl");
  const GURL history_url("http://historyurl");
  const std::string data = "<html><body>foo</body></html>";

  const NavigationController& controller =
      shell()->web_contents()->GetController();
  // Load data. Blocks until it is done.
  content::LoadDataWithBaseURL(shell(), history_url, data, base_url);

  // We should use history_url instead of the base_url as the original url of
  // this navigation entry, because base_url is only used for resolving relative
  // paths in the data, or enforcing same origin policy.
  EXPECT_EQ(controller.GetVisibleEntry()->GetOriginalRequestURL(), history_url);
}

// The renderer uses the position in the history list as a clue to whether a
// navigation is stale. In the case where the entry limit is reached and the
// history list is pruned, make sure that there is no mismatch that would cause
// it to start incorrectly rejecting navigations as stale. See
// http://crbug.com/89798.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       DontIgnoreBackAfterNavEntryLimit) {
  NavigationController& controller =
      shell()->web_contents()->GetController();

  const int kMaxEntryCount =
      static_cast<int>(NavigationControllerImpl::max_entry_count());

  // Load up to the max count, all entries should be there.
  for (int url_index = 0; url_index < kMaxEntryCount; ++url_index) {
    GURL url(base::StringPrintf("data:text/html,page%d", url_index));
    EXPECT_TRUE(NavigateToURL(shell(), url));
  }

  EXPECT_EQ(controller.GetEntryCount(), kMaxEntryCount);

  // Navigate twice more more.
  for (int url_index = kMaxEntryCount;
       url_index < kMaxEntryCount + 2; ++url_index) {
    GURL url(base::StringPrintf("data:text/html,page%d", url_index));
    EXPECT_TRUE(NavigateToURL(shell(), url));
  }

  // We expect page0 and page1 to be gone.
  EXPECT_EQ(kMaxEntryCount, controller.GetEntryCount());
  EXPECT_EQ(GURL("data:text/html,page2"),
            controller.GetEntryAtIndex(0)->GetURL());

  // Now try to go back. This should not hang.
  ASSERT_TRUE(controller.CanGoBack());
  controller.GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // This should have successfully gone back.
  EXPECT_EQ(GURL(base::StringPrintf("data:text/html,page%d", kMaxEntryCount)),
            controller.GetLastCommittedEntry()->GetURL());
}

namespace {

int RendererHistoryLength(Shell* shell) {
  int value = 0;
  EXPECT_TRUE(ExecuteScriptAndExtractInt(
      shell->web_contents(),
      "domAutomationController.send(history.length)",
      &value));
  return value;
}

// Similar to the ones from content_browser_test_utils.
bool NavigateToURLAndReplace(Shell* shell, const GURL& url) {
  WebContents* web_contents = shell->web_contents();
  WaitForLoadStop(web_contents);
  TestNavigationObserver same_tab_observer(web_contents, 1);
  NavigationController::LoadURLParams params(url);
  params.should_replace_current_entry = true;
  web_contents->GetController().LoadURLWithParams(params);
  web_contents->Focus();
  same_tab_observer.Wait();
  if (!IsLastCommittedEntryOfPageType(web_contents, PAGE_TYPE_NORMAL))
    return false;
  return web_contents->GetLastCommittedURL() == url;
}

}  // namespace

// When loading a new page to replace an old page in the history list, make sure
// that the browser and renderer agree, and that both get it right.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       CorrectLengthWithCurrentItemReplacement) {
  NavigationController& controller =
      shell()->web_contents()->GetController();

  EXPECT_TRUE(NavigateToURL(shell(), GURL("data:text/html,page1")));
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(1, RendererHistoryLength(shell()));

  EXPECT_TRUE(NavigateToURLAndReplace(shell(), GURL("data:text/html,page2")));
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(1, RendererHistoryLength(shell()));

  // Note that there's no way to access the renderer's notion of the history
  // offset via JavaScript. Checking just the history length, though, is enough;
  // if the replacement failed, there would be a new history entry and thus an
  // incorrect length.
}

// When spawning a new page from a WebUI page, make sure that the browser and
// renderer agree about the length of the history list, and that both get it
// right.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       CorrectLengthWithNewTabNavigatingFromWebUI) {
  GURL web_ui_page(std::string(kChromeUIScheme) + "://" +
                   std::string(kChromeUIGpuHost));
  EXPECT_TRUE(NavigateToURL(shell(), web_ui_page));
  EXPECT_EQ(BINDINGS_POLICY_WEB_UI,
      shell()->web_contents()->GetRenderViewHost()->GetEnabledBindings());

  ShellAddedObserver observer;
  std::string page_url = embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html").spec();
  EXPECT_TRUE(ExecuteScript(shell()->web_contents(),
                            "window.open('" + page_url + "', '_blank')"));
  Shell* shell2 = observer.GetShell();
  EXPECT_TRUE(WaitForLoadStop(shell2->web_contents()));

  EXPECT_EQ(1, shell2->web_contents()->GetController().GetEntryCount());
  EXPECT_EQ(1, RendererHistoryLength(shell2));

  // Again, as above, there's no way to access the renderer's notion of the
  // history offset via JavaScript. Checking just the history length, again,
  // will have to suffice.
}

namespace {

class NoNavigationsObserver : public WebContentsObserver {
 public:
  // Observes navigation for the specified |web_contents|.
  explicit NoNavigationsObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

 private:
  void DidNavigateAnyFrame(RenderFrameHost* render_frame_host,
                           const LoadCommittedDetails& details,
                           const FrameNavigateParams& params) override {
    FAIL() << "No navigations should occur";
  }
};

}  // namespace

// Some pages create a popup, then write an iframe into it. This causes a
// subframe navigation without having any committed entry. Such navigations
// just get thrown on the ground, but we shouldn't crash.
//
// This test actually hits NAVIGATION_TYPE_NAV_IGNORE three times. Two of them,
// the initial window.open() and the iframe creation, don't try to create
// navigation entries, and the third, the new navigation, tries to.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest, SubframeOnEmptyPage) {
  NavigateToURL(shell(), GURL(url::kAboutBlankURL));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell()->web_contents())->
          GetFrameTree()->root();

  // Pop open a new window.
  ShellAddedObserver new_shell_observer;
  std::string script = "window.open()";
  EXPECT_TRUE(content::ExecuteScript(root->current_frame_host(), script));
  Shell* new_shell = new_shell_observer.GetShell();
  ASSERT_NE(new_shell->web_contents(), shell()->web_contents());
  FrameTreeNode* new_root =
      static_cast<WebContentsImpl*>(new_shell->web_contents())->
          GetFrameTree()->root();

  // Make a new iframe in it.
  NoNavigationsObserver observer(new_shell->web_contents());
  script = "var iframe = document.createElement('iframe');"
           "iframe.src = 'data:text/html,<p>some page</p>';"
           "document.body.appendChild(iframe);";
  EXPECT_TRUE(content::ExecuteScript(new_root->current_frame_host(), script));
  // The success check is of the last-committed entry, and there is none.
  WaitForLoadStopWithoutSuccessCheck(new_shell->web_contents());

  ASSERT_EQ(1U, new_root->child_count());
  ASSERT_NE(nullptr, new_root->child_at(0));

  // Navigate it.
  GURL frame_url = embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_2.html");
  script = "location.assign('" + frame_url.spec() + "')";
  EXPECT_TRUE(content::ExecuteScript(
      new_root->child_at(0)->current_frame_host(), script));

  // Success is not crashing, and not navigating.
  EXPECT_EQ(nullptr,
            new_shell->web_contents()->GetController().GetLastCommittedEntry());
}

namespace {

class FrameNavigateParamsCapturer : public WebContentsObserver {
 public:
  // Observes navigation for the specified |node|.
  explicit FrameNavigateParamsCapturer(FrameTreeNode* node)
      : WebContentsObserver(
            node->current_frame_host()->delegate()->GetAsWebContents()),
        frame_tree_node_id_(node->frame_tree_node_id()),
        navigations_remaining_(1),
        message_loop_runner_(new MessageLoopRunner) {}

  void set_navigations_remaining(int count) {
    navigations_remaining_ = count;
  }

  void Wait() {
    message_loop_runner_->Run();
  }

  const FrameNavigateParams& params() const {
    EXPECT_EQ(1U, params_.size());
    return params_[0];
  }

  const std::vector<FrameNavigateParams>& all_params() const {
    return params_;
  }

  const LoadCommittedDetails& details() const {
    EXPECT_EQ(1U, details_.size());
    return details_[0];
  }

  const std::vector<LoadCommittedDetails>& all_details() const {
    return details_;
  }

 private:
  void DidNavigateAnyFrame(RenderFrameHost* render_frame_host,
                           const LoadCommittedDetails& details,
                           const FrameNavigateParams& params) override {
    RenderFrameHostImpl* rfh =
        static_cast<RenderFrameHostImpl*>(render_frame_host);
    if (rfh->frame_tree_node()->frame_tree_node_id() != frame_tree_node_id_)
      return;

    --navigations_remaining_;
    params_.push_back(params);
    details_.push_back(details);
    if (!web_contents()->IsLoading() && !navigations_remaining_)
      message_loop_runner_->Quit();
  }

  void DidStopLoading(RenderViewHost* render_view_host) override {
    if (!navigations_remaining_)
      message_loop_runner_->Quit();
  }

  // The id of the FrameTreeNode whose navigations to observe.
  int frame_tree_node_id_;

  // How many navigations remain to capture.
  int navigations_remaining_;

  // The params of the navigations.
  std::vector<FrameNavigateParams> params_;

  // The details of the navigations.
  std::vector<LoadCommittedDetails> details_;

  // The MessageLoopRunner used to spin the message loop.
  scoped_refptr<MessageLoopRunner> message_loop_runner_;
};

class LoadCommittedCapturer : public WebContentsObserver {
 public:
  // Observes the load commit for the specified |node|.
  explicit LoadCommittedCapturer(FrameTreeNode* node)
      : WebContentsObserver(
            node->current_frame_host()->delegate()->GetAsWebContents()),
        frame_tree_node_id_(node->frame_tree_node_id()),
        message_loop_runner_(new MessageLoopRunner) {}

  // Observes the load commit for the next created frame in the specified
  // |web_contents|.
  explicit LoadCommittedCapturer(WebContents* web_contents)
      : WebContentsObserver(web_contents),
        frame_tree_node_id_(0),
        message_loop_runner_(new MessageLoopRunner) {}

  void Wait() {
    message_loop_runner_->Run();
  }

  ui::PageTransition transition_type() const {
    return transition_type_;
  }

 private:
  void RenderFrameCreated(RenderFrameHost* render_frame_host) override {
    // If this object was created with a specified tree frame node, there
    // shouldn't be any frames being created.
    DCHECK_EQ(0, frame_tree_node_id_);
    RenderFrameHostImpl* rfh =
        static_cast<RenderFrameHostImpl*>(render_frame_host);
    frame_tree_node_id_ = rfh->frame_tree_node()->frame_tree_node_id();
  }

  void DidCommitProvisionalLoadForFrame(
      RenderFrameHost* render_frame_host,
      const GURL& url,
      ui::PageTransition transition_type) override {
    DCHECK_NE(0, frame_tree_node_id_);
    RenderFrameHostImpl* rfh =
        static_cast<RenderFrameHostImpl*>(render_frame_host);
    if (rfh->frame_tree_node()->frame_tree_node_id() != frame_tree_node_id_)
      return;

    transition_type_ = transition_type;
    if (!web_contents()->IsLoading())
      message_loop_runner_->Quit();
  }

  void DidStopLoading(RenderViewHost* render_view_host) override {
    message_loop_runner_->Quit();
  }

  // The id of the FrameTreeNode whose navigations to observe.
  int frame_tree_node_id_;

  // The transition_type of the last navigation.
  ui::PageTransition transition_type_;

  // The MessageLoopRunner used to spin the message loop.
  scoped_refptr<MessageLoopRunner> message_loop_runner_;
};

}  // namespace

// Various tests for navigation type classifications. TODO(avi): It's rather
// bogus that the same info is in two different enums; http://crbug.com/453555.

// Verify that navigations for NAVIGATION_TYPE_NEW_PAGE are correctly
// classified.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       NavigationTypeClassification_NewPage) {
  NavigateToURL(shell(), GURL(url::kAboutBlankURL));

  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell()->web_contents())->
          GetFrameTree()->root();

  {
    // Simple load.
    FrameNavigateParamsCapturer capturer(root);
    GURL frame_url(embedded_test_server()->GetURL(
        "/navigation_controller/page_with_links.html"));
    NavigateFrameToURL(root, frame_url);
    capturer.Wait();
    // TODO(avi,creis): Why is this (and quite a few others below) a "link"
    // transition? Lots of these transitions should be cleaned up.
    EXPECT_EQ(ui::PAGE_TRANSITION_LINK, capturer.params().transition);
    EXPECT_EQ(NAVIGATION_TYPE_NEW_PAGE, capturer.details().type);
  }

  {
    // Load via a fragment link click.
    FrameNavigateParamsCapturer capturer(root);
    std::string script = "document.getElementById('fraglink').click()";
    EXPECT_TRUE(content::ExecuteScript(root->current_frame_host(), script));
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_LINK, capturer.params().transition);
    EXPECT_EQ(NAVIGATION_TYPE_NEW_PAGE, capturer.details().type);
  }

  {
    // Load via link click.
    FrameNavigateParamsCapturer capturer(root);
    std::string script = "document.getElementById('thelink').click()";
    EXPECT_TRUE(content::ExecuteScript(root->current_frame_host(), script));
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_LINK, capturer.params().transition);
    EXPECT_EQ(NAVIGATION_TYPE_NEW_PAGE, capturer.details().type);
  }

  {
    // location.assign().
    FrameNavigateParamsCapturer capturer(root);
    GURL frame_url(embedded_test_server()->GetURL(
        "/navigation_controller/simple_page_2.html"));
    std::string script = "location.assign('" + frame_url.spec() + "')";
    EXPECT_TRUE(content::ExecuteScript(root->current_frame_host(), script));
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_CLIENT_REDIRECT,
              capturer.params().transition);
    EXPECT_EQ(NAVIGATION_TYPE_NEW_PAGE, capturer.details().type);
  }

  {
    // history.pushState().
    FrameNavigateParamsCapturer capturer(root);
    std::string script =
        "history.pushState({}, 'page 1', 'simple_page_1.html')";
    EXPECT_TRUE(content::ExecuteScript(root->current_frame_host(), script));
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_CLIENT_REDIRECT,
              capturer.params().transition);
    EXPECT_EQ(NAVIGATION_TYPE_NEW_PAGE, capturer.details().type);
  }
}

// Verify that navigations for NAVIGATION_TYPE_EXISTING_PAGE are correctly
// classified.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       NavigationTypeClassification_ExistingPage) {
  GURL url1(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  NavigateToURL(shell(), url1);
  GURL url2(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_2.html"));
  NavigateToURL(shell(), url2);

  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell()->web_contents())->
          GetFrameTree()->root();

  {
    // Back from the browser side.
    FrameNavigateParamsCapturer capturer(root);
    shell()->web_contents()->GetController().GoBack();
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_TYPED
              | ui::PAGE_TRANSITION_FORWARD_BACK
              | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR,
              capturer.params().transition);
    EXPECT_EQ(NAVIGATION_TYPE_EXISTING_PAGE, capturer.details().type);
  }

  {
    // Forward from the browser side.
    FrameNavigateParamsCapturer capturer(root);
    shell()->web_contents()->GetController().GoForward();
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_TYPED
              | ui::PAGE_TRANSITION_FORWARD_BACK
              | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR,
              capturer.params().transition);
    EXPECT_EQ(NAVIGATION_TYPE_EXISTING_PAGE, capturer.details().type);
  }

  {
    // Back from the renderer side.
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_TRUE(content::ExecuteScript(root->current_frame_host(),
                                       "history.back()"));
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_TYPED
              | ui::PAGE_TRANSITION_FORWARD_BACK
              | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR,
              capturer.params().transition);
    EXPECT_EQ(NAVIGATION_TYPE_EXISTING_PAGE, capturer.details().type);
  }

  {
    // Forward from the renderer side.
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_TRUE(content::ExecuteScript(root->current_frame_host(),
                                       "history.forward()"));
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_TYPED
              | ui::PAGE_TRANSITION_FORWARD_BACK
              | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR,
              capturer.params().transition);
    EXPECT_EQ(NAVIGATION_TYPE_EXISTING_PAGE, capturer.details().type);
  }

  {
    // Back from the renderer side via history.go().
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_TRUE(content::ExecuteScript(root->current_frame_host(),
                                       "history.go(-1)"));
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_TYPED
              | ui::PAGE_TRANSITION_FORWARD_BACK
              | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR,
              capturer.params().transition);
    EXPECT_EQ(NAVIGATION_TYPE_EXISTING_PAGE, capturer.details().type);
  }

  {
    // Forward from the renderer side via history.go().
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_TRUE(content::ExecuteScript(root->current_frame_host(),
                                       "history.go(1)"));
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_TYPED
              | ui::PAGE_TRANSITION_FORWARD_BACK
              | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR,
              capturer.params().transition);
    EXPECT_EQ(NAVIGATION_TYPE_EXISTING_PAGE, capturer.details().type);
  }

  {
    // Reload from the browser side.
    FrameNavigateParamsCapturer capturer(root);
    shell()->web_contents()->GetController().Reload(false);
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_RELOAD, capturer.params().transition);
    EXPECT_EQ(NAVIGATION_TYPE_EXISTING_PAGE, capturer.details().type);
  }

  {
    // Reload from the renderer side.
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_TRUE(content::ExecuteScript(root->current_frame_host(),
                                       "location.reload()"));
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_CLIENT_REDIRECT,
              capturer.params().transition);
    EXPECT_EQ(NAVIGATION_TYPE_EXISTING_PAGE, capturer.details().type);
  }

  {
    // location.replace().
    FrameNavigateParamsCapturer capturer(root);
    GURL frame_url(embedded_test_server()->GetURL(
        "/navigation_controller/simple_page_1.html"));
    std::string script = "location.replace('" + frame_url.spec() + "')";
    EXPECT_TRUE(content::ExecuteScript(root->current_frame_host(), script));
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_CLIENT_REDIRECT,
              capturer.params().transition);
    EXPECT_EQ(NAVIGATION_TYPE_EXISTING_PAGE, capturer.details().type);
  }
}

// Verify that navigations for NAVIGATION_TYPE_SAME_PAGE are correctly
// classified.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       NavigationTypeClassification_SamePage) {
  GURL url1(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  NavigateToURL(shell(), url1);

  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell()->web_contents())->
          GetFrameTree()->root();

  {
    // Simple load.
    FrameNavigateParamsCapturer capturer(root);
    GURL frame_url(embedded_test_server()->GetURL(
        "/navigation_controller/simple_page_1.html"));
    NavigateFrameToURL(root, frame_url);
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_LINK, capturer.params().transition);
    EXPECT_EQ(NAVIGATION_TYPE_SAME_PAGE, capturer.details().type);
  }
}

// Verify that navigations for NAVIGATION_TYPE_IN_PAGE are correctly
// classified.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       NavigationTypeClassification_InPage) {
  GURL url1(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  NavigateToURL(shell(), url1);

  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell()->web_contents())->
          GetFrameTree()->root();

  {
    // history.replaceState().
    FrameNavigateParamsCapturer capturer(root);
    std::string script =
        "history.replaceState({}, 'page 1', 'simple_page_2.html')";
    EXPECT_TRUE(content::ExecuteScript(root->current_frame_host(), script));
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_LINK, capturer.params().transition);
    EXPECT_EQ(NAVIGATION_TYPE_IN_PAGE, capturer.details().type);
  }

  // Back and forward across a fragment navigation.

  GURL url2(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_links.html"));
  NavigateToURL(shell(), url2);
  std::string script = "document.getElementById('fraglink').click()";
  EXPECT_TRUE(content::ExecuteScript(root->current_frame_host(), script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  {
    // Back.
    FrameNavigateParamsCapturer capturer(root);
    shell()->web_contents()->GetController().GoBack();
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_TYPED
              | ui::PAGE_TRANSITION_FORWARD_BACK
              | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR,
              capturer.params().transition);
    EXPECT_EQ(NAVIGATION_TYPE_IN_PAGE, capturer.details().type);
  }

  {
    // Forward.
    FrameNavigateParamsCapturer capturer(root);
    shell()->web_contents()->GetController().GoForward();
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_FORWARD_BACK,
              capturer.params().transition);
    EXPECT_EQ(NAVIGATION_TYPE_IN_PAGE, capturer.details().type);
  }

  // Back and forward across a pushState-created navigation.

  NavigateToURL(shell(), url1);
  script = "history.pushState({}, 'page 2', 'simple_page_2.html')";
  EXPECT_TRUE(content::ExecuteScript(root->current_frame_host(), script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  {
    // Back.
    FrameNavigateParamsCapturer capturer(root);
    shell()->web_contents()->GetController().GoBack();
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_TYPED
              | ui::PAGE_TRANSITION_FORWARD_BACK
              | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR,
              capturer.params().transition);
    EXPECT_EQ(NAVIGATION_TYPE_IN_PAGE, capturer.details().type);
  }

  {
    // Forward.
    FrameNavigateParamsCapturer capturer(root);
    shell()->web_contents()->GetController().GoForward();
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_FORWARD_BACK,
              capturer.params().transition);
    EXPECT_EQ(NAVIGATION_TYPE_IN_PAGE, capturer.details().type);
  }
}

// Verify that navigations for NAVIGATION_TYPE_NEW_SUBFRAME and
// NAVIGATION_TYPE_AUTO_SUBFRAME are properly classified.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       NavigationTypeClassification_NewAndAutoSubframe) {
  GURL main_url(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_iframe.html"));
  NavigateToURL(shell(), main_url);

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell()->web_contents())->
          GetFrameTree()->root();

  ASSERT_EQ(1U, root->child_count());
  ASSERT_NE(nullptr, root->child_at(0));

  {
    // Simple load.
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    GURL frame_url(embedded_test_server()->GetURL(
        "/navigation_controller/simple_page_1.html"));
    NavigateFrameToURL(root->child_at(0), frame_url);
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_MANUAL_SUBFRAME,
              capturer.params().transition);
    EXPECT_EQ(NAVIGATION_TYPE_NEW_SUBFRAME, capturer.details().type);
  }

  {
    // Back.
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    shell()->web_contents()->GetController().GoBack();
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_AUTO_SUBFRAME, capturer.params().transition);
    EXPECT_EQ(NAVIGATION_TYPE_AUTO_SUBFRAME, capturer.details().type);
  }

  {
    // Forward.
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    shell()->web_contents()->GetController().GoForward();
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_AUTO_SUBFRAME, capturer.params().transition);
    EXPECT_EQ(NAVIGATION_TYPE_AUTO_SUBFRAME, capturer.details().type);
  }

  {
    // Simple load.
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    GURL frame_url(embedded_test_server()->GetURL(
        "/navigation_controller/page_with_links.html"));
    NavigateFrameToURL(root->child_at(0), frame_url);
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_MANUAL_SUBFRAME,
              capturer.params().transition);
    EXPECT_EQ(NAVIGATION_TYPE_NEW_SUBFRAME, capturer.details().type);
  }

  {
    // Load via a fragment link click.
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    std::string script = "document.getElementById('fraglink').click()";
    EXPECT_TRUE(content::ExecuteScript(root->child_at(0)->current_frame_host(),
                                       script));
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_MANUAL_SUBFRAME,
              capturer.params().transition);
    EXPECT_EQ(NAVIGATION_TYPE_NEW_SUBFRAME, capturer.details().type);
  }

  {
    // location.assign().
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    GURL frame_url(embedded_test_server()->GetURL(
        "/navigation_controller/simple_page_1.html"));
    std::string script = "location.assign('" + frame_url.spec() + "')";
    EXPECT_TRUE(content::ExecuteScript(root->child_at(0)->current_frame_host(),
                                       script));
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_MANUAL_SUBFRAME,
              capturer.params().transition);
    EXPECT_EQ(NAVIGATION_TYPE_NEW_SUBFRAME, capturer.details().type);
  }

  {
    // location.replace().
    LoadCommittedCapturer capturer(root->child_at(0));
    GURL frame_url(embedded_test_server()->GetURL(
        "/navigation_controller/simple_page_2.html"));
    std::string script = "location.replace('" + frame_url.spec() + "')";
    EXPECT_TRUE(content::ExecuteScript(root->child_at(0)->current_frame_host(),
                                       script));
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_AUTO_SUBFRAME, capturer.transition_type());
  }

  {
    // history.pushState().
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    std::string script =
        "history.pushState({}, 'page 1', 'simple_page_1.html')";
    EXPECT_TRUE(content::ExecuteScript(root->child_at(0)->current_frame_host(),
                                       script));
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_MANUAL_SUBFRAME,
              capturer.params().transition);
    EXPECT_EQ(NAVIGATION_TYPE_NEW_SUBFRAME, capturer.details().type);
  }

  {
    // history.replaceState().
    LoadCommittedCapturer capturer(root->child_at(0));
    std::string script =
        "history.replaceState({}, 'page 2', 'simple_page_2.html')";
    EXPECT_TRUE(content::ExecuteScript(root->child_at(0)->current_frame_host(),
                                       script));
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_AUTO_SUBFRAME, capturer.transition_type());
  }

  {
    // Reload.
    LoadCommittedCapturer capturer(root->child_at(0));
    EXPECT_TRUE(content::ExecuteScript(root->child_at(0)->current_frame_host(),
                                       "location.reload()"));
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_AUTO_SUBFRAME, capturer.transition_type());
  }

  {
    // Create an iframe.
    LoadCommittedCapturer capturer(shell()->web_contents());
    GURL frame_url(embedded_test_server()->GetURL(
        "/navigation_controller/simple_page_1.html"));
    std::string script = "var iframe = document.createElement('iframe');"
                         "iframe.src = '" + frame_url.spec() + "';"
                         "document.body.appendChild(iframe);";
    EXPECT_TRUE(content::ExecuteScript(root->current_frame_host(), script));
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_AUTO_SUBFRAME, capturer.transition_type());
  }
}

// Verify that navigations caused by client-side redirects are correctly
// classified.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       NavigationTypeClassification_ClientSideRedirect) {
  NavigateToURL(shell(), GURL(url::kAboutBlankURL));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell()->web_contents())->
          GetFrameTree()->root();

  {
    // Load the redirecting page.
    FrameNavigateParamsCapturer capturer(root);
    capturer.set_navigations_remaining(2);
    GURL frame_url(embedded_test_server()->GetURL(
        "/navigation_controller/client_redirect.html"));
    NavigateFrameToURL(root, frame_url);
    capturer.Wait();

    std::vector<FrameNavigateParams> params = capturer.all_params();
    std::vector<LoadCommittedDetails> details = capturer.all_details();
    ASSERT_EQ(2U, params.size());
    ASSERT_EQ(2U, details.size());
    EXPECT_EQ(ui::PAGE_TRANSITION_LINK, params[0].transition);
    EXPECT_EQ(NAVIGATION_TYPE_NEW_PAGE, details[0].type);
    EXPECT_EQ(ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_CLIENT_REDIRECT,
              params[1].transition);
    EXPECT_EQ(NAVIGATION_TYPE_EXISTING_PAGE, details[1].type);
  }
}

}  // namespace content
