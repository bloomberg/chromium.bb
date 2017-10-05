// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/pattern.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view.h"
#include "content/common/frame_messages.h"
#include "content/common/site_isolation_policy.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/load_notification_details.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_resource_dispatcher_host_delegate.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

void ResizeWebContentsView(Shell* shell, const gfx::Size& size,
                           bool set_start_page) {
  // Shell::SizeTo is not implemented on Aura; WebContentsView::SizeContents
  // works on Win and ChromeOS but not Linux - we need to resize the shell
  // window on Linux because if we don't, the next layout of the unchanged shell
  // window will resize WebContentsView back to the previous size.
  // SizeContents is a hack and should not be relied on.
#if defined(OS_MACOSX)
  shell->SizeTo(size);
  // If |set_start_page| is true, start with blank page to make sure resize
  // takes effect.
  if (set_start_page)
    NavigateToURL(shell, GURL("about://blank"));
#else
  static_cast<WebContentsImpl*>(shell->web_contents())->GetView()->
      SizeContents(size);
#endif  // defined(OS_MACOSX)
}

class WebContentsImplBrowserTest : public ContentBrowserTest {
 public:
  WebContentsImplBrowserTest() {}
  void SetUp() override {
    RenderWidgetHostImpl::DisableResizeAckCheckForTesting();
    ContentBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    // Setup the server to allow serving separate sites, so we can perform
    // cross-process navigation.
    host_resolver()->AddRule("*", "127.0.0.1");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebContentsImplBrowserTest);
};

// Keeps track of data from LoadNotificationDetails so we can later verify that
// they are correct, after the LoadNotificationDetails object is deleted.
class LoadStopNotificationObserver : public WindowedNotificationObserver {
 public:
  explicit LoadStopNotificationObserver(NavigationController* controller)
      : WindowedNotificationObserver(NOTIFICATION_LOAD_STOP,
                                     Source<NavigationController>(controller)),
        session_index_(-1),
        controller_(NULL) {}
  void Observe(int type,
               const NotificationSource& source,
               const NotificationDetails& details) override {
    if (type == NOTIFICATION_LOAD_STOP) {
      const Details<LoadNotificationDetails> load_details(details);
      url_ = load_details->url;
      session_index_ = load_details->session_index;
      controller_ = load_details->controller;
    }
    WindowedNotificationObserver::Observe(type, source, details);
  }

  GURL url_;
  int session_index_;
  NavigationController* controller_;
};

// Starts a new navigation as soon as the current one commits, but does not
// wait for it to complete.  This allows us to observe DidStopLoading while
// a pending entry is present.
class NavigateOnCommitObserver : public WebContentsObserver {
 public:
  NavigateOnCommitObserver(Shell* shell, GURL url)
      : WebContentsObserver(shell->web_contents()),
        shell_(shell),
        url_(url),
        done_(false) {
  }

  // WebContentsObserver:
  void NavigationEntryCommitted(
      const LoadCommittedDetails& load_details) override {
    if (!done_) {
      done_ = true;
      shell_->LoadURL(url_);

      // There should be a pending entry.
      CHECK(shell_->web_contents()->GetController().GetPendingEntry());

      // Now that there is a pending entry, stop the load.
      shell_->Stop();
    }
  }

  Shell* shell_;
  GURL url_;
  bool done_;
};

class RenderViewSizeDelegate : public WebContentsDelegate {
 public:
  void set_size_insets(const gfx::Size& size_insets) {
    size_insets_ = size_insets;
  }

  // WebContentsDelegate:
  gfx::Size GetSizeForNewRenderView(WebContents* web_contents) const override {
    gfx::Size size(web_contents->GetContainerBounds().size());
    size.Enlarge(size_insets_.width(), size_insets_.height());
    return size;
  }

 private:
  gfx::Size size_insets_;
};

class RenderViewSizeObserver : public WebContentsObserver {
 public:
  RenderViewSizeObserver(Shell* shell, const gfx::Size& wcv_new_size)
      : WebContentsObserver(shell->web_contents()),
        shell_(shell),
        wcv_new_size_(wcv_new_size) {
  }

  // WebContentsObserver:
  void RenderFrameCreated(RenderFrameHost* rfh) override {
    if (!rfh->GetParent())
      rwhv_create_size_ = rfh->GetView()->GetViewBounds().size();
  }

  void DidStartNavigation(NavigationHandle* navigation_handle) override {
    ResizeWebContentsView(shell_, wcv_new_size_, false);
  }

  gfx::Size rwhv_create_size() const { return rwhv_create_size_; }

 private:
  Shell* shell_;  // Weak ptr.
  gfx::Size wcv_new_size_;
  gfx::Size rwhv_create_size_;
};

class LoadingStateChangedDelegate : public WebContentsDelegate {
 public:
  LoadingStateChangedDelegate()
      : loadingStateChangedCount_(0)
      , loadingStateToDifferentDocumentCount_(0) {
  }

  // WebContentsDelegate:
  void LoadingStateChanged(WebContents* contents,
                           bool to_different_document) override {
      loadingStateChangedCount_++;
      if (to_different_document)
        loadingStateToDifferentDocumentCount_++;
  }

  int loadingStateChangedCount() const { return loadingStateChangedCount_; }
  int loadingStateToDifferentDocumentCount() const {
    return loadingStateToDifferentDocumentCount_;
  }

 private:
  int loadingStateChangedCount_;
  int loadingStateToDifferentDocumentCount_;
};

// Test that DidStopLoading includes the correct URL in the details.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, DidStopLoadingDetails) {
  ASSERT_TRUE(embedded_test_server()->Start());

  LoadStopNotificationObserver load_observer(
      &shell()->web_contents()->GetController());
  NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html"));
  load_observer.Wait();

  EXPECT_EQ("/title1.html", load_observer.url_.path());
  EXPECT_EQ(0, load_observer.session_index_);
  EXPECT_EQ(&shell()->web_contents()->GetController(),
            load_observer.controller_);
}

// Test that DidStopLoading includes the correct URL in the details when a
// pending entry is present.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       DidStopLoadingDetailsWithPending) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // TODO(clamy): Add a cross-process navigation case as well once
  // crbug.com/581024 is fixed.
  GURL url1 = embedded_test_server()->GetURL("/title1.html");
  GURL url2 = embedded_test_server()->GetURL("/title2.html");

  // Listen for the first load to stop.
  LoadStopNotificationObserver load_observer(
      &shell()->web_contents()->GetController());
  // Start a new pending navigation as soon as the first load commits.
  // We will hear a DidStopLoading from the first load as the new load
  // is started.
  NavigateOnCommitObserver commit_observer(shell(), url2);
  NavigateToURL(shell(), url1);
  load_observer.Wait();

  EXPECT_EQ(url1, load_observer.url_);
  EXPECT_EQ(0, load_observer.session_index_);
  EXPECT_EQ(&shell()->web_contents()->GetController(),
            load_observer.controller_);
}

// Test that a renderer-initiated navigation to an invalid URL does not leave
// around a pending entry that could be used in a URL spoof.  We test this in
// a browser test because our unit test framework incorrectly calls
// DidStartProvisionalLoadForFrame for in-page navigations.
// See http://crbug.com/280512.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       ClearNonVisiblePendingOnFail) {
  ASSERT_TRUE(embedded_test_server()->Start());

  NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html"));

  // Navigate to an invalid URL and make sure it doesn't leave a pending entry.
  LoadStopNotificationObserver load_observer1(
      &shell()->web_contents()->GetController());
  ASSERT_TRUE(
      ExecuteScript(shell(), "window.location.href=\"nonexistent:12121\";"));
  load_observer1.Wait();
  EXPECT_FALSE(shell()->web_contents()->GetController().GetPendingEntry());

  LoadStopNotificationObserver load_observer2(
      &shell()->web_contents()->GetController());
  ASSERT_TRUE(ExecuteScript(shell(), "window.location.href=\"#foo\";"));
  load_observer2.Wait();
  EXPECT_EQ(embedded_test_server()->GetURL("/title1.html#foo"),
            shell()->web_contents()->GetVisibleURL());
}

