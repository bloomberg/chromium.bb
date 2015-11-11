// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/frame_host/frame_navigation_entry.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/navigation_controller_impl.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/site_isolation_policy.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_controller.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"
#include "content/public/browser/resource_throttle.h"
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
#include "net/test/url_request/url_request_failed_job.h"

namespace content {

class NavigationControllerBrowserTest : public ContentBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

// Ensure that tests can navigate subframes cross-site in both default mode and
// --site-per-process, but that they only go cross-process in the latter.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest, LoadCrossSiteSubframe) {
  // Load a main frame with a subframe.
  GURL main_url(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_iframe.html"));
  NavigateToURL(shell(), main_url);
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  ASSERT_EQ(1U, root->child_count());
  ASSERT_NE(nullptr, root->child_at(0));

  // Use NavigateFrameToURL to go cross-site in the subframe.
  GURL foo_url(embedded_test_server()->GetURL(
      "foo.com", "/navigation_controller/simple_page_1.html"));
  NavigateFrameToURL(root->child_at(0), foo_url);
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // We should only have swapped processes in --site-per-process.
  bool cross_process = root->current_frame_host()->GetProcess() !=
                       root->child_at(0)->current_frame_host()->GetProcess();
  EXPECT_EQ(AreAllSitesIsolatedForTesting(), cross_process);
}

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

IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest, UniqueIDs) {
  const NavigationControllerImpl& controller =
      static_cast<const NavigationControllerImpl&>(
          shell()->web_contents()->GetController());

  GURL main_url(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_link_to_load_iframe.html"));
  NavigateToURL(shell(), main_url);
  ASSERT_EQ(1, controller.GetEntryCount());

  // Use JavaScript to click the link and load the iframe.
  std::string script = "document.getElementById('link').click()";
  EXPECT_TRUE(content::ExecuteScript(shell()->web_contents(), script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ASSERT_EQ(2, controller.GetEntryCount());

  // Unique IDs should... um... be unique.
  ASSERT_NE(controller.GetEntryAtIndex(0)->GetUniqueID(),
            controller.GetEntryAtIndex(1)->GetUniqueID());
}

// Ensures that RenderFrameHosts end up with the correct nav_entry_id() after
// navigations.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest, UniqueIDsOnFrames) {
  NavigationController& controller = shell()->web_contents()->GetController();

  // Load a main frame with an about:blank subframe.
  GURL main_url(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_iframe.html"));
  NavigateToURL(shell(), main_url);
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  ASSERT_EQ(1U, root->child_count());
  ASSERT_NE(nullptr, root->child_at(0));

  // The main frame's nav_entry_id should match the last committed entry.
  int unique_id = controller.GetLastCommittedEntry()->GetUniqueID();
  EXPECT_EQ(unique_id, root->current_frame_host()->nav_entry_id());

  // The about:blank iframe should have inherited the same nav_entry_id.
  EXPECT_EQ(unique_id, root->child_at(0)->current_frame_host()->nav_entry_id());

  // Use NavigateFrameToURL to go cross-site in the subframe.
  GURL foo_url(embedded_test_server()->GetURL(
      "foo.com", "/navigation_controller/simple_page_1.html"));
  NavigateFrameToURL(root->child_at(0), foo_url);
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // The unique ID should have stayed the same for the auto-subframe navigation,
  // since the new page replaces the initial about:blank page in the subframe.
  EXPECT_EQ(unique_id, controller.GetLastCommittedEntry()->GetUniqueID());
  EXPECT_EQ(unique_id, root->current_frame_host()->nav_entry_id());
  EXPECT_EQ(unique_id, root->child_at(0)->current_frame_host()->nav_entry_id());

  // Navigating in the subframe again should create a new entry.
  GURL foo_url2(embedded_test_server()->GetURL(
      "foo.com", "/navigation_controller/simple_page_2.html"));
  NavigateFrameToURL(root->child_at(0), foo_url2);
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  int unique_id2 = controller.GetLastCommittedEntry()->GetUniqueID();
  EXPECT_NE(unique_id, unique_id2);

  // The unique ID should have updated for the current RenderFrameHost in both
  // frames, not just the subframe.
  EXPECT_EQ(unique_id2, root->current_frame_host()->nav_entry_id());
  EXPECT_EQ(unique_id2,
            root->child_at(0)->current_frame_host()->nav_entry_id());
}

// This test used to make sure that a scheme used to prevent spoofs didn't ever
// interfere with navigations. We switched to a different scheme, so now this is
// just a test to make sure we can still navigate once we prune the history
// list.
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

  EXPECT_TRUE(NavigateToURLAndReplace(shell(), GURL("data:text/html,page1a")));
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(1, RendererHistoryLength(shell()));

  // Now create two more entries and go back, to test replacing an entry without
  // pruning the forward history.
  EXPECT_TRUE(NavigateToURL(shell(), GURL("data:text/html,page2")));
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(2, RendererHistoryLength(shell()));

  EXPECT_TRUE(NavigateToURL(shell(), GURL("data:text/html,page3")));
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(3, RendererHistoryLength(shell()));

  controller.GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  controller.GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_TRUE(controller.CanGoForward());

  EXPECT_TRUE(NavigateToURLAndReplace(shell(), GURL("data:text/html,page1b")));
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(3, RendererHistoryLength(shell()));
  EXPECT_TRUE(controller.CanGoForward());

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
        wait_for_load_(true),
        message_loop_runner_(new MessageLoopRunner) {}

  void set_navigations_remaining(int count) {
    navigations_remaining_ = count;
  }

  void set_wait_for_load(bool ignore) {
    wait_for_load_ = ignore;
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
    if (!navigations_remaining_ &&
        (!web_contents()->IsLoading() || !wait_for_load_))
      message_loop_runner_->Quit();
  }

  void DidStopLoading() override {
    if (!navigations_remaining_)
      message_loop_runner_->Quit();
  }

  // The id of the FrameTreeNode whose navigations to observe.
  int frame_tree_node_id_;

  // How many navigations remain to capture.
  int navigations_remaining_;

  // Whether to also wait for the load to complete.
  bool wait_for_load_;

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
    RenderFrameHostImpl* rfh =
        static_cast<RenderFrameHostImpl*>(render_frame_host);

    // Don't pay attention to swapped out RenderFrameHosts in the main frame.
    // TODO(nasko): Remove once swappedout:// is gone.
    // See https://crbug.com/357747.
    if (!RenderFrameHostImpl::IsRFHStateActive(rfh->rfh_state())) {
      DLOG(INFO) << "Skipping swapped out RFH: "
                 << rfh->GetSiteInstance()->GetSiteURL();
      return;
    }

    // If this object was not created with a specified frame tree node, then use
    // the first created active RenderFrameHost.  Once a node is selected, there
    // shouldn't be any other frames being created.
    int frame_tree_node_id = rfh->frame_tree_node()->frame_tree_node_id();
    DCHECK(frame_tree_node_id_ == 0 ||
           frame_tree_node_id_ == frame_tree_node_id);
    frame_tree_node_id_ = frame_tree_node_id;
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

  void DidStopLoading() override { message_loop_runner_->Quit(); }

  // The id of the FrameTreeNode whose navigations to observe.
  int frame_tree_node_id_;

  // The transition_type of the last navigation.
  ui::PageTransition transition_type_;

  // The MessageLoopRunner used to spin the message loop.
  scoped_refptr<MessageLoopRunner> message_loop_runner_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       ErrorPageReplacement) {
  NavigationController& controller = shell()->web_contents()->GetController();
  GURL error_url(
      net::URLRequestFailedJob::GetMockHttpUrl(net::ERR_CONNECTION_RESET));
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&net::URLRequestFailedJob::AddUrlHandler));

  NavigateToURL(shell(), GURL(url::kAboutBlankURL));
  EXPECT_EQ(1, controller.GetEntryCount());

  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell()->web_contents())->
          GetFrameTree()->root();

  // Navigate to a page that fails to load. It must result in an error page, the
  // NEW_PAGE navigation type, and an addition to the history list.
  {
    FrameNavigateParamsCapturer capturer(root);
    NavigateFrameToURL(root, error_url);
    capturer.Wait();
    EXPECT_EQ(NAVIGATION_TYPE_NEW_PAGE, capturer.details().type);
    NavigationEntry* entry = controller.GetLastCommittedEntry();
    EXPECT_EQ(PAGE_TYPE_ERROR, entry->GetPageType());
    EXPECT_EQ(2, controller.GetEntryCount());
  }

  // Navigate again to the page that fails to load. It must result in an error
  // page, the EXISTING_PAGE navigation type, and no addition to the history
  // list. We do not use SAME_PAGE here; that case only differs in that it
  // clears the pending entry, and there is no pending entry after a load
  // failure.
  {
    FrameNavigateParamsCapturer capturer(root);
    NavigateFrameToURL(root, error_url);
    capturer.Wait();
    EXPECT_EQ(NAVIGATION_TYPE_EXISTING_PAGE, capturer.details().type);
    NavigationEntry* entry = controller.GetLastCommittedEntry();
    EXPECT_EQ(PAGE_TYPE_ERROR, entry->GetPageType());
    EXPECT_EQ(2, controller.GetEntryCount());
  }

  // Make a new entry ...
  NavigateToURL(shell(), GURL(url::kAboutBlankURL));
  EXPECT_EQ(3, controller.GetEntryCount());

  // ... and replace it with a failed load. (Note that when you set the
  // should_replace_current_entry flag, the navigation is classified as NEW_PAGE
  // because that is a classification of the renderer's behavior, and the flag
  // is a browser-side flag.)
  {
    FrameNavigateParamsCapturer capturer(root);
    NavigateToURLAndReplace(shell(), error_url);
    capturer.Wait();
    EXPECT_EQ(NAVIGATION_TYPE_NEW_PAGE, capturer.details().type);
    NavigationEntry* entry = controller.GetLastCommittedEntry();
    EXPECT_EQ(PAGE_TYPE_ERROR, entry->GetPageType());
    EXPECT_EQ(3, controller.GetEntryCount());
  }

  // Make a new web ui page to force a process swap ...
  GURL web_ui_page(std::string(kChromeUIScheme) + "://" +
                   std::string(kChromeUIGpuHost));
  NavigateToURL(shell(), web_ui_page);
  EXPECT_EQ(4, controller.GetEntryCount());

  // ... and replace it with a failed load. (It is NEW_PAGE for the reason noted
  // above.)
  {
    FrameNavigateParamsCapturer capturer(root);
    NavigateToURLAndReplace(shell(), error_url);
    capturer.Wait();
    EXPECT_EQ(NAVIGATION_TYPE_NEW_PAGE, capturer.details().type);
    NavigationEntry* entry = controller.GetLastCommittedEntry();
    EXPECT_EQ(PAGE_TYPE_ERROR, entry->GetPageType());
    EXPECT_EQ(4, controller.GetEntryCount());
  }
}

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
    EXPECT_FALSE(capturer.details().is_in_page);
  }

  {
    // Load via a fragment link click.
    FrameNavigateParamsCapturer capturer(root);
    std::string script = "document.getElementById('fraglink').click()";
    EXPECT_TRUE(content::ExecuteScript(root->current_frame_host(), script));
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_LINK, capturer.params().transition);
    EXPECT_EQ(NAVIGATION_TYPE_NEW_PAGE, capturer.details().type);
    EXPECT_TRUE(capturer.details().is_in_page);
  }

  {
    // Load via link click.
    FrameNavigateParamsCapturer capturer(root);
    std::string script = "document.getElementById('thelink').click()";
    EXPECT_TRUE(content::ExecuteScript(root->current_frame_host(), script));
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_LINK, capturer.params().transition);
    EXPECT_EQ(NAVIGATION_TYPE_NEW_PAGE, capturer.details().type);
    EXPECT_FALSE(capturer.details().is_in_page);
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
    EXPECT_FALSE(capturer.details().is_in_page);
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
    EXPECT_TRUE(capturer.details().is_in_page);
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
    EXPECT_FALSE(capturer.details().is_in_page);
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
    EXPECT_FALSE(capturer.details().is_in_page);
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
    EXPECT_FALSE(capturer.details().is_in_page);
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
    EXPECT_FALSE(capturer.details().is_in_page);
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
    EXPECT_FALSE(capturer.details().is_in_page);
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
    EXPECT_FALSE(capturer.details().is_in_page);
  }

  {
    // Reload from the browser side.
    FrameNavigateParamsCapturer capturer(root);
    shell()->web_contents()->GetController().Reload(false);
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_RELOAD, capturer.params().transition);
    EXPECT_EQ(NAVIGATION_TYPE_EXISTING_PAGE, capturer.details().type);
    EXPECT_FALSE(capturer.details().is_in_page);
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
    EXPECT_FALSE(capturer.details().is_in_page);
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
    EXPECT_FALSE(capturer.details().is_in_page);
  }

  // Now, various in-page navigations.

  {
    // history.replaceState().
    FrameNavigateParamsCapturer capturer(root);
    std::string script =
        "history.replaceState({}, 'page 2', 'simple_page_2.html')";
    EXPECT_TRUE(content::ExecuteScript(root->current_frame_host(), script));
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_CLIENT_REDIRECT,
              capturer.params().transition);
    EXPECT_EQ(NAVIGATION_TYPE_EXISTING_PAGE, capturer.details().type);
    EXPECT_TRUE(capturer.details().is_in_page);
  }

  // Back and forward across a fragment navigation.

  GURL url_links(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_links.html"));
  NavigateToURL(shell(), url_links);
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
    EXPECT_EQ(NAVIGATION_TYPE_EXISTING_PAGE, capturer.details().type);
    EXPECT_TRUE(capturer.details().is_in_page);
  }

  {
    // Forward.
    FrameNavigateParamsCapturer capturer(root);
    shell()->web_contents()->GetController().GoForward();
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_FORWARD_BACK,
              capturer.params().transition);
    EXPECT_EQ(NAVIGATION_TYPE_EXISTING_PAGE, capturer.details().type);
    EXPECT_TRUE(capturer.details().is_in_page);
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
    EXPECT_EQ(NAVIGATION_TYPE_EXISTING_PAGE, capturer.details().type);
    EXPECT_TRUE(capturer.details().is_in_page);
  }

  {
    // Forward.
    FrameNavigateParamsCapturer capturer(root);
    shell()->web_contents()->GetController().GoForward();
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_FORWARD_BACK,
              capturer.params().transition);
    EXPECT_EQ(NAVIGATION_TYPE_EXISTING_PAGE, capturer.details().type);
    EXPECT_TRUE(capturer.details().is_in_page);
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

