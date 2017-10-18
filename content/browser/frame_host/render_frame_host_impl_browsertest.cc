// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/render_frame_host_impl.h"

#include <utility>

#include "base/macros.h"
#include "base/run_loop.h"
#include "content/browser/frame_host/navigation_handle_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/frame_messages.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/controllable_http_response.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/test_content_browser_client.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"

namespace content {

namespace {

// Implementation of ContentBrowserClient that overrides
// OverridePageVisibilityState() and allows consumers to set a value.
class PrerenderTestContentBrowserClient : public TestContentBrowserClient {
 public:
  PrerenderTestContentBrowserClient()
      : override_enabled_(false),
        visibility_override_(blink::kWebPageVisibilityStateVisible) {}
  ~PrerenderTestContentBrowserClient() override {}

  void EnableVisibilityOverride(
      blink::WebPageVisibilityState visibility_override) {
    override_enabled_ = true;
    visibility_override_ = visibility_override;
  }

  void OverridePageVisibilityState(
      RenderFrameHost* render_frame_host,
      blink::WebPageVisibilityState* visibility_state) override {
    if (override_enabled_)
      *visibility_state = visibility_override_;
  }

 private:
  bool override_enabled_;
  blink::WebPageVisibilityState visibility_override_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderTestContentBrowserClient);
};

}  // anonymous namespace

// TODO(mlamouri): part of these tests were removed because they were dependent
// on an environment were focus is guaranteed. This is only for
// interactive_ui_tests so these bits need to move there.
// See https://crbug.com/491535
class RenderFrameHostImplBrowserTest : public ContentBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

// Test that when creating a new window, the main frame is correctly focused.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       IsFocused_AtLoad) {
  EXPECT_TRUE(
      NavigateToURL(shell(), GetTestUrl("render_frame_host", "focus.html")));

  // The main frame should be focused.
  WebContents* web_contents = shell()->web_contents();
  EXPECT_EQ(web_contents->GetMainFrame(), web_contents->GetFocusedFrame());
}

// Test that if the content changes the focused frame, it is correctly exposed.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       IsFocused_Change) {
  EXPECT_TRUE(
      NavigateToURL(shell(), GetTestUrl("render_frame_host", "focus.html")));

  WebContents* web_contents = shell()->web_contents();

  std::string frames[2] = { "frame1", "frame2" };
  for (const std::string& frame : frames) {
    ExecuteScriptAndGetValue(
        web_contents->GetMainFrame(), "focus" + frame + "()");

    // The main frame is not the focused frame in the frame tree but the main
    // frame is focused per RFHI rules because one of its descendant is focused.
    // TODO(mlamouri): we should check the frame focus state per RFHI, see the
    // general comment at the beginning of this test file.
    EXPECT_NE(web_contents->GetMainFrame(), web_contents->GetFocusedFrame());
    EXPECT_EQ(frame, web_contents->GetFocusedFrame()->GetFrameName());
  }
}

// Tests focus behavior when the focused frame is removed from the frame tree.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest, RemoveFocusedFrame) {
  EXPECT_TRUE(
      NavigateToURL(shell(), GetTestUrl("render_frame_host", "focus.html")));

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  ExecuteScriptAndGetValue(web_contents->GetMainFrame(), "focusframe4()");

  EXPECT_NE(web_contents->GetMainFrame(), web_contents->GetFocusedFrame());
  EXPECT_EQ("frame4", web_contents->GetFocusedFrame()->GetFrameName());
  EXPECT_EQ("frame3",
            web_contents->GetFocusedFrame()->GetParent()->GetFrameName());
  EXPECT_NE(-1, web_contents->GetFrameTree()->focused_frame_tree_node_id_);

  ExecuteScriptAndGetValue(web_contents->GetMainFrame(), "detachframe(3)");
  EXPECT_EQ(nullptr, web_contents->GetFocusedFrame());
  EXPECT_EQ(-1, web_contents->GetFrameTree()->focused_frame_tree_node_id_);

  ExecuteScriptAndGetValue(web_contents->GetMainFrame(), "focusframe2()");
  EXPECT_NE(nullptr, web_contents->GetFocusedFrame());
  EXPECT_NE(web_contents->GetMainFrame(), web_contents->GetFocusedFrame());
  EXPECT_NE(-1, web_contents->GetFrameTree()->focused_frame_tree_node_id_);

  ExecuteScriptAndGetValue(web_contents->GetMainFrame(), "detachframe(2)");
  EXPECT_EQ(nullptr, web_contents->GetFocusedFrame());
  EXPECT_EQ(-1, web_contents->GetFrameTree()->focused_frame_tree_node_id_);
}