// Crashes under ThreadSanitizer, http://crbug.com/356758.
#if defined(OS_WIN) || defined(OS_ANDROID) \
    || defined(THREAD_SANITIZER)
#define MAYBE_GetSizeForNewRenderView DISABLED_GetSizeForNewRenderView
#else
#define MAYBE_GetSizeForNewRenderView GetSizeForNewRenderView
#endif
// Test that RenderViewHost is created and updated at the size specified by
// WebContentsDelegate::GetSizeForNewRenderView().
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       MAYBE_GetSizeForNewRenderView) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Create a new server with a different site.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(https_server.Start());

  std::unique_ptr<RenderViewSizeDelegate> delegate(
      new RenderViewSizeDelegate());
  shell()->web_contents()->SetDelegate(delegate.get());
  ASSERT_TRUE(shell()->web_contents()->GetDelegate() == delegate.get());

  // When no size is set, RenderWidgetHostView adopts the size of
  // WebContentsView.
  NavigateToURL(shell(), embedded_test_server()->GetURL("/title2.html"));
  EXPECT_EQ(shell()->web_contents()->GetContainerBounds().size(),
            shell()->web_contents()->GetRenderWidgetHostView()->GetViewBounds().
                size());

  // When a size is set, RenderWidgetHostView and WebContentsView honor this
  // size.
  gfx::Size size(300, 300);
  gfx::Size size_insets(10, 15);
  ResizeWebContentsView(shell(), size, true);
  delegate->set_size_insets(size_insets);
  NavigateToURL(shell(), https_server.GetURL("/"));
  size.Enlarge(size_insets.width(), size_insets.height());
  EXPECT_EQ(size,
            shell()->web_contents()->GetRenderWidgetHostView()->GetViewBounds().
                size());
  // The web_contents size is set by the embedder, and should not depend on the
  // rwhv size. The behavior is correct on OSX, but incorrect on other
  // platforms.
  gfx::Size exp_wcv_size(300, 300);
#if !defined(OS_MACOSX)
  exp_wcv_size.Enlarge(size_insets.width(), size_insets.height());
#endif

  EXPECT_EQ(exp_wcv_size,
            shell()->web_contents()->GetContainerBounds().size());

  // If WebContentsView is resized after RenderWidgetHostView is created but
  // before pending navigation entry is committed, both RenderWidgetHostView and
  // WebContentsView use the new size of WebContentsView.
  gfx::Size init_size(200, 200);
  gfx::Size new_size(100, 100);
  size_insets = gfx::Size(20, 30);
  ResizeWebContentsView(shell(), init_size, true);
  delegate->set_size_insets(size_insets);
  RenderViewSizeObserver observer(shell(), new_size);
  NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html"));
  // RenderWidgetHostView is created at specified size.
  init_size.Enlarge(size_insets.width(), size_insets.height());
  EXPECT_EQ(init_size, observer.rwhv_create_size());

// Once again, the behavior is correct on OSX. The embedder explicitly sets
// the size to (100,100) during navigation. Both the wcv and the rwhv should
// take on that size.
#if !defined(OS_MACOSX)
  new_size.Enlarge(size_insets.width(), size_insets.height());
#endif
  gfx::Size actual_size = shell()->web_contents()->GetRenderWidgetHostView()->
      GetViewBounds().size();

  EXPECT_EQ(new_size, actual_size);
  EXPECT_EQ(new_size, shell()->web_contents()->GetContainerBounds().size());
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, SetTitleOnUnload) {
  GURL url(
      "data:text/html,"
      "<title>A</title>"
      "<body onunload=\"document.title = 'B'\"></body>");
  NavigateToURL(shell(), url);
  ASSERT_EQ(1, shell()->web_contents()->GetController().GetEntryCount());
  NavigationEntryImpl* entry1 = NavigationEntryImpl::FromNavigationEntry(
      shell()->web_contents()->GetController().GetLastCommittedEntry());
  SiteInstance* site_instance1 = entry1->site_instance();
  EXPECT_EQ(base::ASCIIToUTF16("A"), entry1->GetTitle());

  // Force a process switch by going to a privileged page.
  GURL web_ui_page(std::string(kChromeUIScheme) + "://" +
                   std::string(kChromeUIGpuHost));
  NavigateToURL(shell(), web_ui_page);
  NavigationEntryImpl* entry2 = NavigationEntryImpl::FromNavigationEntry(
      shell()->web_contents()->GetController().GetLastCommittedEntry());
  SiteInstance* site_instance2 = entry2->site_instance();
  EXPECT_NE(site_instance1, site_instance2);

  EXPECT_EQ(2, shell()->web_contents()->GetController().GetEntryCount());
  EXPECT_EQ(base::ASCIIToUTF16("B"), entry1->GetTitle());
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, OpenURLSubframe) {
  // Navigate to a page with frames and grab a subframe's FrameTreeNode ID.
  ASSERT_TRUE(embedded_test_server()->Start());
  NavigateToURL(shell(),
                embedded_test_server()->GetURL("/frame_tree/top.html"));
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root = wc->GetFrameTree()->root();
  ASSERT_EQ(3UL, root->child_count());
  int frame_tree_node_id = root->child_at(0)->frame_tree_node_id();
  EXPECT_NE(-1, frame_tree_node_id);

  // Navigate with the subframe's FrameTreeNode ID.
  const GURL url(embedded_test_server()->GetURL("/title1.html"));
  OpenURLParams params(url, Referrer(), frame_tree_node_id,
                       WindowOpenDisposition::CURRENT_TAB,
                       ui::PAGE_TRANSITION_LINK, true);
  shell()->web_contents()->OpenURL(params);

  // Make sure the NavigationEntry ends up with the FrameTreeNode ID.
  NavigationController* controller = &shell()->web_contents()->GetController();
  EXPECT_TRUE(controller->GetPendingEntry());
  EXPECT_EQ(frame_tree_node_id,
            NavigationEntryImpl::FromNavigationEntry(
                controller->GetPendingEntry())->frame_tree_node_id());
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       AppendingFrameInWebUIDoesNotCrash) {
  const GURL kWebUIUrl("chrome://tracing");
  const char kJSCodeForAppendingFrame[] =
      "document.body.appendChild(document.createElement('iframe'));";

  NavigateToURL(shell(), kWebUIUrl);

  bool js_executed = content::ExecuteScript(shell(), kJSCodeForAppendingFrame);
  EXPECT_TRUE(js_executed);
}

// Observer class to track the creation of RenderFrameHost objects. It is used
// in subsequent tests.
class RenderFrameCreatedObserver : public WebContentsObserver {
 public:
  explicit RenderFrameCreatedObserver(Shell* shell)
      : WebContentsObserver(shell->web_contents()), last_rfh_(NULL) {}

  void RenderFrameCreated(RenderFrameHost* render_frame_host) override {
    last_rfh_ = render_frame_host;
  }

  RenderFrameHost* last_rfh() const { return last_rfh_; }

 private:
  RenderFrameHost* last_rfh_;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameCreatedObserver);
};