// Verify that empty GURL navigations are not classified as SAME_PAGE.
// See https://crbug.com/534980.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       NavigationTypeClassification_EmptyGURL) {
  GURL url1(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  NavigateToURL(shell(), url1);

  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell()->web_contents())->
          GetFrameTree()->root();

  {
    // Load an (invalid) empty GURL.  Blink will treat this as an inert commit,
    // but we don't want it to show up as SAME_PAGE.
    FrameNavigateParamsCapturer capturer(root);
    NavigateFrameToURL(root, GURL());
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_LINK, capturer.params().transition);
    EXPECT_EQ(NAVIGATION_TYPE_NEW_PAGE, capturer.details().type);
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
    // Initial load.
    LoadCommittedCapturer capturer(root->child_at(0));
    GURL frame_url(embedded_test_server()->GetURL(
        "/navigation_controller/simple_page_1.html"));
    NavigateFrameToURL(root->child_at(0), frame_url);
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_AUTO_SUBFRAME, capturer.transition_type());
  }

  {
    // Simple load.
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    GURL frame_url(embedded_test_server()->GetURL(
        "/navigation_controller/simple_page_2.html"));
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

// Verify that the LoadCommittedDetails::is_in_page value is properly set for
// non-IN_PAGE navigations. (It's tested for IN_PAGE navigations with the
// NavigationTypeClassification_InPage test.)
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       LoadCommittedDetails_IsInPage) {
  GURL links_url(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_links.html"));
  NavigateToURL(shell(), links_url);
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell()->web_contents())->
          GetFrameTree()->root();

  {
    // Do a fragment link click.
    FrameNavigateParamsCapturer capturer(root);
    std::string script = "document.getElementById('fraglink').click()";
    EXPECT_TRUE(content::ExecuteScript(root->current_frame_host(), script));
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_LINK, capturer.params().transition);
    EXPECT_EQ(NAVIGATION_TYPE_NEW_PAGE, capturer.details().type);
    EXPECT_TRUE(capturer.details().is_in_page);
  }

  {
    // Do a non-fragment link click.
    FrameNavigateParamsCapturer capturer(root);
    std::string script = "document.getElementById('thelink').click()";
    EXPECT_TRUE(content::ExecuteScript(root->current_frame_host(), script));
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_LINK, capturer.params().transition);
    EXPECT_EQ(NAVIGATION_TYPE_NEW_PAGE, capturer.details().type);
    EXPECT_FALSE(capturer.details().is_in_page);
  }

  // Second verse, same as the first. (But in a subframe.)

  GURL iframe_url(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_iframe.html"));
  NavigateToURL(shell(), iframe_url);

  root = static_cast<WebContentsImpl*>(shell()->web_contents())->
      GetFrameTree()->root();

  ASSERT_EQ(1U, root->child_count());
  ASSERT_NE(nullptr, root->child_at(0));

  NavigateFrameToURL(root->child_at(0), links_url);
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  {
    // Do a fragment link click.
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    std::string script = "document.getElementById('fraglink').click()";
    EXPECT_TRUE(content::ExecuteScript(root->child_at(0)->current_frame_host(),
                script));
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_MANUAL_SUBFRAME,
              capturer.params().transition);
    EXPECT_EQ(NAVIGATION_TYPE_NEW_SUBFRAME, capturer.details().type);
    EXPECT_TRUE(capturer.details().is_in_page);
  }

  {
    // Do a non-fragment link click.
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    std::string script = "document.getElementById('thelink').click()";
    EXPECT_TRUE(content::ExecuteScript(root->child_at(0)->current_frame_host(),
                script));
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_MANUAL_SUBFRAME,
              capturer.params().transition);
    EXPECT_EQ(NAVIGATION_TYPE_NEW_SUBFRAME, capturer.details().type);
    EXPECT_FALSE(capturer.details().is_in_page);
  }
}