// Test that a frame is visible/hidden depending on its WebContents visibility
// state.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       GetVisibilityState_Basic) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL("data:text/html,foo")));
  WebContents* web_contents = shell()->web_contents();

  web_contents->WasShown();
  EXPECT_EQ(blink::kWebPageVisibilityStateVisible,
            web_contents->GetMainFrame()->GetVisibilityState());

  web_contents->WasHidden();
  EXPECT_EQ(blink::kWebPageVisibilityStateHidden,
            web_contents->GetMainFrame()->GetVisibilityState());
}

// Test that a frame visibility can be overridden by the ContentBrowserClient.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       GetVisibilityState_Override) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL("data:text/html,foo")));
  WebContents* web_contents = shell()->web_contents();

  PrerenderTestContentBrowserClient new_client;
  ContentBrowserClient* old_client = SetBrowserClientForTesting(&new_client);

  web_contents->WasShown();
  EXPECT_EQ(blink::kWebPageVisibilityStateVisible,
            web_contents->GetMainFrame()->GetVisibilityState());

  new_client.EnableVisibilityOverride(blink::kWebPageVisibilityStatePrerender);
  EXPECT_EQ(blink::kWebPageVisibilityStatePrerender,
            web_contents->GetMainFrame()->GetVisibilityState());

  SetBrowserClientForTesting(old_client);
}

namespace {

class TestJavaScriptDialogManager : public JavaScriptDialogManager,
                                    public WebContentsDelegate {
 public:
  TestJavaScriptDialogManager() : message_loop_runner_(new MessageLoopRunner) {}
  ~TestJavaScriptDialogManager() override {}

  void Wait() {
    message_loop_runner_->Run();
    message_loop_runner_ = new MessageLoopRunner;
  }

  DialogClosedCallback& callback() { return callback_; }

  // WebContentsDelegate

  JavaScriptDialogManager* GetJavaScriptDialogManager(
      WebContents* source) override {
    return this;
  }

  // JavaScriptDialogManager

  void RunJavaScriptDialog(WebContents* web_contents,
                           const GURL& alerting_frame_url,
                           JavaScriptDialogType dialog_type,
                           const base::string16& message_text,
                           const base::string16& default_prompt_text,
                           DialogClosedCallback callback,
                           bool* did_suppress_message) override {}

  void RunBeforeUnloadDialog(WebContents* web_contents,
                             bool is_reload,
                             DialogClosedCallback callback) override {
    callback_ = std::move(callback);
    message_loop_runner_->Quit();
  }

  bool HandleJavaScriptDialog(WebContents* web_contents,
                              bool accept,
                              const base::string16* prompt_override) override {
    return true;
  }

  void CancelDialogs(WebContents* web_contents, bool reset_state) override {}

 private:
  DialogClosedCallback callback_;

  // The MessageLoopRunner used to spin the message loop.
  scoped_refptr<MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(TestJavaScriptDialogManager);
};

class DropBeforeUnloadACKFilter : public BrowserMessageFilter {
 public:
  DropBeforeUnloadACKFilter() : BrowserMessageFilter(FrameMsgStart) {}

 protected:
  ~DropBeforeUnloadACKFilter() override {}

 private:
  // BrowserMessageFilter:
  bool OnMessageReceived(const IPC::Message& message) override {
    return message.type() == FrameHostMsg_BeforeUnload_ACK::ID;
  }

  DISALLOW_COPY_AND_ASSIGN(DropBeforeUnloadACKFilter);
};

}  // namespace