// Test that creation of new RenderFrameHost objects sends the correct object
// to the WebContentObservers. See http://crbug.com/347339.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       RenderFrameCreatedCorrectProcessForObservers) {
  static const char kFooCom[] = "foo.com";
  GURL::Replacements replace_host;
  net::HostPortPair foo_host_port;
  GURL cross_site_url;

  ASSERT_TRUE(embedded_test_server()->Start());

  foo_host_port = embedded_test_server()->host_port_pair();
  foo_host_port.set_host(kFooCom);

  GURL initial_url(embedded_test_server()->GetURL("/title1.html"));

  cross_site_url = embedded_test_server()->GetURL("/title2.html");
  replace_host.SetHostStr(kFooCom);
  cross_site_url = cross_site_url.ReplaceComponents(replace_host);

  // Navigate to the initial URL and capture the RenderFrameHost for later
  // comparison.
  NavigateToURL(shell(), initial_url);
  RenderFrameHost* orig_rfh = shell()->web_contents()->GetMainFrame();

  // Install the observer and navigate cross-site.
  RenderFrameCreatedObserver observer(shell());
  NavigateToURL(shell(), cross_site_url);

  // The observer should've seen a RenderFrameCreated call for the new frame
  // and not the old one.
  EXPECT_NE(observer.last_rfh(), orig_rfh);
  EXPECT_EQ(observer.last_rfh(), shell()->web_contents()->GetMainFrame());
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       LoadingStateChangedForSameDocumentNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  std::unique_ptr<LoadingStateChangedDelegate> delegate(
      new LoadingStateChangedDelegate());
  shell()->web_contents()->SetDelegate(delegate.get());

  LoadStopNotificationObserver load_observer(
      &shell()->web_contents()->GetController());
  TitleWatcher title_watcher(shell()->web_contents(),
                             base::ASCIIToUTF16("pushState"));
  NavigateToURL(shell(), embedded_test_server()->GetURL("/push_state.html"));
  load_observer.Wait();
  base::string16 title = title_watcher.WaitAndGetTitle();
  ASSERT_EQ(title, base::ASCIIToUTF16("pushState"));

  // LoadingStateChanged should be called 5 times: start and stop for the
  // initial load of push_state.html, once for the switch from
  // IsWaitingForResponse() to !IsWaitingForResponse(), and start and stop for
  // the "navigation" triggered by history.pushState(). However, the start
  // notification for the history.pushState() navigation should set
  // to_different_document to false.
  EXPECT_EQ("pushState", shell()->web_contents()->GetLastCommittedURL().ref());
  EXPECT_EQ(5, delegate->loadingStateChangedCount());
  EXPECT_EQ(4, delegate->loadingStateToDifferentDocumentCount());
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       RenderViewCreatedForChildWindow) {
  ASSERT_TRUE(embedded_test_server()->Start());

  NavigateToURL(shell(),
                embedded_test_server()->GetURL("/title1.html"));

  WebContentsAddedObserver new_web_contents_observer;
  ASSERT_TRUE(ExecuteScript(shell(),
                            "var a = document.createElement('a');"
                            "a.href='./title2.html';"
                            "a.target = '_blank';"
                            "document.body.appendChild(a);"
                            "a.click();"));
  WebContents* new_web_contents = new_web_contents_observer.GetWebContents();
  WaitForLoadStop(new_web_contents);
  EXPECT_TRUE(new_web_contents_observer.RenderViewCreatedCalled());
}

namespace {

class DidGetResourceResponseStartObserver : public WebContentsObserver {
 public:
  explicit DidGetResourceResponseStartObserver(Shell* shell)
      : WebContentsObserver(shell->web_contents()), shell_(shell) {
    shell->web_contents()->SetDelegate(&delegate_);
    EXPECT_FALSE(shell->web_contents()->IsWaitingForResponse());
    EXPECT_FALSE(shell->web_contents()->IsLoading());
  }

  ~DidGetResourceResponseStartObserver() override {}

  void DidGetResourceResponseStart(
      const ResourceRequestDetails& details) override {
    EXPECT_FALSE(shell_->web_contents()->IsWaitingForResponse());
    EXPECT_TRUE(shell_->web_contents()->IsLoading());
    EXPECT_GT(delegate_.loadingStateChangedCount(), 0);
    ++resource_response_start_count_;
  }

  int resource_response_start_count() const {
    return resource_response_start_count_;
  }

 private:
  Shell* shell_;
  LoadingStateChangedDelegate delegate_;
  int resource_response_start_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(DidGetResourceResponseStartObserver);
};

}  // namespace

// Makes sure that the WebContents is no longer marked as waiting for a response
// after DidGetResourceResponseStart() is called.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       DidGetResourceResponseStartUpdatesWaitingState) {
  DidGetResourceResponseStartObserver observer(shell());

  ASSERT_TRUE(embedded_test_server()->Start());
  LoadStopNotificationObserver load_observer(
      &shell()->web_contents()->GetController());
  NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html"));
  load_observer.Wait();
  EXPECT_GT(observer.resource_response_start_count(), 0);
}

struct LoadProgressDelegateAndObserver : public WebContentsDelegate,
                                         public WebContentsObserver {
  explicit LoadProgressDelegateAndObserver(Shell* shell)
      : WebContentsObserver(shell->web_contents()),
        did_start_loading(false),
        did_stop_loading(false) {
    web_contents()->SetDelegate(this);
  }

  // WebContentsDelegate:
  void LoadProgressChanged(WebContents* source, double progress) override {
    EXPECT_TRUE(did_start_loading);
    EXPECT_FALSE(did_stop_loading);
    progresses.push_back(progress);
  }

  // WebContentsObserver:
  void DidStartLoading() override {
    EXPECT_FALSE(did_start_loading);
    EXPECT_EQ(0U, progresses.size());
    EXPECT_FALSE(did_stop_loading);
    did_start_loading = true;
  }

  void DidStopLoading() override {
    EXPECT_TRUE(did_start_loading);
    EXPECT_GE(progresses.size(), 1U);
    EXPECT_FALSE(did_stop_loading);
    did_stop_loading = true;
  }

  bool did_start_loading;
  std::vector<double> progresses;
  bool did_stop_loading;
};

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, LoadProgress) {
  ASSERT_TRUE(embedded_test_server()->Start());
  std::unique_ptr<LoadProgressDelegateAndObserver> delegate(
      new LoadProgressDelegateAndObserver(shell()));

  NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html"));

  const std::vector<double>& progresses = delegate->progresses;
  // All updates should be in order ...
  if (std::adjacent_find(progresses.begin(),
                         progresses.end(),
                         std::greater<double>()) != progresses.end()) {
    ADD_FAILURE() << "Progress values should be in order: "
                  << ::testing::PrintToString(progresses);
  }

  // ... and the last one should be 1.0, meaning complete.
  ASSERT_GE(progresses.size(), 1U)
      << "There should be at least one progress update";
  EXPECT_EQ(1.0, *progresses.rbegin());
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, LoadProgressWithFrames) {
  ASSERT_TRUE(embedded_test_server()->Start());
  std::unique_ptr<LoadProgressDelegateAndObserver> delegate(
      new LoadProgressDelegateAndObserver(shell()));

  NavigateToURL(shell(),
                embedded_test_server()->GetURL("/frame_tree/top.html"));

  const std::vector<double>& progresses = delegate->progresses;
  // All updates should be in order ...
  if (std::adjacent_find(progresses.begin(),
                         progresses.end(),
                         std::greater<double>()) != progresses.end()) {
    ADD_FAILURE() << "Progress values should be in order: "
                  << ::testing::PrintToString(progresses);
  }

  // ... and the last one should be 1.0, meaning complete.
  ASSERT_GE(progresses.size(), 1U)
      << "There should be at least one progress update";
  EXPECT_EQ(1.0, *progresses.rbegin());
}