// Verify the tree of FrameNavigationEntries after initial about:blank commits
// in subframes, which should not count as real committed loads.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       FrameNavigationEntry_BlankAutoSubframe) {
  GURL about_blank_url(url::kAboutBlankURL);
  GURL main_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  NavigateToURL(shell(), main_url);
  const NavigationControllerImpl& controller =
      static_cast<const NavigationControllerImpl&>(
          shell()->web_contents()->GetController());
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell()->web_contents())->
          GetFrameTree()->root();

  // 1. Create a iframe with no URL.
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    std::string script = "var iframe = document.createElement('iframe');"
                         "document.body.appendChild(iframe);";
    EXPECT_TRUE(content::ExecuteScript(root->current_frame_host(), script));
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_AUTO_SUBFRAME, capturer.transition_type());
  }

  // Check last committed NavigationEntry.
  EXPECT_EQ(1, controller.GetEntryCount());
  NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(main_url, entry->GetURL());
  FrameNavigationEntry* root_entry = entry->root_node()->frame_entry.get();
  EXPECT_EQ(main_url, root_entry->url());

  // Verify subframe entries if they're enabled (e.g. in --site-per-process).
  if (SiteIsolationPolicy::UseSubframeNavigationEntries()) {
    // The entry should now have one blank subframe FrameNavigationEntry, but
    // this does not count as committing a real load.
    ASSERT_EQ(1U, entry->root_node()->children.size());
    FrameNavigationEntry* frame_entry =
        entry->root_node()->children[0]->frame_entry.get();
    EXPECT_EQ(about_blank_url, frame_entry->url());
  } else {
    // There are no subframe FrameNavigationEntries by default.
    EXPECT_EQ(0U, entry->root_node()->children.size());
  }
  EXPECT_FALSE(root->child_at(0)->has_committed_real_load());

  // 1a. A nested iframe with no URL should also create a subframe entry but not
  // count as a real load.
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    std::string script = "var iframe = document.createElement('iframe');"
                         "document.body.appendChild(iframe);";
    EXPECT_TRUE(content::ExecuteScript(root->child_at(0)->current_frame_host(),
                                       script));
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_AUTO_SUBFRAME, capturer.transition_type());
  }

  // Verify subframe entries if they're enabled (e.g. in --site-per-process).
  if (SiteIsolationPolicy::UseSubframeNavigationEntries()) {
    // The nested entry should have one blank subframe FrameNavigationEntry, but
    // this does not count as committing a real load.
    ASSERT_EQ(1U, entry->root_node()->children[0]->children.size());
    FrameNavigationEntry* frame_entry =
        entry->root_node()->children[0]->children[0]->frame_entry.get();
    EXPECT_EQ(about_blank_url, frame_entry->url());
  } else {
    // There are no subframe FrameNavigationEntries by default.
    EXPECT_EQ(0U, entry->root_node()->children.size());
  }
  EXPECT_FALSE(root->child_at(0)->child_at(0)->has_committed_real_load());

  // 2. Create another iframe with an explicit about:blank URL.
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    std::string script = "var iframe = document.createElement('iframe');"
                         "iframe.src = 'about:blank';"
                         "document.body.appendChild(iframe);";
    EXPECT_TRUE(content::ExecuteScript(root->current_frame_host(), script));
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_AUTO_SUBFRAME, capturer.transition_type());
  }

  // Check last committed NavigationEntry.
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(entry, controller.GetLastCommittedEntry());

  // Verify subframe entries if they're enabled (e.g. in --site-per-process).
  if (SiteIsolationPolicy::UseSubframeNavigationEntries()) {
    // The new entry should have one blank subframe FrameNavigationEntry, but
    // this does not count as committing a real load.
    ASSERT_EQ(2U, entry->root_node()->children.size());
    FrameNavigationEntry* frame_entry =
        entry->root_node()->children[1]->frame_entry.get();
    EXPECT_EQ(about_blank_url, frame_entry->url());
  } else {
    // There are no subframe FrameNavigationEntries by default.
    EXPECT_EQ(0U, entry->root_node()->children.size());
  }
  EXPECT_FALSE(root->child_at(1)->has_committed_real_load());

  // 3. A real same-site navigation in the nested iframe should be AUTO.
  GURL frame_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  {
    LoadCommittedCapturer capturer(root->child_at(0)->child_at(0));
    std::string script = "var frames = document.getElementsByTagName('iframe');"
                         "frames[0].src = '" + frame_url.spec() + "';";
    EXPECT_TRUE(content::ExecuteScript(root->child_at(0)->current_frame_host(),
                                       script));
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_AUTO_SUBFRAME, capturer.transition_type());
  }

  // Check last committed NavigationEntry.  It should have replaced the previous
  // frame entry in the original NavigationEntry.
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(entry, controller.GetLastCommittedEntry());

  // Verify subframe entries if they're enabled (e.g. in --site-per-process).
  if (SiteIsolationPolicy::UseSubframeNavigationEntries()) {
    // The entry should still have one nested subframe FrameNavigationEntry.
    ASSERT_EQ(1U, entry->root_node()->children[0]->children.size());
    FrameNavigationEntry* frame_entry =
        entry->root_node()->children[0]->children[0]->frame_entry.get();
    EXPECT_EQ(frame_url, frame_entry->url());
  } else {
    // There are no subframe FrameNavigationEntries by default.
    EXPECT_EQ(0U, entry->root_node()->children.size());
  }
  EXPECT_FALSE(root->child_at(0)->has_committed_real_load());
  EXPECT_TRUE(root->child_at(0)->child_at(0)->has_committed_real_load());
  EXPECT_FALSE(root->child_at(1)->has_committed_real_load());

  // 4. A real cross-site navigation in the second iframe should be AUTO.
  GURL foo_url(embedded_test_server()->GetURL(
      "foo.com", "/navigation_controller/simple_page_2.html"));
  {
    LoadCommittedCapturer capturer(root->child_at(1));
    std::string script = "var frames = document.getElementsByTagName('iframe');"
                         "frames[1].src = '" + foo_url.spec() + "';";
    EXPECT_TRUE(content::ExecuteScript(root->current_frame_host(), script));
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_AUTO_SUBFRAME, capturer.transition_type());
  }

  // Check last committed NavigationEntry.
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(entry, controller.GetLastCommittedEntry());

  // Verify subframe entries if they're enabled (e.g. in --site-per-process).
  if (SiteIsolationPolicy::UseSubframeNavigationEntries()) {
    // The entry should still have two subframe FrameNavigationEntries.
    ASSERT_EQ(2U, entry->root_node()->children.size());
    FrameNavigationEntry* frame_entry =
        entry->root_node()->children[1]->frame_entry.get();
    EXPECT_EQ(foo_url, frame_entry->url());
  } else {
    // There are no subframe FrameNavigationEntries by default.
    EXPECT_EQ(0U, entry->root_node()->children.size());
  }
  EXPECT_FALSE(root->child_at(0)->has_committed_real_load());
  EXPECT_TRUE(root->child_at(0)->child_at(0)->has_committed_real_load());
  EXPECT_TRUE(root->child_at(1)->has_committed_real_load());

  // 5. A new navigation to about:blank in the nested frame should count as a
  // real load, since that frame has already committed a real load and this is
  // not the initial blank page.
  {
    LoadCommittedCapturer capturer(root->child_at(0)->child_at(0));
    std::string script = "var frames = document.getElementsByTagName('iframe');"
                         "frames[0].src = 'about:blank';";
    EXPECT_TRUE(content::ExecuteScript(root->child_at(0)->current_frame_host(),
                                       script));
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_MANUAL_SUBFRAME, capturer.transition_type());
  }

  // This should have created a new NavigationEntry.
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_NE(entry, controller.GetLastCommittedEntry());
  NavigationEntryImpl* entry2 = controller.GetLastCommittedEntry();

  // Verify subframe entries if they're enabled (e.g. in --site-per-process).
  if (SiteIsolationPolicy::UseSubframeNavigationEntries()) {
    ASSERT_EQ(2U, entry->root_node()->children.size());
    FrameNavigationEntry* frame_entry =
        entry2->root_node()->children[0]->children[0]->frame_entry.get();
    EXPECT_EQ(about_blank_url, frame_entry->url());
  } else {
    // There are no subframe FrameNavigationEntries by default.
    EXPECT_EQ(0U, entry->root_node()->children.size());
  }
  EXPECT_FALSE(root->child_at(0)->has_committed_real_load());
  EXPECT_TRUE(root->child_at(0)->child_at(0)->has_committed_real_load());
  EXPECT_TRUE(root->child_at(1)->has_committed_real_load());

  // Check the end result of the frame tree.
  if (AreAllSitesIsolatedForTesting()) {
    FrameTreeVisualizer visualizer;
    EXPECT_EQ(
        " Site A ------------ proxies for B\n"
        "   |--Site A ------- proxies for B\n"
        "   |    +--Site A -- proxies for B\n"
        "   +--Site B ------- proxies for A\n"
        "Where A = http://127.0.0.1/\n"
        "      B = http://foo.com/",
        visualizer.DepictFrameTree(root));
  }
}