// Tests that a beforeunload dialog in an iframe doesn't stop the beforeunload
// timer of a parent frame.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       IframeBeforeUnloadParentHang) {
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  TestJavaScriptDialogManager dialog_manager;
  wc->SetDelegate(&dialog_manager);

  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));
  // Make an iframe with a beforeunload handler.
  std::string script =
      "var iframe = document.createElement('iframe');"
      "document.body.appendChild(iframe);"
      "iframe.contentWindow.onbeforeunload=function(e){return 'x'};";
  EXPECT_TRUE(content::ExecuteScript(wc, script));
  EXPECT_TRUE(WaitForLoadStop(wc));
  // JavaScript onbeforeunload dialogs require a user gesture.
  for (auto* frame : wc->GetAllFrames())
    frame->ExecuteJavaScriptWithUserGestureForTests(base::string16());

  // Force a process switch by going to a privileged page. The beforeunload
  // timer will be started on the top-level frame but will be paused while the
  // beforeunload dialog is shown by the subframe.
  GURL web_ui_page(std::string(kChromeUIScheme) + "://" +
                   std::string(kChromeUIGpuHost));
  shell()->LoadURL(web_ui_page);
  dialog_manager.Wait();

  RenderFrameHostImpl* main_frame =
      static_cast<RenderFrameHostImpl*>(wc->GetMainFrame());
  EXPECT_TRUE(main_frame->is_waiting_for_beforeunload_ack());

  // Set up a filter to make sure that when the dialog is answered below and the
  // renderer sends the beforeunload ACK, it gets... ahem... lost.
  scoped_refptr<DropBeforeUnloadACKFilter> filter =
      new DropBeforeUnloadACKFilter();
  main_frame->GetProcess()->AddFilter(filter.get());

  // Answer the dialog.
  std::move(dialog_manager.callback()).Run(true, base::string16());

  // There will be no beforeunload ACK, so if the beforeunload ACK timer isn't
  // functioning then the navigation will hang forever and this test will time
  // out. If this waiting for the load stop works, this test won't time out.
  EXPECT_TRUE(WaitForLoadStop(wc));
  EXPECT_EQ(web_ui_page, wc->GetLastCommittedURL());

  wc->SetDelegate(nullptr);
  wc->SetJavaScriptDialogManagerForTesting(nullptr);
}

// Tests that a gesture is required in a frame before it can request a
// beforeunload dialog.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       BeforeUnloadDialogRequiresGesture) {
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  TestJavaScriptDialogManager dialog_manager;
  wc->SetDelegate(&dialog_manager);

  EXPECT_TRUE(NavigateToURL(
      shell(), GetTestUrl("render_frame_host", "beforeunload.html")));
  // Disable the hang monitor, otherwise there will be a race between the
  // beforeunload dialog and the beforeunload hang timer.
  wc->GetMainFrame()->DisableBeforeUnloadHangMonitorForTesting();

  // Reload. There should be no beforeunload dialog because there was no gesture
  // on the page. If there was, this WaitForLoadStop call will hang.
  wc->GetController().Reload(ReloadType::NORMAL, false);
  EXPECT_TRUE(WaitForLoadStop(wc));

  // Give the page a user gesture and try reloading again. This time there
  // should be a dialog. If there is no dialog, the call to Wait will hang.
  wc->GetMainFrame()->ExecuteJavaScriptWithUserGestureForTests(
      base::string16());
  wc->GetController().Reload(ReloadType::NORMAL, false);
  dialog_manager.Wait();

  // Answer the dialog.
  std::move(dialog_manager.callback()).Run(true, base::string16());
  EXPECT_TRUE(WaitForLoadStop(wc));

  // The reload should have cleared the user gesture bit, so upon leaving again
  // there should be no beforeunload dialog.
  shell()->LoadURL(GURL("about:blank"));
  EXPECT_TRUE(WaitForLoadStop(wc));

  wc->SetDelegate(nullptr);
  wc->SetJavaScriptDialogManagerForTesting(nullptr);
}

namespace {

// A helper to execute some script in a frame just before it is deleted, such
// that no message loops are pumped and no sync IPC messages are processed
// between script execution and the destruction of the RenderFrameHost  .
class ExecuteScriptBeforeRenderFrameDeletedHelper
    : public RenderFrameDeletedObserver {
 public:
  ExecuteScriptBeforeRenderFrameDeletedHelper(RenderFrameHost* observed_frame,
                                              const std::string& script)
      : RenderFrameDeletedObserver(observed_frame), script_(script) {}

 protected:
  // WebContentsObserver:
  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override {
    const bool was_deleted = deleted();
    RenderFrameDeletedObserver::RenderFrameDeleted(render_frame_host);
    if (deleted() && !was_deleted)
      ExecuteScriptAsync(render_frame_host, script_);
  }

 private:
  std::string script_;

  DISALLOW_COPY_AND_ASSIGN(ExecuteScriptBeforeRenderFrameDeletedHelper);
};

}  // namespace