// Ensure that a new navigation that interrupts a pending one will still fire
// a DidStopLoading.  See http://crbug.com/429399.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       LoadProgressAfterInterruptedNav) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Start at a real page.
  NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html"));

  // Simulate a navigation that has not completed.
  const GURL kURL2 = embedded_test_server()->GetURL("/title2.html");
  NavigationStallDelegate stall_delegate(kURL2);
  ResourceDispatcherHost::Get()->SetDelegate(&stall_delegate);
  std::unique_ptr<LoadProgressDelegateAndObserver> delegate(
      new LoadProgressDelegateAndObserver(shell()));
  shell()->LoadURL(kURL2);
  EXPECT_TRUE(delegate->did_start_loading);
  EXPECT_FALSE(delegate->did_stop_loading);

  // Also simulate a DidChangeLoadProgress, but not a DidStopLoading.
  RenderFrameHostImpl* main_frame = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetMainFrame());
  FrameHostMsg_DidChangeLoadProgress progress_msg(main_frame->GetRoutingID(),
                                                  1.0);
  main_frame->OnMessageReceived(progress_msg);
  EXPECT_TRUE(delegate->did_start_loading);
  EXPECT_FALSE(delegate->did_stop_loading);

  // Now interrupt with a new cross-process navigation.
  TestNavigationObserver tab_observer(shell()->web_contents(), 1);
  GURL url(embedded_test_server()->GetURL("foo.com", "/title2.html"));
  shell()->LoadURL(url);
  tab_observer.Wait();
  EXPECT_EQ(url, shell()->web_contents()->GetLastCommittedURL());

  // We should have gotten to DidStopLoading.
  EXPECT_TRUE(delegate->did_stop_loading);

  ResourceDispatcherHost::Get()->SetDelegate(nullptr);
}

struct FirstVisuallyNonEmptyPaintObserver : public WebContentsObserver {
  explicit FirstVisuallyNonEmptyPaintObserver(Shell* shell)
      : WebContentsObserver(shell->web_contents()),
        did_fist_visually_non_empty_paint_(false) {}

  void DidFirstVisuallyNonEmptyPaint() override {
    did_fist_visually_non_empty_paint_ = true;
    on_did_first_visually_non_empty_paint_.Run();
  }

  void WaitForDidFirstVisuallyNonEmptyPaint() {
    if (did_fist_visually_non_empty_paint_)
      return;
    base::RunLoop run_loop;
    on_did_first_visually_non_empty_paint_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  base::Closure on_did_first_visually_non_empty_paint_;
  bool did_fist_visually_non_empty_paint_;
};

// See: http://crbug.com/395664
#if defined(OS_ANDROID)
#define MAYBE_FirstVisuallyNonEmptyPaint DISABLED_FirstVisuallyNonEmptyPaint
#else
// http://crbug.com/398471
#define MAYBE_FirstVisuallyNonEmptyPaint DISABLED_FirstVisuallyNonEmptyPaint
#endif
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       MAYBE_FirstVisuallyNonEmptyPaint) {
  ASSERT_TRUE(embedded_test_server()->Start());
  std::unique_ptr<FirstVisuallyNonEmptyPaintObserver> observer(
      new FirstVisuallyNonEmptyPaintObserver(shell()));

  NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html"));

  observer->WaitForDidFirstVisuallyNonEmptyPaint();
  ASSERT_TRUE(observer->did_fist_visually_non_empty_paint_);
}

namespace {

class WebDisplayModeDelegate : public WebContentsDelegate {
 public:
  explicit WebDisplayModeDelegate(blink::WebDisplayMode mode) : mode_(mode) { }
  ~WebDisplayModeDelegate() override { }

  blink::WebDisplayMode GetDisplayMode(
      const WebContents* source) const override { return mode_; }
  void set_mode(blink::WebDisplayMode mode) { mode_ = mode; }
 private:
  blink::WebDisplayMode mode_;

  DISALLOW_COPY_AND_ASSIGN(WebDisplayModeDelegate);
};

}  // namespace

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, ChangeDisplayMode) {
  ASSERT_TRUE(embedded_test_server()->Start());
  WebDisplayModeDelegate delegate(blink::kWebDisplayModeMinimalUi);
  shell()->web_contents()->SetDelegate(&delegate);

  NavigateToURL(shell(), GURL("about://blank"));

  ASSERT_TRUE(ExecuteScript(shell(),
                            "document.title = "
                            " window.matchMedia('(display-mode:"
                            " minimal-ui)').matches"));
  EXPECT_EQ(base::ASCIIToUTF16("true"), shell()->web_contents()->GetTitle());

  delegate.set_mode(blink::kWebDisplayModeFullscreen);
  // Simulate widget is entering fullscreen (changing size is enough).
  shell()->web_contents()->GetRenderViewHost()->GetWidget()->WasResized();

  ASSERT_TRUE(ExecuteScript(shell(),
                            "document.title = "
                            " window.matchMedia('(display-mode:"
                            " fullscreen)').matches"));
  EXPECT_EQ(base::ASCIIToUTF16("true"), shell()->web_contents()->GetTitle());
}

// Observer class used to verify that WebContentsObservers are notified
// when the page scale factor changes.
// See WebContentsImplBrowserTest.ChangePageScale.
class MockPageScaleObserver : public WebContentsObserver {
 public:
  explicit MockPageScaleObserver(Shell* shell)
      : WebContentsObserver(shell->web_contents()),
        got_page_scale_update_(false) {
    // Once OnPageScaleFactorChanged is called, quit the run loop.
    ON_CALL(*this, OnPageScaleFactorChanged(::testing::_)).WillByDefault(
        ::testing::InvokeWithoutArgs(
            this, &MockPageScaleObserver::GotPageScaleUpdate));
  }

  MOCK_METHOD1(OnPageScaleFactorChanged, void(float page_scale_factor));

  void WaitForPageScaleUpdate() {
    if (!got_page_scale_update_) {
      base::RunLoop run_loop;
      on_page_scale_update_ = run_loop.QuitClosure();
      run_loop.Run();
    }
    got_page_scale_update_ = false;
  }

 private:
  void GotPageScaleUpdate() {
    got_page_scale_update_ = true;
    on_page_scale_update_.Run();
  }

  base::Closure on_page_scale_update_;
  bool got_page_scale_update_;
};

// When the page scale factor is set in the renderer it should send
// a notification to the browser so that WebContentsObservers are notified.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, ChangePageScale) {
  ASSERT_TRUE(embedded_test_server()->Start());
  NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html"));

  MockPageScaleObserver observer(shell());
  ::testing::InSequence expect_call_sequence;

  shell()->web_contents()->SetPageScale(1.5);
  EXPECT_CALL(observer, OnPageScaleFactorChanged(::testing::FloatEq(1.5)));
  observer.WaitForPageScaleUpdate();

  // Navigate to reset the page scale factor.
  shell()->LoadURL(embedded_test_server()->GetURL("/title2.html"));
  EXPECT_CALL(observer, OnPageScaleFactorChanged(::testing::_));
  observer.WaitForPageScaleUpdate();
}

// Test that a direct navigation to a view-source URL works.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, ViewSourceDirectNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kUrl(embedded_test_server()->GetURL("/simple_page.html"));
  const GURL kViewSourceURL(kViewSourceScheme + std::string(":") + kUrl.spec());
  NavigateToURL(shell(), kViewSourceURL);
  // Displayed view-source URLs don't include the scheme of the effective URL if
  // the effective URL is HTTP. (e.g. view-source:example.com is displayed
  // instead of view-source:http://example.com).
  EXPECT_EQ(base::ASCIIToUTF16(std::string("view-source:") + kUrl.host() + ":" +
                               kUrl.port() + kUrl.path()),
            shell()->web_contents()->GetTitle());
  EXPECT_TRUE(shell()
                  ->web_contents()
                  ->GetController()
                  .GetLastCommittedEntry()
                  ->IsViewSourceMode());
}

// Test that window.open to a view-source URL is blocked.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       ViewSourceWindowOpen_ShouldBeBlocked) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kUrl(embedded_test_server()->GetURL("/simple_page.html"));
  const GURL kViewSourceURL(kViewSourceScheme + std::string(":") + kUrl.spec());
  NavigateToURL(shell(), kUrl);

  ShellAddedObserver new_shell_observer;
  EXPECT_TRUE(ExecuteScript(shell()->web_contents(),
                            "window.open('" + kViewSourceURL.spec() + "');"));
  Shell* new_shell = new_shell_observer.GetShell();
  WaitForLoadStop(new_shell->web_contents());
  EXPECT_TRUE(new_shell->web_contents()->GetURL().spec().empty());
  // No navigation should commit.
  EXPECT_FALSE(
      new_shell->web_contents()->GetController().GetLastCommittedEntry());
}