// Verify the tree of FrameNavigationEntries after NAVIGATION_TYPE_AUTO_SUBFRAME
// commits.
// TODO(creis): Test updating entries for history auto subframe navigations.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       FrameNavigationEntry_AutoSubframe) {
  GURL main_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  NavigateToURL(shell(), main_url);
  const NavigationControllerImpl& controller =
      static_cast<const NavigationControllerImpl&>(
          shell()->web_contents()->GetController());
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell()->web_contents())->
          GetFrameTree()->root();

  // 1. Create a same-site iframe.
  GURL frame_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_2.html"));
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    std::string script = "var iframe = document.createElement('iframe');"
                         "iframe.src = '" + frame_url.spec() + "';"
                         "document.body.appendChild(iframe);";
    EXPECT_TRUE(content::ExecuteScript(root->current_frame_host(), script));
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_AUTO_SUBFRAME, capturer.transition_type());
  }

  // Check last committed NavigationEntry.
  EXPECT_EQ(1, controller.GetEntryCount());
  NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(main_url, entry->GetURL());
  FrameNavigationEntry* root_entry = entry->root_node()->frame_entry.get();
  EXPECT_EQ(main_url, root_entry->url());
  EXPECT_FALSE(controller.GetPendingEntry());

  // Verify subframe entries if they're enabled (e.g. in --site-per-process).
  if (SiteIsolationPolicy::UseSubframeNavigationEntries()) {
    // The entry should now have a subframe FrameNavigationEntry.
    ASSERT_EQ(1U, entry->root_node()->children.size());
    FrameNavigationEntry* frame_entry =
        entry->root_node()->children[0]->frame_entry.get();
    EXPECT_EQ(frame_url, frame_entry->url());
    EXPECT_TRUE(root->child_at(0)->has_committed_real_load());
  } else {
    // There are no subframe FrameNavigationEntries by default.
    EXPECT_EQ(0U, entry->root_node()->children.size());
  }

  // 2. Create a second, initially cross-site iframe.
  GURL foo_url(embedded_test_server()->GetURL(
      "foo.com", "/navigation_controller/simple_page_1.html"));
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    std::string script = "var iframe = document.createElement('iframe');"
                         "iframe.src = '" + foo_url.spec() + "';"
                         "document.body.appendChild(iframe);";
    EXPECT_TRUE(content::ExecuteScript(root->current_frame_host(), script));
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_AUTO_SUBFRAME, capturer.transition_type());
  }

  // The last committed NavigationEntry shouldn't have changed.
  EXPECT_EQ(1, controller.GetEntryCount());
  entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(main_url, entry->GetURL());
  root_entry = entry->root_node()->frame_entry.get();
  EXPECT_EQ(main_url, root_entry->url());
  EXPECT_FALSE(controller.GetPendingEntry());

  // Verify subframe entries if they're enabled (e.g. in --site-per-process).
  if (SiteIsolationPolicy::UseSubframeNavigationEntries()) {
    // The entry should now have 2 subframe FrameNavigationEntries.
    ASSERT_EQ(2U, entry->root_node()->children.size());
    FrameNavigationEntry* frame_entry =
        entry->root_node()->children[1]->frame_entry.get();
    EXPECT_EQ(foo_url, frame_entry->url());
    EXPECT_TRUE(root->child_at(1)->has_committed_real_load());
  } else {
    // There are no subframe FrameNavigationEntries by default.
    EXPECT_EQ(0U, entry->root_node()->children.size());
  }

  // 3. Create a nested iframe in the second subframe.
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    std::string script = "var iframe = document.createElement('iframe');"
                         "iframe.src = '" + foo_url.spec() + "';"
                         "document.body.appendChild(iframe);";
    EXPECT_TRUE(content::ExecuteScript(root->child_at(1)->current_frame_host(),
                                       script));
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_AUTO_SUBFRAME, capturer.transition_type());
  }

  // The last committed NavigationEntry shouldn't have changed.
  EXPECT_EQ(1, controller.GetEntryCount());
  entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(main_url, entry->GetURL());
  root_entry = entry->root_node()->frame_entry.get();
  EXPECT_EQ(main_url, root_entry->url());

  // Verify subframe entries if they're enabled (e.g. in --site-per-process).
  if (SiteIsolationPolicy::UseSubframeNavigationEntries()) {
    // The entry should now have 2 subframe FrameNavigationEntries.
    ASSERT_EQ(2U, entry->root_node()->children.size());
    ASSERT_EQ(1U, entry->root_node()->children[1]->children.size());
    FrameNavigationEntry* frame_entry =
        entry->root_node()->children[1]->children[0]->frame_entry.get();
    EXPECT_EQ(foo_url, frame_entry->url());
  } else {
    // There are no subframe FrameNavigationEntries by default.
    EXPECT_EQ(0U, entry->root_node()->children.size());
  }

  // 4. Create a third iframe on the same site as the second.  This ensures that
  // the commit type is correct even when the subframe process already exists.
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    std::string script = "var iframe = document.createElement('iframe');"
                         "iframe.src = '" + foo_url.spec() + "';"
                         "document.body.appendChild(iframe);";
    EXPECT_TRUE(content::ExecuteScript(root->current_frame_host(), script));
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_AUTO_SUBFRAME, capturer.transition_type());
  }

  // The last committed NavigationEntry shouldn't have changed.
  EXPECT_EQ(1, controller.GetEntryCount());
  entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(main_url, entry->GetURL());
  root_entry = entry->root_node()->frame_entry.get();
  EXPECT_EQ(main_url, root_entry->url());

  // Verify subframe entries if they're enabled (e.g. in --site-per-process).
  if (SiteIsolationPolicy::UseSubframeNavigationEntries()) {
    // The entry should now have 3 subframe FrameNavigationEntries.
    ASSERT_EQ(3U, entry->root_node()->children.size());
    FrameNavigationEntry* frame_entry =
        entry->root_node()->children[2]->frame_entry.get();
    EXPECT_EQ(foo_url, frame_entry->url());
  } else {
    // There are no subframe FrameNavigationEntries by default.
    EXPECT_EQ(0U, entry->root_node()->children.size());
  }

  // 5. Create a nested iframe on the original site (A-B-A).
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    std::string script = "var iframe = document.createElement('iframe');"
                         "iframe.src = '" + frame_url.spec() + "';"
                         "document.body.appendChild(iframe);";
    FrameTreeNode* child = root->child_at(2);
    EXPECT_TRUE(content::ExecuteScript(child->current_frame_host(), script));
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_AUTO_SUBFRAME, capturer.transition_type());
  }

  // The last committed NavigationEntry shouldn't have changed.
  EXPECT_EQ(1, controller.GetEntryCount());
  entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(main_url, entry->GetURL());
  root_entry = entry->root_node()->frame_entry.get();
  EXPECT_EQ(main_url, root_entry->url());

  // Verify subframe entries if they're enabled (e.g. in --site-per-process).
  if (SiteIsolationPolicy::UseSubframeNavigationEntries()) {
    // There should be a corresponding FrameNavigationEntry.
    ASSERT_EQ(1U, entry->root_node()->children[2]->children.size());
    FrameNavigationEntry* frame_entry =
        entry->root_node()->children[2]->children[0]->frame_entry.get();
    EXPECT_EQ(frame_url, frame_entry->url());
  } else {
    // There are no subframe FrameNavigationEntries by default.
    EXPECT_EQ(0U, entry->root_node()->children.size());
  }

  // Check the end result of the frame tree.
  if (AreAllSitesIsolatedForTesting()) {
    FrameTreeVisualizer visualizer;
    EXPECT_EQ(
        " Site A ------------ proxies for B\n"
        "   |--Site A ------- proxies for B\n"
        "   |--Site B ------- proxies for A\n"
        "   |    +--Site B -- proxies for A\n"
        "   +--Site B ------- proxies for A\n"
        "        +--Site A -- proxies for B\n"
        "Where A = http://127.0.0.1/\n"
        "      B = http://foo.com/",
        visualizer.DepictFrameTree(root));
  }
}

