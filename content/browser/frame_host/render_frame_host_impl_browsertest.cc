// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/render_frame_host_impl.h"

#include "base/macros.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/frame_messages.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/test_content_browser_client.h"

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
using RenderFrameHostImplBrowserTest = ContentBrowserTest;

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
                           const GURL& origin_url,
                           JavaScriptDialogType dialog_type,
                           const base::string16& message_text,
                           const base::string16& default_prompt_text,
                           const DialogClosedCallback& callback,
                           bool* did_suppress_message) override {}

  void RunBeforeUnloadDialog(WebContents* web_contents,
                             bool is_reload,
                             const DialogClosedCallback& callback) override {
    callback_ = callback;
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
  dialog_manager.callback().Run(true, base::string16());

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
  dialog_manager.callback().Run(true, base::string16());
  EXPECT_TRUE(WaitForLoadStop(wc));

  // The reload should have cleared the user gesture bit, so upon leaving again
  // there should be no beforeunload dialog.
  shell()->LoadURL(GURL("about:blank"));
  EXPECT_TRUE(WaitForLoadStop(wc));

  wc->SetDelegate(nullptr);
  wc->SetJavaScriptDialogManagerForTesting(nullptr);
}

}  // namespace content