// Test that a content initiated navigation to a view-source URL is blocked.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       ViewSourceRedirect_ShouldBeBlocked) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kUrl(embedded_test_server()->GetURL("/simple_page.html"));
  const GURL kViewSourceURL(kViewSourceScheme + std::string(":") + kUrl.spec());
  NavigateToURL(shell(), kUrl);

  std::unique_ptr<ConsoleObserverDelegate> console_delegate(
      new ConsoleObserverDelegate(
          shell()->web_contents(),
          "Not allowed to load local resource: view-source:*"));
  shell()->web_contents()->SetDelegate(console_delegate.get());

  EXPECT_TRUE(
      ExecuteScript(shell()->web_contents(),
                    "window.location = '" + kViewSourceURL.spec() + "';"));
  console_delegate->Wait();
  // Original page shouldn't navigate away.
  EXPECT_EQ(kUrl, shell()->web_contents()->GetURL());
  EXPECT_FALSE(shell()
                   ->web_contents()
                   ->GetController()
                   .GetLastCommittedEntry()
                   ->IsViewSourceMode());
}

// Test that view source mode for a webui page can be opened.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, ViewSourceWebUI) {
  const std::string kUrl =
      "view-source:chrome://" + std::string(kChromeUIGpuHost);
  const GURL kGURL(kUrl);
  NavigateToURL(shell(), kGURL);
  EXPECT_EQ(base::ASCIIToUTF16(kUrl), shell()->web_contents()->GetTitle());
  EXPECT_TRUE(shell()
                  ->web_contents()
                  ->GetController()
                  .GetLastCommittedEntry()
                  ->IsViewSourceMode());
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, NewNamedWindow) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url = embedded_test_server()->GetURL("/click-noreferrer-links.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  {
    ShellAddedObserver new_shell_observer;

    // Open a new, named window.
    EXPECT_TRUE(
        ExecuteScript(shell(), "window.open('about:blank','new_window');"));

    Shell* new_shell = new_shell_observer.GetShell();
    WaitForLoadStop(new_shell->web_contents());

    EXPECT_EQ("new_window",
              static_cast<WebContentsImpl*>(new_shell->web_contents())
                  ->GetFrameTree()->root()->frame_name());

    bool success = false;
    EXPECT_TRUE(ExecuteScriptAndExtractBool(
        new_shell,
        "window.domAutomationController.send(window.name == 'new_window');",
        &success));
    EXPECT_TRUE(success);
  }

  {
    ShellAddedObserver new_shell_observer;

    // Test clicking a target=foo link.
    bool success = false;
    EXPECT_TRUE(ExecuteScriptAndExtractBool(
        shell(),
        "window.domAutomationController.send(clickSameSiteTargetedLink());",
        &success));
    EXPECT_TRUE(success);

    Shell* new_shell = new_shell_observer.GetShell();
    WaitForLoadStop(new_shell->web_contents());

    EXPECT_EQ("foo",
              static_cast<WebContentsImpl*>(new_shell->web_contents())
                  ->GetFrameTree()->root()->frame_name());
  }
}

// Test that HasOriginalOpener() tracks provenance through closed WebContentses.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       HasOriginalOpenerTracksThroughClosedWebContents) {
  const GURL blank_url = GURL("about:blank");

  Shell* shell1 = shell();
  EXPECT_TRUE(NavigateToURL(shell1, blank_url));

  Shell* shell2 = OpenPopup(shell1, blank_url, "window2");
  Shell* shell3 = OpenPopup(shell2, blank_url, "window3");

  EXPECT_EQ(shell2->web_contents(),
            WebContents::FromRenderFrameHost(
                shell3->web_contents()->GetOriginalOpener()));
  EXPECT_EQ(shell1->web_contents(),
            WebContents::FromRenderFrameHost(
                shell2->web_contents()->GetOriginalOpener()));

  shell2->Close();

  EXPECT_EQ(shell1->web_contents(),
            WebContents::FromRenderFrameHost(
                shell3->web_contents()->GetOriginalOpener()));
}

// TODO(clamy): Make the test work on Windows and on Mac. On Mac and Windows,
// there seem to be an issue with the ShellJavascriptDialogManager.
// Flaky on all platforms: https://crbug.com/655628
// Test that if a BeforeUnload dialog is destroyed due to the commit of a
// cross-site navigation, it will not reset the loading state.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       DISABLED_NoResetOnBeforeUnloadCanceledOnCommit) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kStartURL(
      embedded_test_server()->GetURL("/hang_before_unload.html"));
  const GURL kCrossSiteURL(
      embedded_test_server()->GetURL("bar.com", "/title1.html"));

  // Navigate to a first web page with a BeforeUnload event listener.
  EXPECT_TRUE(NavigateToURL(shell(), kStartURL));

  // Start a cross-site navigation that will not commit for the moment.
  TestNavigationManager cross_site_delayer(shell()->web_contents(),
                                           kCrossSiteURL);
  shell()->LoadURL(kCrossSiteURL);
  EXPECT_TRUE(cross_site_delayer.WaitForRequestStart());

  // Click on a link in the page. This will show the BeforeUnload dialog.
  // Ensure the dialog is not dismissed, which will cause it to still be
  // present when the cross-site navigation later commits.
  // Note: the javascript function executed will not do the link click but
  // schedule it for afterwards. Since the BeforeUnload event is synchronous,
  // clicking on the link right away would cause the ExecuteScript to never
  // return.
  SetShouldProceedOnBeforeUnload(shell(), false);
  EXPECT_TRUE(ExecuteScript(shell(), "clickLinkSoon()"));
  WaitForAppModalDialog(shell());

  // Have the cross-site navigation commit. The main RenderFrameHost should
  // still be loading after that.
  cross_site_delayer.WaitForNavigationFinished();
  EXPECT_TRUE(shell()->web_contents()->IsLoading());
}

namespace {
void NavigateToDataURLAndCheckForTerminationDisabler(
    Shell* shell,
    const std::string& html,
    bool expect_onunload,
    bool expect_onbeforeunload) {
  NavigateToURL(shell, GURL("data:text/html," + html));
  RenderFrameHostImpl* rfh =
      static_cast<RenderFrameHostImpl*>(shell->web_contents()->GetMainFrame());
  EXPECT_EQ(expect_onunload,
            rfh->GetSuddenTerminationDisablerState(blink::kUnloadHandler));
  EXPECT_EQ(expect_onbeforeunload, rfh->GetSuddenTerminationDisablerState(
                                       blink::kBeforeUnloadHandler));
}
}  // namespace

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       SuddenTerminationDisablerNone) {
  const std::string NO_HANDLERS_HTML = "<html><body>foo</body></html>";
  NavigateToDataURLAndCheckForTerminationDisabler(shell(), NO_HANDLERS_HTML,
                                                  false, false);
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       SuddenTerminationDisablerOnUnload) {
  const std::string UNLOAD_HTML =
      "<html><body><script>window.onunload=function(e) {}</script>"
      "</body></html>";
  NavigateToDataURLAndCheckForTerminationDisabler(shell(), UNLOAD_HTML, true,
                                                  false);
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       SuddenTerminationDisablerOnBeforeUnload) {
  const std::string BEFORE_UNLOAD_HTML =
      "<html><body><script>window.onbeforeunload=function(e) {}</script>"
      "</body></html>";
  NavigateToDataURLAndCheckForTerminationDisabler(shell(), BEFORE_UNLOAD_HTML,
                                                  false, true);
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       SuddenTerminationDisablerOnUnloadAndBeforeUnload) {
  const std::string UNLOAD_AND_BEFORE_UNLOAD_HTML =
      "<html><body><script>window.onunload=function(e) {};"
      "window.onbeforeunload=function(e) {}</script>"
      "</body></html>";
  NavigateToDataURLAndCheckForTerminationDisabler(
      shell(), UNLOAD_AND_BEFORE_UNLOAD_HTML, true, true);
}