// Verify the tree of FrameNavigationEntries after NAVIGATION_TYPE_NEW_SUBFRAME
// commits.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       FrameNavigationEntry_NewSubframe) {
  GURL main_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  NavigateToURL(shell(), main_url);
  const NavigationControllerImpl& controller =
      static_cast<const NavigationControllerImpl&>(
          shell()->web_contents()->GetController());
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell()->web_contents())->
          GetFrameTree()->root();

  // 1. Create a same-site iframe.
  GURL frame_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_2.html"));
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    std::string script = "var iframe = document.createElement('iframe');"
                         "iframe.src = '" + frame_url.spec() + "';"
                         "document.body.appendChild(iframe);";
    EXPECT_TRUE(content::ExecuteScript(root->current_frame_host(), script));
    capturer.Wait();
  }
  NavigationEntryImpl* entry = controller.GetLastCommittedEntry();

  // 2. Navigate in the subframe same-site.
  GURL frame_url2(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_links.html"));
  {
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    NavigateFrameToURL(root->child_at(0), frame_url2);
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_MANUAL_SUBFRAME,
              capturer.params().transition);
    EXPECT_EQ(NAVIGATION_TYPE_NEW_SUBFRAME, capturer.details().type);
  }

  // We should have created a new NavigationEntry with the same main frame URL.
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  NavigationEntryImpl* entry2 = controller.GetLastCommittedEntry();
  EXPECT_NE(entry, entry2);
  EXPECT_EQ(main_url, entry2->GetURL());
  FrameNavigationEntry* root_entry2 = entry2->root_node()->frame_entry.get();
  EXPECT_EQ(main_url, root_entry2->url());

  // Verify subframe entries if they're enabled (e.g. in --site-per-process).
  if (SiteIsolationPolicy::UseSubframeNavigationEntries()) {
    // The entry should have a new FrameNavigationEntries for the subframe.
    ASSERT_EQ(1U, entry2->root_node()->children.size());
    EXPECT_EQ(frame_url2, entry2->root_node()->children[0]->frame_entry->url());
  } else {
    // There are no subframe FrameNavigationEntries by default.
    EXPECT_EQ(0U, entry2->root_node()->children.size());
  }

  // 3. Create a second, initially cross-site iframe.
  GURL foo_url(embedded_test_server()->GetURL(
      "foo.com", "/navigation_controller/simple_page_1.html"));
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    std::string script = "var iframe = document.createElement('iframe');"
                         "iframe.src = '" + foo_url.spec() + "';"
                         "document.body.appendChild(iframe);";
    EXPECT_TRUE(content::ExecuteScript(root->current_frame_host(), script));
    capturer.Wait();
  }

  // 4. Create a nested same-site iframe in the second subframe, wait for it to
  // commit, then navigate it again.
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    std::string script = "var iframe = document.createElement('iframe');"
                         "iframe.src = '" + foo_url.spec() + "';"
                         "document.body.appendChild(iframe);";
    EXPECT_TRUE(content::ExecuteScript(root->child_at(1)->current_frame_host(),
                                       script));
    capturer.Wait();
  }
  GURL bar_url(embedded_test_server()->GetURL(
      "bar.com", "/navigation_controller/simple_page_1.html"));
  {
    FrameNavigateParamsCapturer capturer(root->child_at(1)->child_at(0));
    NavigateFrameToURL(root->child_at(1)->child_at(0), bar_url);
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_MANUAL_SUBFRAME,
              capturer.params().transition);
    EXPECT_EQ(NAVIGATION_TYPE_NEW_SUBFRAME, capturer.details().type);
  }

  // We should have created a new NavigationEntry with the same main frame URL.
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());
  NavigationEntryImpl* entry3 = controller.GetLastCommittedEntry();
  EXPECT_NE(entry, entry3);
  EXPECT_EQ(main_url, entry3->GetURL());
  FrameNavigationEntry* root_entry3 = entry3->root_node()->frame_entry.get();
  EXPECT_EQ(main_url, root_entry3->url());

  // Verify subframe entries if they're enabled (e.g. in --site-per-process).
  if (SiteIsolationPolicy::UseSubframeNavigationEntries()) {
    // The entry should still have FrameNavigationEntries for all 3 subframes.
    ASSERT_EQ(2U, entry3->root_node()->children.size());
    EXPECT_EQ(frame_url2, entry3->root_node()->children[0]->frame_entry->url());
    EXPECT_EQ(foo_url, entry3->root_node()->children[1]->frame_entry->url());
    ASSERT_EQ(1U, entry3->root_node()->children[1]->children.size());
    EXPECT_EQ(
        bar_url,
        entry3->root_node()->children[1]->children[0]->frame_entry->url());
  } else {
    // There are no subframe FrameNavigationEntries by default.
    EXPECT_EQ(0U, entry3->root_node()->children.size());
  }

  // 6. Navigate the second subframe cross-site, clearing its existing subtree.
  GURL baz_url(embedded_test_server()->GetURL(
      "baz.com", "/navigation_controller/simple_page_1.html"));
  {
    FrameNavigateParamsCapturer capturer(root->child_at(1));
    std::string script = "var frames = document.getElementsByTagName('iframe');"
                         "frames[1].src = '" + baz_url.spec() + "';";
    EXPECT_TRUE(content::ExecuteScript(root->current_frame_host(), script));
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_MANUAL_SUBFRAME,
              capturer.params().transition);
    EXPECT_EQ(NAVIGATION_TYPE_NEW_SUBFRAME, capturer.details().type);
  }

  // We should have created a new NavigationEntry with the same main frame URL.
  EXPECT_EQ(4, controller.GetEntryCount());
  EXPECT_EQ(3, controller.GetLastCommittedEntryIndex());
  NavigationEntryImpl* entry4 = controller.GetLastCommittedEntry();
  EXPECT_NE(entry, entry4);
  EXPECT_EQ(main_url, entry4->GetURL());
  FrameNavigationEntry* root_entry4 = entry4->root_node()->frame_entry.get();
  EXPECT_EQ(main_url, root_entry4->url());

  // Verify subframe entries if they're enabled (e.g. in --site-per-process).
  if (SiteIsolationPolicy::UseSubframeNavigationEntries()) {
    // The entry should still have FrameNavigationEntries for all 3 subframes.
    ASSERT_EQ(2U, entry4->root_node()->children.size());
    EXPECT_EQ(frame_url2, entry4->root_node()->children[0]->frame_entry->url());
    EXPECT_EQ(baz_url, entry4->root_node()->children[1]->frame_entry->url());
    ASSERT_EQ(0U, entry4->root_node()->children[1]->children.size());
  } else {
    // There are no subframe FrameNavigationEntries by default.
    EXPECT_EQ(0U, entry4->root_node()->children.size());
  }

  // Check the end result of the frame tree.
  if (AreAllSitesIsolatedForTesting()) {
    FrameTreeVisualizer visualizer;
    EXPECT_EQ(
        " Site A ------------ proxies for B\n"
        "   |--Site A ------- proxies for B\n"
        "   +--Site B ------- proxies for A\n"
        "Where A = http://127.0.0.1/\n"
        "      B = http://baz.com/",
        visualizer.DepictFrameTree(root));
  }
}