// Regression test for https://crbug.com/728171 where the sync IPC channel has a
// connection error but we don't properly check for it. This occurs because we
// send a sync window.open IPC after the RenderFrameHost is destroyed.
//
// The test creates two WebContents rendered in the same process. The first is
// is the window-opener of the second, so the first window can be used to relay
// information collected during the destruction of the RenderFrame in the second
// WebContents back to the browser process.
//
// The issue is then reproduced by asynchronously triggering a call to
// window.open() in the main frame of the second WebContents in response to
// WebContentsObserver::RenderFrameDeleted -- that is, just before the RFHI is
// destroyed on the browser side. The test assumes that between these two
// events, the UI message loop is not pumped, and no sync IPC messages are
// processed on the UI thread.
//
// Note that if the second WebContents scheduled a call to window.close() to
// close itself after it calls window.open(), the CreateNewWindow sync IPC could
// be dispatched *before* ViewHostMsg_Close in the browser process, provided
// that the browser happened to be in IPC::SyncChannel::WaitForReply on the UI
// thread (most likely after sending GpuCommandBufferMsg_* messages), in which
// case incoming sync IPCs to this thread are dispatched, but the message loop
// is not pumped, so proxied non-sync IPCs are not delivered.
//
// Furthermore, on Android, exercising window.open() must be delayed until after
// content::RemoveShellView returns, as that method calls into JNI to close the
// view corresponding to the WebContents, which will then call back into native
// code and may run nested message loops and send sync IPC messages.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       FrameDetached_WindowOpenIPCFails) {
  NavigateToURL(shell(), GetTestUrl("", "title1.html"));
  EXPECT_EQ(1u, Shell::windows().size());
  GURL test_url = GetTestUrl("render_frame_host", "window_open.html");
  std::string open_script =
      base::StringPrintf("popup = window.open('%s');", test_url.spec().c_str());

  TestNavigationObserver second_contents_navigation_observer(nullptr, 1);
  second_contents_navigation_observer.StartWatchingNewWebContents();
  EXPECT_TRUE(content::ExecuteScript(shell(), open_script));
  second_contents_navigation_observer.Wait();

  ASSERT_EQ(2u, Shell::windows().size());
  Shell* new_shell = Shell::windows()[1];
  ExecuteScriptBeforeRenderFrameDeletedHelper deleted_observer(
      new_shell->web_contents()->GetMainFrame(), "callWindowOpen();");
  new_shell->Close();
  deleted_observer.WaitUntilDeleted();

  bool did_call_window_open = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(), "domAutomationController.send(!!popup.didCallWindowOpen)",
      &did_call_window_open));
  EXPECT_TRUE(did_call_window_open);

  std::string result_of_window_open;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      shell(), "domAutomationController.send(String(popup.resultOfWindowOpen))",
      &result_of_window_open));
  EXPECT_EQ("null", result_of_window_open);
}

// After a navigation, the StreamHandle must be released.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest, StreamHandleReleased) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestUrl("", "title1.html")));
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame =
      static_cast<RenderFrameHostImpl*>(wc->GetMainFrame());
  EXPECT_EQ(nullptr, main_frame->stream_handle_for_testing());
}

namespace {
class DropStreamHandleConsumedFilter : public BrowserMessageFilter {
 public:
  DropStreamHandleConsumedFilter() : BrowserMessageFilter(FrameMsgStart) {}

 protected:
  ~DropStreamHandleConsumedFilter() override {}

 private:
  // BrowserMessageFilter:
  bool OnMessageReceived(const IPC::Message& message) override {
    return message.type() == FrameHostMsg_StreamHandleConsumed::ID;
  }

  DISALLOW_COPY_AND_ASSIGN(DropStreamHandleConsumedFilter);
};
}  // namespace