namespace {

class TestWCDelegateForDialogsAndFullscreen : public JavaScriptDialogManager,
                                              public WebContentsDelegate {
 public:
  TestWCDelegateForDialogsAndFullscreen()
      : is_fullscreen_(false), message_loop_runner_(new MessageLoopRunner) {}
  ~TestWCDelegateForDialogsAndFullscreen() override {}

  void Wait() {
    message_loop_runner_->Run();
    message_loop_runner_ = new MessageLoopRunner;
  }

  std::string last_message() { return last_message_; }

  // WebContentsDelegate

  JavaScriptDialogManager* GetJavaScriptDialogManager(
      WebContents* source) override {
    return this;
  }

  void EnterFullscreenModeForTab(WebContents* web_contents,
                                 const GURL& origin) override {
    is_fullscreen_ = true;
  }

  void ExitFullscreenModeForTab(WebContents*) override {
    is_fullscreen_ = false;
  }

  bool IsFullscreenForTabOrPending(
      const WebContents* web_contents) const override {
    return is_fullscreen_;
  }

  void AddNewContents(WebContents* source,
                      WebContents* new_contents,
                      WindowOpenDisposition disposition,
                      const gfx::Rect& initial_rect,
                      bool user_gesture,
                      bool* was_blocked) override {
    delete new_contents;
    message_loop_runner_->Quit();
  }

  // JavaScriptDialogManager

  void RunJavaScriptDialog(WebContents* web_contents,
                           const GURL& alerting_frame_url,
                           JavaScriptDialogType dialog_type,
                           const base::string16& message_text,
                           const base::string16& default_prompt_text,
                           DialogClosedCallback callback,
                           bool* did_suppress_message) override {
    last_message_ = base::UTF16ToUTF8(message_text);
    *did_suppress_message = true;

    message_loop_runner_->Quit();
  };

  void RunBeforeUnloadDialog(WebContents* web_contents,
                             bool is_reload,
                             DialogClosedCallback callback) override {
    std::move(callback).Run(true, base::string16());
    message_loop_runner_->Quit();
  }

  bool HandleJavaScriptDialog(WebContents* web_contents,
                              bool accept,
                              const base::string16* prompt_override) override {
    return true;
  }

  void CancelDialogs(WebContents* web_contents,
                     bool reset_state) override {}

 private:
  std::string last_message_;

  bool is_fullscreen_;

  // The MessageLoopRunner used to spin the message loop.
  scoped_refptr<MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(TestWCDelegateForDialogsAndFullscreen);
};

}  // namespace

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       JavaScriptDialogsInMainAndSubframes) {
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  TestWCDelegateForDialogsAndFullscreen test_delegate;
  wc->SetDelegate(&test_delegate);

  ASSERT_TRUE(embedded_test_server()->Start());

  NavigateToURL(shell(),
                embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(WaitForLoadStop(wc));

  FrameTreeNode* root = wc->GetFrameTree()->root();
  ASSERT_EQ(0U, root->child_count());

  std::string script =
      "var iframe = document.createElement('iframe');"
      "document.body.appendChild(iframe);";
  EXPECT_TRUE(content::ExecuteScript(root->current_frame_host(), script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  ASSERT_EQ(1U, root->child_count());
  FrameTreeNode* frame = root->child_at(0);
  ASSERT_NE(nullptr, frame);

  url::Replacements<char> clear_port;
  clear_port.ClearPort();

  // A dialog from the main frame.
  std::string alert_location = "alert(document.location)";
  EXPECT_TRUE(
      content::ExecuteScript(root->current_frame_host(), alert_location));
  test_delegate.Wait();
  EXPECT_EQ(GURL("http://a.com/title1.html"),
            GURL(test_delegate.last_message()).ReplaceComponents(clear_port));

  // A dialog from the subframe.
  EXPECT_TRUE(
      content::ExecuteScript(frame->current_frame_host(), alert_location));
  test_delegate.Wait();
  EXPECT_EQ("about:blank", test_delegate.last_message());

  // Navigate the subframe cross-site.
  NavigateFrameToURL(frame,
                     embedded_test_server()->GetURL("b.com", "/title2.html"));
  EXPECT_TRUE(WaitForLoadStop(wc));

  // A dialog from the subframe.
  EXPECT_TRUE(
      content::ExecuteScript(frame->current_frame_host(), alert_location));
  test_delegate.Wait();
  EXPECT_EQ(GURL("http://b.com/title2.html"),
            GURL(test_delegate.last_message()).ReplaceComponents(clear_port));

  // A dialog from the main frame.
  EXPECT_TRUE(
      content::ExecuteScript(root->current_frame_host(), alert_location));
  test_delegate.Wait();
  EXPECT_EQ(GURL("http://a.com/title1.html"),
            GURL(test_delegate.last_message()).ReplaceComponents(clear_port));

  // Navigate the top frame cross-site; ensure that dialogs work.
  NavigateToURL(shell(),
                embedded_test_server()->GetURL("c.com", "/title3.html"));
  EXPECT_TRUE(WaitForLoadStop(wc));
  EXPECT_TRUE(
      content::ExecuteScript(root->current_frame_host(), alert_location));
  test_delegate.Wait();
  EXPECT_EQ(GURL("http://c.com/title3.html"),
            GURL(test_delegate.last_message()).ReplaceComponents(clear_port));

  // Navigate back; ensure that dialogs work.
  wc->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(wc));
  EXPECT_TRUE(
      content::ExecuteScript(root->current_frame_host(), alert_location));
  test_delegate.Wait();
  EXPECT_EQ(GURL("http://a.com/title1.html"),
            GURL(test_delegate.last_message()).ReplaceComponents(clear_port));

  wc->SetDelegate(nullptr);
  wc->SetJavaScriptDialogManagerForTesting(nullptr);
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       CreateWebContentsWithRendererProcess) {
  GURL url("http://c.com/title3.html");
  ASSERT_TRUE(embedded_test_server()->Start());
  WebContents* base_web_contents = shell()->web_contents();
  ASSERT_TRUE(base_web_contents);

  WebContents::CreateParams create_params(
      base_web_contents->GetBrowserContext());
  create_params.initialize_renderer = true;
  create_params.initial_size =
      base_web_contents->GetContainerBounds().size();
  std::unique_ptr<WebContents> web_contents(WebContents::Create(create_params));
  ASSERT_TRUE(web_contents);

  // There is no navigation (to about:blank or something like that).
  EXPECT_FALSE(web_contents->IsLoading());

  ASSERT_TRUE(web_contents->GetMainFrame());
  EXPECT_TRUE(web_contents->GetMainFrame()->IsRenderFrameLive());
  EXPECT_TRUE(web_contents->GetController().IsInitialBlankNavigation());
  int renderer_id = web_contents->GetMainFrame()->GetProcess()->GetID();

  TestNavigationObserver same_tab_observer(web_contents.get(), 1);
  NavigationController::LoadURLParams params(url);
  params.transition_type = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
  web_contents->GetController().LoadURLWithParams(params);
  same_tab_observer.Wait();

  // Check that pre-warmed process is used.
  EXPECT_EQ(renderer_id, web_contents->GetMainFrame()->GetProcess()->GetID());
  EXPECT_EQ(1, web_contents->GetController().GetEntryCount());
  NavigationEntry* entry =
      web_contents->GetController().GetLastCommittedEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(url, entry->GetURL());
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       NavigatingToWebUIDoesNotUsePreWarmedProcess) {
  GURL web_ui_url(std::string(kChromeUIScheme) + "://" +
                  std::string(kChromeUIGpuHost));
  ASSERT_TRUE(embedded_test_server()->Start());
  WebContents* base_web_contents = shell()->web_contents();
  ASSERT_TRUE(base_web_contents);

  WebContents::CreateParams create_params(
      base_web_contents->GetBrowserContext());
  create_params.initialize_renderer = true;
  create_params.initial_size =
      base_web_contents->GetContainerBounds().size();
  std::unique_ptr<WebContents> web_contents(WebContents::Create(create_params));
  ASSERT_TRUE(web_contents);

  // There is no navigation (to about:blank or something like that).
  EXPECT_FALSE(web_contents->IsLoading());

  ASSERT_TRUE(web_contents->GetMainFrame());
  EXPECT_TRUE(web_contents->GetMainFrame()->IsRenderFrameLive());
  EXPECT_TRUE(web_contents->GetController().IsInitialBlankNavigation());
  int renderer_id = web_contents->GetMainFrame()->GetProcess()->GetID();

  TestNavigationObserver same_tab_observer(web_contents.get(), 1);
  NavigationController::LoadURLParams params(web_ui_url);
  params.transition_type = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
  web_contents->GetController().LoadURLWithParams(params);
  same_tab_observer.Wait();

  // Check that pre-warmed process isn't used.
  EXPECT_NE(renderer_id, web_contents->GetMainFrame()->GetProcess()->GetID());
  EXPECT_EQ(1, web_contents->GetController().GetEntryCount());
  NavigationEntry* entry =
      web_contents->GetController().GetLastCommittedEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(web_ui_url, entry->GetURL());
}

namespace {

class DownloadImageObserver {
 public:
  MOCK_METHOD5(OnFinishDownloadImage, void(
      int id,
      int status_code,
      const GURL& image_url,
      const std::vector<SkBitmap>& bitmap,
      const std::vector<gfx::Size>& sizes));
  ~DownloadImageObserver() = default;
};

void DownloadImageTestInternal(Shell* shell,
                               const GURL& image_url,
                               int expected_http_status,
                               int expected_number_of_images) {
  using ::testing::_;
  using ::testing::InvokeWithoutArgs;
  using ::testing::SizeIs;

  // Set up everything.
  DownloadImageObserver download_image_observer;
  scoped_refptr<MessageLoopRunner> loop_runner =
      new MessageLoopRunner();

  // Set up expectation and stub.
  EXPECT_CALL(download_image_observer,
              OnFinishDownloadImage(_, expected_http_status, _,
                                    SizeIs(expected_number_of_images), _));
  ON_CALL(download_image_observer, OnFinishDownloadImage(_, _, _, _, _))
      .WillByDefault(
          InvokeWithoutArgs(loop_runner.get(), &MessageLoopRunner::Quit));

  shell->LoadURL(GURL("about:blank"));
  shell->web_contents()->DownloadImage(
      image_url, false, 1024, false,
      base::Bind(&DownloadImageObserver::OnFinishDownloadImage,
                 base::Unretained(&download_image_observer)));

  // Wait for response.
  loop_runner->Run();
}

void ExpectNoValidImageCallback(const base::Closure& quit_closure,
                                int id,
                                int status_code,
                                const GURL& image_url,
                                const std::vector<SkBitmap>& bitmap,
                                const std::vector<gfx::Size>& sizes) {
  EXPECT_EQ(200, status_code);
  EXPECT_TRUE(bitmap.empty());
  EXPECT_TRUE(sizes.empty());
  quit_closure.Run();
}

}  // anonymous namespace

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       DownloadImage_HttpImage) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kImageUrl = embedded_test_server()->GetURL("/single_face.jpg");
  DownloadImageTestInternal(shell(), kImageUrl, 200, 1);
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       DownloadImage_Deny_FileImage) {
  ASSERT_TRUE(embedded_test_server()->Start());
  shell()->LoadURL(embedded_test_server()->GetURL("/simple_page.html"));

  const GURL kImageUrl = GetTestUrl("", "single_face.jpg");
  DownloadImageTestInternal(shell(), kImageUrl, 0, 0);
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       DownloadImage_Allow_FileImage) {
  shell()->LoadURL(GetTestUrl("", "simple_page.html"));

  const GURL kImageUrl =
      GetTestUrl("", "image.jpg");
  DownloadImageTestInternal(shell(), kImageUrl, 0, 0);
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, DownloadImage_NoValidImage) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kImageUrl = embedded_test_server()->GetURL("/invalid.ico");
  shell()->LoadURL(GURL("about:blank"));
  base::RunLoop run_loop;
  shell()->web_contents()->DownloadImage(
      kImageUrl, false, 2, false,
      base::Bind(&ExpectNoValidImageCallback, run_loop.QuitClosure()));

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, DownloadImage_DataImage) {
  const GURL kImageUrl = GURL(
      "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAAHE"
      "lEQVQI12P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHwAAAABJRU5ErkJggg==");
  DownloadImageTestInternal(shell(), kImageUrl, 0, 1);
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       DownloadImage_InvalidDataImage) {
  const GURL kImageUrl = GURL("data:image/png;invalid");
  DownloadImageTestInternal(shell(), kImageUrl, 0, 0);
}