// Ensure that we don't crash when navigating subframes after in-page
// navigations.  See https://crbug.com/522193.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       FrameNavigationEntry_SubframeAfterInPage) {
  // 1. Start on a page with a subframe.
  GURL main_url(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_iframe.html"));
  NavigateToURL(shell(), main_url);
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell()->web_contents())->
          GetFrameTree()->root();

  ASSERT_EQ(1U, root->child_count());
  ASSERT_NE(nullptr, root->child_at(0));

  // Navigate to a real page in the subframe, so that the next navigation will
  // be MANUAL_SUBFRAME.
  GURL subframe_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  {
    LoadCommittedCapturer capturer(root->child_at(0));
    NavigateFrameToURL(root->child_at(0), subframe_url);
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_AUTO_SUBFRAME, capturer.transition_type());
  }

  // 2. In-page navigation in the main frame.
  std::string push_script = "history.pushState({}, 'page 2', 'page_2.html')";
  EXPECT_TRUE(content::ExecuteScript(root->current_frame_host(), push_script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // TODO(creis): Verify subframe entries.  https://crbug.com/522193.

  // 3. Add a nested subframe.
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    std::string script = "var iframe = document.createElement('iframe');"
                         "iframe.src = '" + subframe_url.spec() + "';"
                         "document.body.appendChild(iframe);";
    EXPECT_TRUE(content::ExecuteScript(root->child_at(0)->current_frame_host(),
                                       script));
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_AUTO_SUBFRAME, capturer.transition_type());
  }

  // TODO(creis): Verify subframe entries.  https://crbug.com/522193.
}

// Verify the tree of FrameNavigationEntries after back/forward navigations in a
// cross-site subframe.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       FrameNavigationEntry_SubframeBackForward) {
  GURL main_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  NavigateToURL(shell(), main_url);
  const NavigationControllerImpl& controller =
      static_cast<const NavigationControllerImpl&>(
          shell()->web_contents()->GetController());
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  // 1. Create a same-site iframe.
  GURL frame_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_2.html"));
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    std::string script = "var iframe = document.createElement('iframe');"
                         "iframe.src = '" + frame_url.spec() + "';"
                         "document.body.appendChild(iframe);";
    EXPECT_TRUE(content::ExecuteScript(root->current_frame_host(), script));
    capturer.Wait();
  }
  NavigationEntryImpl* entry1 = controller.GetLastCommittedEntry();

  // 2. Navigate in the subframe cross-site.
  GURL frame_url2(embedded_test_server()->GetURL(
      "foo.com", "/navigation_controller/page_with_links.html"));
  {
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    NavigateFrameToURL(root->child_at(0), frame_url2);
    capturer.Wait();
  }
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  NavigationEntryImpl* entry2 = controller.GetLastCommittedEntry();

  // 3. Navigate in the subframe cross-site again.
  GURL frame_url3(embedded_test_server()->GetURL(
      "bar.com", "/navigation_controller/page_with_links.html"));
  {
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    NavigateFrameToURL(root->child_at(0), frame_url3);
    capturer.Wait();
  }
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());
  NavigationEntryImpl* entry3 = controller.GetLastCommittedEntry();

  // 4. Go back in the subframe.
  {
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    shell()->web_contents()->GetController().GoBack();
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_AUTO_SUBFRAME, capturer.params().transition);
    EXPECT_EQ(NAVIGATION_TYPE_AUTO_SUBFRAME, capturer.details().type);
  }
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(entry2, controller.GetLastCommittedEntry());

  // Verify subframe entries if they're enabled (e.g. in --site-per-process).
  if (SiteIsolationPolicy::UseSubframeNavigationEntries()) {
    // The entry should have a new FrameNavigationEntries for the subframe.
    ASSERT_EQ(1U, entry2->root_node()->children.size());
    EXPECT_EQ(frame_url2, entry2->root_node()->children[0]->frame_entry->url());
  } else {
    // There are no subframe FrameNavigationEntries by default.
    EXPECT_EQ(0U, entry2->root_node()->children.size());
  }

  // 5. Go back in the subframe again to the parent page's site.
  {
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    shell()->web_contents()->GetController().GoBack();
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_AUTO_SUBFRAME, capturer.params().transition);
    EXPECT_EQ(NAVIGATION_TYPE_AUTO_SUBFRAME, capturer.details().type);
  }
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(entry1, controller.GetLastCommittedEntry());

  // Verify subframe entries if they're enabled (e.g. in --site-per-process).
  if (SiteIsolationPolicy::UseSubframeNavigationEntries()) {
    // The entry should have a new FrameNavigationEntries for the subframe.
    ASSERT_EQ(1U, entry1->root_node()->children.size());
    EXPECT_EQ(frame_url, entry1->root_node()->children[0]->frame_entry->url());
  } else {
    // There are no subframe FrameNavigationEntries by default.
    EXPECT_EQ(0U, entry1->root_node()->children.size());
  }

  // 6. Go forward in the subframe cross-site.
  {
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    shell()->web_contents()->GetController().GoForward();
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_AUTO_SUBFRAME, capturer.params().transition);
    EXPECT_EQ(NAVIGATION_TYPE_AUTO_SUBFRAME, capturer.details().type);
  }
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(entry2, controller.GetLastCommittedEntry());

  // Verify subframe entries if they're enabled (e.g. in --site-per-process).
  if (SiteIsolationPolicy::UseSubframeNavigationEntries()) {
    // The entry should have a new FrameNavigationEntries for the subframe.
    ASSERT_EQ(1U, entry2->root_node()->children.size());
    EXPECT_EQ(frame_url2, entry2->root_node()->children[0]->frame_entry->url());
  } else {
    // There are no subframe FrameNavigationEntries by default.
    EXPECT_EQ(0U, entry2->root_node()->children.size());
  }

  // 7. Go forward in the subframe again, cross-site.
  {
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    shell()->web_contents()->GetController().GoForward();
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_AUTO_SUBFRAME, capturer.params().transition);
    EXPECT_EQ(NAVIGATION_TYPE_AUTO_SUBFRAME, capturer.details().type);
  }
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(entry3, controller.GetLastCommittedEntry());

  // Verify subframe entries if they're enabled (e.g. in --site-per-process).
  if (SiteIsolationPolicy::UseSubframeNavigationEntries()) {
    // The entry should have a new FrameNavigationEntries for the subframe.
    ASSERT_EQ(1U, entry3->root_node()->children.size());
    EXPECT_EQ(frame_url3, entry3->root_node()->children[0]->frame_entry->url());
  } else {
    // There are no subframe FrameNavigationEntries by default.
    EXPECT_EQ(0U, entry3->root_node()->children.size());
  }
}

// Verifies that the |frame_unique_name| is set to the correct frame, so that we
// can match subframe FrameNavigationEntries to newly created frames after
// back/forward and restore.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       FrameNavigationEntry_FrameUniqueName) {
  const NavigationControllerImpl& controller =
      static_cast<const NavigationControllerImpl&>(
          shell()->web_contents()->GetController());

  // 1. Navigate the main frame.
  GURL url(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_links.html"));
  NavigateToURL(shell(), url);
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  SiteInstance* main_site_instance =
      root->current_frame_host()->GetSiteInstance();

  // The main frame defaults to an empty name.
  FrameNavigationEntry* frame_entry =
      controller.GetLastCommittedEntry()->GetFrameEntry(root);
  EXPECT_EQ("", frame_entry->frame_unique_name());

  // Test subframe unique names only if enabled, e.g. in --site-per-process.
  if (!SiteIsolationPolicy::UseSubframeNavigationEntries())
    return;

  // 2. Add an unnamed subframe, which does an AUTO_SUBFRAME navigation.
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    std::string script = "var iframe = document.createElement('iframe');"
                         "iframe.src = '" + url.spec() + "';"
                         "document.body.appendChild(iframe);";
    EXPECT_TRUE(content::ExecuteScript(root->current_frame_host(), script));
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_AUTO_SUBFRAME, capturer.transition_type());
  }

  // The root FrameNavigationEntry hasn't changed.
  EXPECT_EQ(frame_entry,
            controller.GetLastCommittedEntry()->GetFrameEntry(root));

  // The subframe should have a generated name.
  FrameTreeNode* subframe = root->child_at(0);
  EXPECT_EQ(main_site_instance,
            subframe->current_frame_host()->GetSiteInstance());
  FrameNavigationEntry* subframe_entry =
      controller.GetLastCommittedEntry()->GetFrameEntry(subframe);
  std::string unnamed_subframe_name = "<!--framePath //<!--frame0-->-->";
  EXPECT_EQ(unnamed_subframe_name, subframe_entry->frame_unique_name());

  // 3. Add a named subframe.
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    std::string script = "var iframe = document.createElement('iframe');"
                         "iframe.src = '" + url.spec() + "';"
                         "iframe.name = 'foo';"
                         "document.body.appendChild(iframe);";
    EXPECT_TRUE(content::ExecuteScript(root->current_frame_host(), script));
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_AUTO_SUBFRAME, capturer.transition_type());
  }

  // The new subframe should have the specified name.
  EXPECT_EQ(frame_entry,
            controller.GetLastCommittedEntry()->GetFrameEntry(root));
  FrameTreeNode* foo_subframe = root->child_at(1);
  EXPECT_EQ(main_site_instance,
            foo_subframe->current_frame_host()->GetSiteInstance());
  FrameNavigationEntry* foo_subframe_entry =
      controller.GetLastCommittedEntry()->GetFrameEntry(foo_subframe);
  std::string named_subframe_name = "foo";
  EXPECT_EQ(named_subframe_name, foo_subframe_entry->frame_unique_name());

  // 4. Navigating in the subframes cross-process shouldn't change their names.
  // TODO(creis): Fix the unnamed case in https://crbug.com/502317.
  GURL bar_url(embedded_test_server()->GetURL(
      "bar.com", "/navigation_controller/simple_page_1.html"));
  NavigateFrameToURL(foo_subframe, bar_url);
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_NE(main_site_instance,
            foo_subframe->current_frame_host()->GetSiteInstance());
  foo_subframe_entry =
      controller.GetLastCommittedEntry()->GetFrameEntry(foo_subframe);
  EXPECT_EQ(named_subframe_name, foo_subframe_entry->frame_unique_name());
}