// After a renderer crash, the StreamHandle must be released.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       StreamHandleReleasedOnRendererCrash) {
  // |stream_handle_| is only used with PlzNavigate.
  if (!IsBrowserSideNavigationEnabled())
    return;

  GURL url_1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_2(embedded_test_server()->GetURL("a.com", "/title2.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url_1));

  // Set up a filter to make sure that the browser is not notified that its
  // |stream_handle_| has been consumed.
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame =
      static_cast<RenderFrameHostImpl*>(wc->GetMainFrame());
  scoped_refptr<DropStreamHandleConsumedFilter> filter =
      new DropStreamHandleConsumedFilter();
  main_frame->GetProcess()->AddFilter(filter.get());

  EXPECT_TRUE(NavigateToURL(shell(), url_2));

  // Check that the |stream_handle_| hasn't been released yet.
  EXPECT_NE(nullptr, main_frame->stream_handle_for_testing());

  // Make the renderer crash.
  RenderProcessHost* renderer_process = main_frame->GetProcess();
  RenderProcessHostWatcher crash_observer(
      renderer_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  renderer_process->Shutdown(0, false);
  crash_observer.Wait();

  // The |stream_handle_| must have been released now.
  EXPECT_EQ(nullptr, main_frame->stream_handle_for_testing());
}

namespace {
void PostRequestMonitor(int* post_counter,
                        const net::test_server::HttpRequest& request) {
  if (request.method != net::test_server::METHOD_POST)
    return;
  (*post_counter)++;
  auto it = request.headers.find("Content-Type");
  CHECK(it != request.headers.end());
  CHECK(!it->second.empty());
}
}  // namespace

// Verifies form submits and resubmits work.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest, POSTNavigation) {
  net::EmbeddedTestServer http_server;
  base::FilePath content_test_data(FILE_PATH_LITERAL("content/test/data"));
  http_server.AddDefaultHandlers(content_test_data);
  int post_counter = 0;
  http_server.RegisterRequestMonitor(
      base::Bind(&PostRequestMonitor, &post_counter));
  ASSERT_TRUE(http_server.Start());

  GURL url(http_server.GetURL("/session_history/form.html"));
  GURL post_url = http_server.GetURL("/echotitle");

  // Navigate to a page with a form.
  TestNavigationObserver observer(shell()->web_contents());
  NavigateToURL(shell(), url);
  EXPECT_EQ(url, observer.last_navigation_url());
  EXPECT_TRUE(observer.last_navigation_succeeded());

  // Submit the form.
  GURL submit_url("javascript:submitForm('isubmit')");
  NavigateToURL(shell(), submit_url);

  // Check that a proper POST navigation was done.
  EXPECT_EQ("text=&select=a",
            base::UTF16ToASCII(shell()->web_contents()->GetTitle()));
  EXPECT_EQ(post_url, shell()->web_contents()->GetLastCommittedURL());
  EXPECT_TRUE(shell()
                  ->web_contents()
                  ->GetController()
                  .GetActiveEntry()
                  ->GetHasPostData());

  // Reload and verify the form was submitted.
  shell()->web_contents()->GetController().Reload(ReloadType::NORMAL, false);
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ("text=&select=a",
            base::UTF16ToASCII(shell()->web_contents()->GetTitle()));
  CHECK_EQ(2, post_counter);
}

namespace {

class NavigationHandleGrabber : public WebContentsObserver {
 public:
  explicit NavigationHandleGrabber(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  void ReadyToCommitNavigation(NavigationHandle* navigation_handle) override {
    if (navigation_handle->GetURL().path() != "/title2.html")
      return;
    static_cast<NavigationHandleImpl*>(navigation_handle)
        ->set_complete_callback_for_testing(
            base::Bind(&NavigationHandleGrabber::SendingNavigationCommitted,
                       base::Unretained(this), navigation_handle));
  }

  void SendingNavigationCommitted(
      NavigationHandle* navigation_handle,
      NavigationThrottle::ThrottleCheckResult result) {
    if (navigation_handle->GetURL().path() != "/title2.html")
      return;
    ExecuteScriptAsync(web_contents(), "document.open();");
  }

  void DidFinishNavigation(NavigationHandle* navigation_handle) override {
    if (navigation_handle->GetURL().path() != "/title2.html")
      return;
    if (navigation_handle->HasCommitted())
      committed_title2_ = true;
    run_loop_.QuitClosure().Run();
  }

  void WaitForTitle2() { run_loop_.Run(); }

  bool committed_title2() { return committed_title2_; }

 private:
  bool committed_title2_ = false;
  base::RunLoop run_loop_;
};
}  // namespace

// Verifies that if a frame aborts a navigation right after it starts, it is
// cancelled.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest, FastNavigationAbort) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  NavigateToURL(shell(), url);

  // Now make a navigation.
  NavigationHandleGrabber observer(shell()->web_contents());
  const base::string16 title = base::ASCIIToUTF16("done");
  EXPECT_TRUE(ExecuteScript(shell()->web_contents(),
                            "window.location.href='/title2.html'"));
  observer.WaitForTitle2();
  // Flush IPCs to make sure the renderer didn't tell us to navigate. Need to
  // make two round trips.
  EXPECT_TRUE(ExecuteScript(shell()->web_contents(), ""));
  EXPECT_TRUE(ExecuteScript(shell()->web_contents(), ""));
  EXPECT_FALSE(observer.committed_title2());
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       TerminationDisablersClearedOnRendererCrash) {
  EXPECT_TRUE(NavigateToURL(
      shell(), GetTestUrl("render_frame_host", "beforeunload.html")));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame =
      static_cast<RenderFrameHostImpl*>(wc->GetMainFrame());

  EXPECT_TRUE(main_frame->GetSuddenTerminationDisablerState(
      blink::kBeforeUnloadHandler));

  // Make the renderer crash.
  RenderProcessHost* renderer_process = main_frame->GetProcess();
  RenderProcessHostWatcher crash_observer(
      renderer_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  renderer_process->Shutdown(0, false);
  crash_observer.Wait();

  EXPECT_FALSE(main_frame->GetSuddenTerminationDisablerState(
      blink::kBeforeUnloadHandler));

  // This should not trigger a DCHECK once the renderer sends up the termination
  // disabler flags.
  shell()->web_contents()->GetController().Reload(ReloadType::NORMAL, false);
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  EXPECT_TRUE(main_frame->GetSuddenTerminationDisablerState(
      blink::kBeforeUnloadHandler));
}

// Aborted renderer-initiated navigations that don't destroy the current
// document (e.g. no error page is displayed) must not cancel pending
// XMLHttpRequests.
// See https://crbug.com/762945.
IN_PROC_BROWSER_TEST_F(
    ContentBrowserTest,
    AbortedRendererInitiatedNavigationDoNotCancelPendingXHR) {
  ControllableHttpResponse xhr_response(embedded_test_server(), "/xhr_request");
  EXPECT_TRUE(embedded_test_server()->Start());

  GURL main_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // 1) Send an xhr request, but do not send its response for the moment.
  const char* send_slow_xhr =
      "var request = new XMLHttpRequest();"
      "request.addEventListener('abort', () => document.title = 'xhr aborted');"
      "request.addEventListener('load', () => document.title = 'xhr loaded');"
      "request.open('GET', '%s');"
      "request.send();";
  const GURL slow_url = embedded_test_server()->GetURL("/xhr_request");
  EXPECT_TRUE(content::ExecuteScript(
      shell(), base::StringPrintf(send_slow_xhr, slow_url.spec().c_str())));
  xhr_response.WaitForRequest();

  // 2) In the meantime, create a renderer-initiated navigation. It will be
  // aborted.
  TestNavigationManager observer(shell()->web_contents(),
                                 GURL("customprotocol:aborted"));
  EXPECT_TRUE(content::ExecuteScript(
      shell(), "window.location = 'customprotocol:aborted'"));
  EXPECT_FALSE(observer.WaitForResponse());
  observer.WaitForNavigationFinished();

  // 3) Send the response for the XHR requests.
  xhr_response.Send(
      "HTTP/1.1 200 OK\r\n"
      "Connection: close\r\n"
      "Content-Length: 2\r\n"
      "Content-Type: text/plain; charset=utf-8\r\n"
      "\r\n"
      "OK");
  xhr_response.Done();

  // 4) Wait for the XHR request to complete.
  const base::string16 xhr_aborted_title = base::ASCIIToUTF16("xhr aborted");
  const base::string16 xhr_loaded_title = base::ASCIIToUTF16("xhr loaded");
  TitleWatcher watcher(shell()->web_contents(), xhr_loaded_title);
  watcher.AlsoWaitForTitle(xhr_aborted_title);

  EXPECT_EQ(xhr_loaded_title, watcher.WaitAndGetTitle());
}

// A browser-initiated javascript-url navigation must not prevent the current
// document from loading.
// See https://crbug.com/766149.
IN_PROC_BROWSER_TEST_F(ContentBrowserTest,
                       BrowserInitiatedJavascriptUrlDoNotPreventLoading) {
  ControllableHttpResponse main_document_response(embedded_test_server(),
                                                  "/main_document");
  EXPECT_TRUE(embedded_test_server()->Start());

  GURL main_document_url(embedded_test_server()->GetURL("/main_document"));
  TestNavigationManager main_document_observer(shell()->web_contents(),
                                               main_document_url);

  // 1) Navigate. Send the header but not the body. The navigation commits in
  //    the browser. The renderer is still loading the document.
  {
    shell()->LoadURL(main_document_url);
    EXPECT_TRUE(main_document_observer.WaitForRequestStart());
    main_document_observer.ResumeNavigation();  // Send the request.

    main_document_response.WaitForRequest();
    main_document_response.Send(
        "HTTP/1.1 200 OK\r\n"
        "Connection: close\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "\r\n");

    EXPECT_TRUE(main_document_observer.WaitForResponse());
    main_document_observer.ResumeNavigation();  // Commit the navigation.
  }

  // 2) A browser-initiated javascript-url navigation happens.
  {
    GURL javascript_url(
        "javascript:window.domAutomationController.send('done')");
    shell()->LoadURL(javascript_url);
    DOMMessageQueue dom_message_queue(WebContents::FromRenderFrameHost(
        shell()->web_contents()->GetMainFrame()));
    std::string done;
    EXPECT_TRUE(dom_message_queue.WaitForMessage(&done));
    EXPECT_EQ("\"done\"", done);
  }

  // 3) The end of the response is issued. The renderer must be able to receive
  //    it.
  {
    const base::string16 document_loaded_title =
        base::ASCIIToUTF16("document loaded");
    TitleWatcher watcher(shell()->web_contents(), document_loaded_title);
    main_document_response.Send(
        "<script>"
        "   window.onload = function(){"
        "     document.title = 'document loaded'"
        "   }"
        "</script>");
    main_document_response.Done();
    EXPECT_EQ(document_loaded_title, watcher.WaitAndGetTitle());
  }
}

// Test that a same-document browser-initiated navigation doesn't prevent a
// document from loading. See https://crbug.com/769645.
IN_PROC_BROWSER_TEST_F(
    ContentBrowserTest,
    SameDocumentBrowserInitiatedNavigationWhileDocumentIsLoading) {
  ControllableHttpResponse response(embedded_test_server(), "/main_document");
  EXPECT_TRUE(embedded_test_server()->Start());

  // 1) Load a new document. It reaches the ReadyToCommit stage and then is slow
  //    to load.
  GURL url(embedded_test_server()->GetURL("/main_document"));
  TestNavigationManager observer_new_document(shell()->web_contents(), url);
  shell()->LoadURL(url);

  // The navigation starts
  EXPECT_TRUE(observer_new_document.WaitForRequestStart());
  observer_new_document.ResumeNavigation();

  // The server sends the first part of the response and waits.
  response.WaitForRequest();
  response.Send(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "\r\n"
      "<html>"
      "  <body>"
      "    <div id=\"anchor\"></div>"
      "    <script>"
      "      domAutomationController.send('First part received')"
      "    </script>");

  // The browser reaches the ReadyToCommit stage.
  EXPECT_TRUE(observer_new_document.WaitForResponse());
  RenderFrameHostImpl* main_rfh = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetMainFrame());
  DOMMessageQueue dom_message_queue(WebContents::FromRenderFrameHost(main_rfh));
  observer_new_document.ResumeNavigation();

  // Wait for the renderer to load the first part of the response.
  std::string first_part_received;
  EXPECT_TRUE(dom_message_queue.WaitForMessage(&first_part_received));
  EXPECT_EQ("\"First part received\"", first_part_received);

  // 2) In the meantime, a browser-initiated same-document navigation commits.
  GURL anchor_url(url.spec() + "#anchor");
  TestNavigationManager observer_same_document(shell()->web_contents(),
                                               anchor_url);
  shell()->LoadURL(anchor_url);
  observer_same_document.WaitForNavigationFinished();

  // 3) The last part of the response is received.
  response.Send(
      "    <script>"
      "      domAutomationController.send('Second part received')"
      "    </script>"
      "  </body>"
      "</html>");
  response.Done();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // The renderer should be able to load the end of the response.
  std::string second_part_received;
  EXPECT_TRUE(dom_message_queue.WaitForMessage(&second_part_received));
  EXPECT_EQ("\"Second part received\"", second_part_received);
}

}  // namespace content