class MouseLockDelegate : public WebContentsDelegate {
 public:
  // WebContentsDelegate:
  void RequestToLockMouse(WebContents* web_contents,
                          bool user_gesture,
                          bool last_unlocked_by_target) override {
    request_to_lock_mouse_called_ = true;
  }
  bool request_to_lock_mouse_called_ = false;
};

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       RenderWidgetDeletedWhileMouseLockPending) {
  ASSERT_TRUE(embedded_test_server()->Start());

  std::unique_ptr<MouseLockDelegate> delegate(new MouseLockDelegate());
  shell()->web_contents()->SetDelegate(delegate.get());
  ASSERT_TRUE(shell()->web_contents()->GetDelegate() == delegate.get());

  NavigateToURL(shell(),
                embedded_test_server()->GetURL("a.com", "/title1.html"));

  // Try to request pointer lock. WebContentsDelegate should get a notification.
  ASSERT_TRUE(ExecuteScript(shell(),
                            "window.domAutomationController.send(document.body."
                            "requestPointerLock());"));
  EXPECT_TRUE(delegate.get()->request_to_lock_mouse_called_);

  // Make sure that the renderer didn't get the pointer lock, since the
  // WebContentsDelegate didn't approve the notification.
  bool locked = false;
  ASSERT_TRUE(ExecuteScriptAndExtractBool(shell(),
                                          "window.domAutomationController.send("
                                          "document.pointerLockElement == "
                                          "null);",
                                          &locked));
  EXPECT_TRUE(locked);

  // Try to request the pointer lock again. Since there's a pending request in
  // WebContentsDelelgate, the WebContents shouldn't ask again.
  delegate.get()->request_to_lock_mouse_called_ = false;
  ASSERT_TRUE(ExecuteScript(shell(),
                            "window.domAutomationController.send(document.body."
                            "requestPointerLock());"));
  EXPECT_FALSE(delegate.get()->request_to_lock_mouse_called_);

  // Force a cross-process navigation so that the RenderWidgetHost is deleted.
  NavigateToURL(shell(),
                embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Make sure the WebContents cleaned up the previous pending request. A new
  // request should be forwarded to the WebContentsDelegate.
  delegate.get()->request_to_lock_mouse_called_ = false;
  ASSERT_TRUE(ExecuteScript(shell(),
                            "window.domAutomationController.send(document.body."
                            "requestPointerLock());"));
  EXPECT_TRUE(delegate.get()->request_to_lock_mouse_called_);
}

namespace {
class TestResourceDispatcherHostDelegate
    : public ShellResourceDispatcherHostDelegate {
 public:
  explicit TestResourceDispatcherHostDelegate(bool* saw_override)
      : saw_override_(saw_override) {}

  void RequestBeginning(
      net::URLRequest* request,
      ResourceContext* resource_context,
      AppCacheService* appcache_service,
      ResourceType resource_type,
      std::vector<std::unique_ptr<ResourceThrottle>>* throttles) override {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    ShellResourceDispatcherHostDelegate::RequestBeginning(
        request, resource_context, appcache_service, resource_type, throttles);

    net::HttpRequestHeaders headers = request->extra_request_headers();
    std::string user_agent;
    CHECK(headers.GetHeader(net::HttpRequestHeaders::kUserAgent, &user_agent));
    if (user_agent.find("foo") != std::string::npos)
      *saw_override_ = true;
  }

 private:
  bool* saw_override_;
};
}  // namespace

// Checks that user agent override string is only used when it's overridden.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, UserAgentOverride) {
  bool saw_override = false;
  TestResourceDispatcherHostDelegate new_delegate(&saw_override);
  ResourceDispatcherHostDelegate* old_delegate =
      ResourceDispatcherHostImpl::Get()->delegate();
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&ResourceDispatcherHost::SetDelegate,
                     base::Unretained(ResourceDispatcherHostImpl::Get()),
                     &new_delegate));

  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kUrl(embedded_test_server()->GetURL("/simple_page.html"));
  NavigateToURL(shell(), kUrl);
  ASSERT_FALSE(saw_override);

  shell()->web_contents()->SetUserAgentOverride("foo");
  NavigateToURL(shell(), kUrl);
  ASSERT_FALSE(saw_override);

  shell()
      ->web_contents()
      ->GetController()
      .GetLastCommittedEntry()
      ->SetIsOverridingUserAgent(true);
  TestNavigationObserver tab_observer(shell()->web_contents(), 1);
  shell()->web_contents()->GetController().Reload(ReloadType::NORMAL, false);
  tab_observer.Wait();
  ASSERT_TRUE(saw_override);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&ResourceDispatcherHost::SetDelegate,
                     base::Unretained(ResourceDispatcherHostImpl::Get()),
                     old_delegate));
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       DialogsFromJavaScriptEndFullscreen) {
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  TestWCDelegateForDialogsAndFullscreen test_delegate;
  wc->SetDelegate(&test_delegate);

  GURL url("about:blank");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // alert
  wc->EnterFullscreenMode(url);
  EXPECT_TRUE(wc->IsFullscreenForCurrentTab());
  std::string script = "alert('hi')";
  EXPECT_TRUE(content::ExecuteScript(wc, script));
  test_delegate.Wait();
  EXPECT_FALSE(wc->IsFullscreenForCurrentTab());

  // confirm
  wc->EnterFullscreenMode(url);
  EXPECT_TRUE(wc->IsFullscreenForCurrentTab());
  script = "confirm('hi')";
  EXPECT_TRUE(content::ExecuteScript(wc, script));
  test_delegate.Wait();
  EXPECT_FALSE(wc->IsFullscreenForCurrentTab());

  // prompt
  wc->EnterFullscreenMode(url);
  EXPECT_TRUE(wc->IsFullscreenForCurrentTab());
  script = "prompt('hi')";
  EXPECT_TRUE(content::ExecuteScript(wc, script));
  test_delegate.Wait();
  EXPECT_FALSE(wc->IsFullscreenForCurrentTab());

  // beforeunload
  wc->EnterFullscreenMode(url);
  EXPECT_TRUE(wc->IsFullscreenForCurrentTab());
  // Disable the hang monitor (otherwise there will be a race between the
  // beforeunload dialog and the beforeunload hang timer) and give the page a
  // gesture to allow dialogs.
  wc->GetMainFrame()->DisableBeforeUnloadHangMonitorForTesting();
  wc->GetMainFrame()->ExecuteJavaScriptWithUserGestureForTests(
      base::string16());
  script = "window.onbeforeunload=function(e){ return 'x' };";
  EXPECT_TRUE(content::ExecuteScript(wc, script));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  test_delegate.Wait();
  EXPECT_FALSE(wc->IsFullscreenForCurrentTab());

  wc->SetDelegate(nullptr);
  wc->SetJavaScriptDialogManagerForTesting(nullptr);
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       PopupsFromJavaScriptEndFullscreen) {
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  TestWCDelegateForDialogsAndFullscreen test_delegate;
  wc->SetDelegate(&test_delegate);

  GURL url("about:blank");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // popup
  wc->EnterFullscreenMode(url);
  EXPECT_TRUE(wc->IsFullscreenForCurrentTab());
  std::string script = "window.open('', '', 'width=200,height=100')";
  EXPECT_TRUE(content::ExecuteScript(wc, script));
  test_delegate.Wait();
  EXPECT_FALSE(wc->IsFullscreenForCurrentTab());

  wc->SetDelegate(nullptr);
  wc->SetJavaScriptDialogManagerForTesting(nullptr);
}