// Verifies that item sequence numbers and document sequence numbers update
// properly for main frames and subframes.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       FrameNavigationEntry_SequenceNumbers) {
  const NavigationControllerImpl& controller =
      static_cast<const NavigationControllerImpl&>(
          shell()->web_contents()->GetController());

  // 1. Navigate the main frame.
  GURL url(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_links.html"));
  NavigateToURL(shell(), url);
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  FrameNavigationEntry* frame_entry =
      controller.GetLastCommittedEntry()->GetFrameEntry(root);
  int64 isn_1 = frame_entry->item_sequence_number();
  int64 dsn_1 = frame_entry->document_sequence_number();
  EXPECT_NE(-1, isn_1);
  EXPECT_NE(-1, dsn_1);

  // 2. Do an in-page fragment navigation.
  std::string script = "document.getElementById('fraglink').click()";
  EXPECT_TRUE(content::ExecuteScript(root->current_frame_host(), script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  frame_entry = controller.GetLastCommittedEntry()->GetFrameEntry(root);
  int64 isn_2 = frame_entry->item_sequence_number();
  int64 dsn_2 = frame_entry->document_sequence_number();
  EXPECT_NE(-1, isn_2);
  EXPECT_NE(isn_1, isn_2);
  EXPECT_EQ(dsn_1, dsn_2);

  // Test subframe sequence numbers only if enabled, e.g. in --site-per-process.
  if (!SiteIsolationPolicy::UseSubframeNavigationEntries())
    return;

  // 3. Add a subframe, which does an AUTO_SUBFRAME navigation.
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    std::string add_script = "var iframe = document.createElement('iframe');"
                             "iframe.src = '" + url.spec() + "';"
                             "document.body.appendChild(iframe);";
    EXPECT_TRUE(content::ExecuteScript(root->current_frame_host(), add_script));
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_AUTO_SUBFRAME, capturer.transition_type());
  }

  // The root FrameNavigationEntry hasn't changed.
  EXPECT_EQ(frame_entry,
            controller.GetLastCommittedEntry()->GetFrameEntry(root));

  // We should have a unique ISN and DSN for the subframe entry.
  FrameTreeNode* subframe = root->child_at(0);
  FrameNavigationEntry* subframe_entry =
      controller.GetLastCommittedEntry()->GetFrameEntry(subframe);
  int64 isn_3 = subframe_entry->item_sequence_number();
  int64 dsn_3 = subframe_entry->document_sequence_number();
  EXPECT_NE(-1, isn_2);
  EXPECT_NE(isn_2, isn_3);
  EXPECT_NE(dsn_2, dsn_3);

  // 4. Do an in-page fragment navigation in the subframe.
  EXPECT_TRUE(content::ExecuteScript(subframe->current_frame_host(), script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  subframe_entry = controller.GetLastCommittedEntry()->GetFrameEntry(subframe);
  int64 isn_4 = subframe_entry->item_sequence_number();
  int64 dsn_4 = subframe_entry->document_sequence_number();
  EXPECT_NE(-1, isn_4);
  EXPECT_NE(isn_3, isn_4);
  EXPECT_EQ(dsn_3, dsn_4);
}

namespace {

// Loads |start_url|, then loads |stalled_url| which stalls. While the page is
// stalled, an in-page navigation happens. Make sure that all the navigations
// are properly classified.
void DoReplaceStateWhilePending(Shell* shell,
                                const GURL& start_url,
                                const GURL& stalled_url,
                                const std::string& replace_state_filename) {
  NavigationControllerImpl& controller =
      static_cast<NavigationControllerImpl&>(
          shell->web_contents()->GetController());

  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell->web_contents())->
          GetFrameTree()->root();

  // Start with one page.
  EXPECT_TRUE(NavigateToURL(shell, start_url));

  // Have the user decide to go to a different page which is very slow.
  NavigationStallDelegate stall_delegate(stalled_url);
  ResourceDispatcherHost::Get()->SetDelegate(&stall_delegate);
  controller.LoadURL(
      stalled_url, Referrer(), ui::PAGE_TRANSITION_LINK, std::string());

  // That should be the pending entry.
  NavigationEntryImpl* entry = controller.GetPendingEntry();
  ASSERT_NE(nullptr, entry);
  EXPECT_EQ(stalled_url, entry->GetURL());

  {
    // Now the existing page uses history.replaceState().
    FrameNavigateParamsCapturer capturer(root);
    capturer.set_wait_for_load(false);
    std::string script =
        "history.replaceState({}, '', '" + replace_state_filename + "')";
    EXPECT_TRUE(content::ExecuteScript(root->current_frame_host(), script));
    capturer.Wait();

    // The fact that there was a pending entry shouldn't interfere with the
    // classification.
    EXPECT_EQ(NAVIGATION_TYPE_EXISTING_PAGE, capturer.details().type);
    EXPECT_TRUE(capturer.details().is_in_page);
  }

  ResourceDispatcherHost::Get()->SetDelegate(nullptr);
}

}  // namespace

IN_PROC_BROWSER_TEST_F(
    NavigationControllerBrowserTest,
    NavigationTypeClassification_On1InPageToXWhile2Pending) {
  GURL url1(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  GURL url2(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_2.html"));
  DoReplaceStateWhilePending(shell(), url1, url2, "x");
}

IN_PROC_BROWSER_TEST_F(
    NavigationControllerBrowserTest,
    NavigationTypeClassification_On1InPageTo2While2Pending) {
  GURL url1(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  GURL url2(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_2.html"));
  DoReplaceStateWhilePending(shell(), url1, url2, "simple_page_2.html");
}

IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       NavigationTypeClassification_On1InPageToXWhile1Pending) {
  GURL url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  DoReplaceStateWhilePending(shell(), url, url, "x");
}

IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       NavigationTypeClassification_On1InPageTo1While1Pending) {
  GURL url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  DoReplaceStateWhilePending(shell(), url, url, "simple_page_1.html");
}

// Ensure the renderer process does not get confused about the current entry
// due to subframes and replaced entries.  See https://crbug.com/480201.
// TODO(creis): Re-enable for Site Isolation FYI bots: https://crbug.com/502317.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       PreventSpoofFromSubframeAndReplace) {
  // Start at an initial URL.
  GURL url1(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  NavigateToURL(shell(), url1);

  // Now go to a page with a real iframe.
  GURL url2(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_data_iframe.html"));
  NavigateToURL(shell(), url2);

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  ASSERT_EQ(1U, root->child_count());
  ASSERT_NE(nullptr, root->child_at(0));

  {
    // Navigate in the iframe.
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    GURL frame_url(embedded_test_server()->GetURL(
        "/navigation_controller/simple_page_2.html"));
    NavigateFrameToURL(root->child_at(0), frame_url);
    capturer.Wait();
    EXPECT_EQ(NAVIGATION_TYPE_NEW_SUBFRAME, capturer.details().type);
  }

  {
    // Go back in the iframe.
    TestNavigationObserver back_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_load_observer.Wait();
  }

  {
    // Go forward in the iframe.
    TestNavigationObserver forward_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoForward();
    forward_load_observer.Wait();
  }

  GURL url3(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_iframe.html"));
  {
    // location.replace() to cause an inert commit.
    TestNavigationObserver replace_load_observer(shell()->web_contents());
    std::string script = "location.replace('" + url3.spec() + "')";
    EXPECT_TRUE(content::ExecuteScript(root->current_frame_host(), script));
    replace_load_observer.Wait();
  }

  {
    // Go back to url2.
    TestNavigationObserver back_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_load_observer.Wait();

    // Make sure the URL is correct for both the entry and the main frame, and
    // that the process hasn't been killed for showing a spoof.
    EXPECT_TRUE(root->current_frame_host()->IsRenderFrameLive());
    EXPECT_EQ(url2, shell()->web_contents()->GetLastCommittedURL());
    EXPECT_EQ(url2, root->current_url());
  }

  {
    // Go back to reset main frame entirely.
    TestNavigationObserver back_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_load_observer.Wait();
    EXPECT_EQ(url1, shell()->web_contents()->GetLastCommittedURL());
    EXPECT_EQ(url1, root->current_url());
  }

  {
    // Go forward.
    TestNavigationObserver back_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoForward();
    back_load_observer.Wait();
    EXPECT_EQ(url2, shell()->web_contents()->GetLastCommittedURL());
    EXPECT_EQ(url2, root->current_url());
  }

  {
    // Go forward to the replaced URL.
    TestNavigationObserver forward_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoForward();
    forward_load_observer.Wait();

    // Make sure the URL is correct for both the entry and the main frame, and
    // that the process hasn't been killed for showing a spoof.
    EXPECT_TRUE(root->current_frame_host()->IsRenderFrameLive());
    EXPECT_EQ(url3, shell()->web_contents()->GetLastCommittedURL());
    EXPECT_EQ(url3, root->current_url());
  }
}

// Ensure the renderer process does not get killed if the main frame URL's path
// changes when going back in a subframe, since this is currently possible after
// a replaceState in the main frame (thanks to https://crbug.com/373041).
// See https:///crbug.com/486916.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       SubframeBackFromReplaceState) {
  // Start at a page with a real iframe.
  GURL url1(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_data_iframe.html"));
  NavigateToURL(shell(), url1);

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  ASSERT_EQ(1U, root->child_count());
  ASSERT_NE(nullptr, root->child_at(0));

  {
    // Navigate in the iframe.
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    GURL frame_url(embedded_test_server()->GetURL(
        "/navigation_controller/simple_page_2.html"));
    NavigateFrameToURL(root->child_at(0), frame_url);
    capturer.Wait();
    EXPECT_EQ(NAVIGATION_TYPE_NEW_SUBFRAME, capturer.details().type);
  }

  {
    // history.replaceState().
    FrameNavigateParamsCapturer capturer(root);
    std::string script =
        "history.replaceState({}, 'replaced', 'replaced')";
    EXPECT_TRUE(content::ExecuteScript(root->current_frame_host(), script));
    capturer.Wait();
  }

  {
    // Go back in the iframe.
    TestNavigationObserver back_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_load_observer.Wait();
  }

  // For now, we expect the main frame's URL to revert.  This won't happen once
  // https://crbug.com/373041 is fixed.
  EXPECT_EQ(url1, shell()->web_contents()->GetLastCommittedURL());

  // Make sure the renderer process has not been killed.
  EXPECT_TRUE(root->current_frame_host()->IsRenderFrameLive());
}

namespace {

class FailureWatcher : public WebContentsObserver {
 public:
  // Observes failure for the specified |node|.
  explicit FailureWatcher(FrameTreeNode* node)
      : WebContentsObserver(
            node->current_frame_host()->delegate()->GetAsWebContents()),
        frame_tree_node_id_(node->frame_tree_node_id()),
        message_loop_runner_(new MessageLoopRunner) {}

  void Wait() {
    message_loop_runner_->Run();
  }

 private:
  void DidFailLoad(RenderFrameHost* render_frame_host,
                   const GURL& validated_url,
                   int error_code,
                   const base::string16& error_description,
                   bool was_ignored_by_handler) override {
    RenderFrameHostImpl* rfh =
        static_cast<RenderFrameHostImpl*>(render_frame_host);
    if (rfh->frame_tree_node()->frame_tree_node_id() != frame_tree_node_id_)
      return;

    message_loop_runner_->Quit();
  }

  void DidFailProvisionalLoad(
      RenderFrameHost* render_frame_host,
      const GURL& validated_url,
      int error_code,
      const base::string16& error_description,
      bool was_ignored_by_handler) override {
    RenderFrameHostImpl* rfh =
        static_cast<RenderFrameHostImpl*>(render_frame_host);
    if (rfh->frame_tree_node()->frame_tree_node_id() != frame_tree_node_id_)
      return;

    message_loop_runner_->Quit();
  }

  // The id of the FrameTreeNode whose navigations to observe.
  int frame_tree_node_id_;

  // The MessageLoopRunner used to spin the message loop.
  scoped_refptr<MessageLoopRunner> message_loop_runner_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       StopCausesFailureDespiteJavaScriptURL) {
  NavigationControllerImpl& controller =
      static_cast<NavigationControllerImpl&>(
          shell()->web_contents()->GetController());

  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell()->web_contents())->
          GetFrameTree()->root();

  // Start with a normal page.
  GURL url1(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url1));

  // Have the user decide to go to a different page which is very slow.
  GURL url2(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_2.html"));
  NavigationStallDelegate stall_delegate(url2);
  ResourceDispatcherHost::Get()->SetDelegate(&stall_delegate);
  controller.LoadURL(url2, Referrer(), ui::PAGE_TRANSITION_LINK, std::string());

  // That should be the pending entry.
  NavigationEntryImpl* entry = controller.GetPendingEntry();
  ASSERT_NE(nullptr, entry);
  EXPECT_EQ(url2, entry->GetURL());

  // Loading a JavaScript URL shouldn't affect the ability to stop.
  {
    FailureWatcher watcher(root);
    GURL js("javascript:(function(){})()");
    controller.LoadURL(js, Referrer(), ui::PAGE_TRANSITION_LINK, std::string());
    // This LoadURL ends up purging the pending entry, which is why this is
    // tricky.
    EXPECT_EQ(nullptr, controller.GetPendingEntry());
    shell()->web_contents()->Stop();
    watcher.Wait();
  }

  ResourceDispatcherHost::Get()->SetDelegate(nullptr);
}

namespace {
class RenderProcessKilledObserver : public WebContentsObserver {
 public:
  RenderProcessKilledObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}
  ~RenderProcessKilledObserver() override {}

  void RenderProcessGone(base::TerminationStatus status) override {
    CHECK_NE(status,
             base::TerminationStatus::TERMINATION_STATUS_PROCESS_WAS_KILLED);
  }
};
}

// This tests a race in ReloadOriginalRequest, where a cross-origin reload was
// causing an in-flight replaceState to look like a cross-origin navigation,
// even though it's in-page.  (The reload should not modify the underlying last
// committed entry.)  Not crashing means that the test is successful.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest, ReloadOriginalRequest) {
  GURL original_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  NavigateToURL(shell(), original_url);
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  RenderProcessKilledObserver kill_observer(shell()->web_contents());

  // Redirect so that we can use ReloadOriginalRequest.
  GURL redirect_url(embedded_test_server()->GetURL(
      "foo.com", "/navigation_controller/simple_page_1.html"));
  {
    std::string script = "location.replace('" + redirect_url.spec() + "');";
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_TRUE(ExecuteScript(shell()->web_contents(), script));
    capturer.Wait();
    EXPECT_EQ(ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_CLIENT_REDIRECT,
              capturer.params().transition);
    EXPECT_EQ(NAVIGATION_TYPE_EXISTING_PAGE, capturer.details().type);
  }

  // Modify an entry in the session history and reload the original request.
  {
    // We first send a replaceState() to the renderer, which will cause the
    // renderer to send back a DidCommitProvisionalLoad. Immediately after,
    // we send a ReloadOriginalRequest (which in this case is a different
    // origin) and will also cause the renderer to commit the frame. In the
    // end we verify that both navigations committed and that the URLs are
    // correct.
    std::string script = "history.replaceState({}, '', 'foo');";
    root->render_manager()
        ->current_frame_host()
        ->ExecuteJavaScriptWithUserGestureForTests(base::UTF8ToUTF16(script));
    EXPECT_FALSE(shell()->web_contents()->IsLoading());
    shell()->web_contents()->GetController().ReloadOriginalRequestURL(false);
    EXPECT_TRUE(shell()->web_contents()->IsLoading());
    EXPECT_EQ(redirect_url, shell()->web_contents()->GetLastCommittedURL());

    // Wait until there's no more navigations.
    GURL modified_url(embedded_test_server()->GetURL(
        "foo.com", "/navigation_controller/foo"));
    FrameNavigateParamsCapturer capturer(root);
    capturer.set_wait_for_load(false);
    capturer.set_navigations_remaining(2);
    capturer.Wait();
    EXPECT_EQ(2U, capturer.all_details().size());
    EXPECT_EQ(modified_url, capturer.all_params()[0].url);
    EXPECT_EQ(original_url, capturer.all_params()[1].url);
    EXPECT_EQ(original_url, shell()->web_contents()->GetLastCommittedURL());
  }

  // Make sure the renderer is still alive.
  EXPECT_TRUE(
      ExecuteScript(shell()->web_contents(), "console.log('Success');"));
}

}  // namespace content