class FormBubbleDelegate : public WebContentsDelegate {
 public:
  FormBubbleDelegate() = default;

  void WaitUntilShown() {
    while (!is_visible_) {
      message_loop_runner_ = new MessageLoopRunner;
      message_loop_runner_->Run();
    }
  }

  void WaitUntilHidden() {
    while (is_visible_) {
      message_loop_runner_ = new MessageLoopRunner;
      message_loop_runner_->Run();
    }
  }

 private:
  void ShowValidationMessage(WebContents* web_contents,
                             const gfx::Rect& anchor_in_root_view,
                             const base::string16& main_text,
                             const base::string16& sub_text) override {
    is_visible_ = true;
    if (message_loop_runner_)
      message_loop_runner_->Quit();
  }

  void HideValidationMessage(WebContents* web_contents) override {
    is_visible_ = false;
    if (message_loop_runner_)
      message_loop_runner_->Quit();
  }

  bool is_visible_ = false;
  scoped_refptr<MessageLoopRunner> message_loop_runner_;
};

// TODO(tkent): Remove this test when we remove the browser-side validation
// bubble implementation. crbug.com/739091
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       DISABLED_NavigationHidesFormValidationBubble) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title1.html")));

  // Start listening for requests to show or hide the form validation bubble.
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  FormBubbleDelegate bubble_delegate;
  web_contents->SetDelegate(&bubble_delegate);

  // Trigger a form validation bubble and verify that the bubble is shown.
  std::string script = R"(
      var input_field = document.createElement('input');
      input_field.required = true;
      var form = document.createElement('form');
      form.appendChild(input_field);
      document.body.appendChild(form);

      setTimeout(function() {
              input_field.setCustomValidity('Custom validity message');
              input_field.reportValidity();
          },
          0);
      )";
  ASSERT_TRUE(ExecuteScript(web_contents, script));
  bubble_delegate.WaitUntilShown();

  // Navigate to another page and verify that the form validation bubble is
  // hidden.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title2.html")));
  bubble_delegate.WaitUntilHidden();
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       FrameDetachInCopyDoesNotCrash) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(NavigateToURL(
      shell(),
      embedded_test_server()->GetURL("a.com", "/detach_frame_in_copy.html")));

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  // Focus the child frame before sending it a copy command: the child frame
  // will detach itself upon getting a 'copy' event.
  ASSERT_TRUE(ExecuteScript(web_contents, "window[0].focus();"));
  FrameTree* frame_tree = web_contents->GetFrameTree();
  FrameTreeNode* root = frame_tree->root();
  ASSERT_EQ(root->child_at(0), frame_tree->GetFocusedFrame());
  shell()->web_contents()->Copy();

  TitleWatcher title_watcher(web_contents, base::ASCIIToUTF16("done"));
  base::string16 title = title_watcher.WaitAndGetTitle();
  ASSERT_EQ(title, base::ASCIIToUTF16("done"));
}

}  // namespace content
