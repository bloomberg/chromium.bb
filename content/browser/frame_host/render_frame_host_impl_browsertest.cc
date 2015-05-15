// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/test_content_browser_client.h"

namespace content {

namespace {

RenderFrameHostImpl* ToRFHI(RenderFrameHost* render_frame_host) {
  return static_cast<RenderFrameHostImpl*>(render_frame_host);
}

// Implementation of ContentBrowserClient that overrides
// OverridePageVisibilityState() and allows consumers to set a value.
class PrerenderTestContentBrowserClient : public TestContentBrowserClient {
 public:
  PrerenderTestContentBrowserClient()
    : override_enabled_(false),
      visibility_override_(blink::WebPageVisibilityStateVisible)
  {}
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

using RenderFrameHostImplBrowserTest = ContentBrowserTest;

// Test that when creating a new window, the main frame is correctly focused.
// flaky http://crbug.com/452631
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       DISABLED_IsFocused_AtLoad) {
  EXPECT_TRUE(
      NavigateToURL(shell(), GetTestUrl("render_frame_host", "focus.html")));

  // The main frame should be focused.
  WebContents* web_contents = shell()->web_contents();
  EXPECT_TRUE(ToRFHI(web_contents->GetMainFrame())->IsFocused());
}

// Test that if the content changes the focused frame, it is correctly exposed.
// Disabled to to flaky failures: crbug.com/452631
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       DISABLED_IsFocused_Change) {
  EXPECT_TRUE(
      NavigateToURL(shell(), GetTestUrl("render_frame_host", "focus.html")));

  WebContents* web_contents = shell()->web_contents();

  std::string frames[2] = { "frame1", "frame2" };
  for (const std::string& frame : frames) {
    ExecuteScriptAndGetValue(
        web_contents->GetMainFrame(), "focus" + frame + "()");

    // The main frame is not the focused frame in the frame tree but the main
    // frame is focused per RFHI rules because one of its descendant is focused.
    EXPECT_NE(web_contents->GetMainFrame(), web_contents->GetFocusedFrame());
    EXPECT_TRUE(ToRFHI(web_contents->GetFocusedFrame())->IsFocused());
    EXPECT_TRUE(ToRFHI(web_contents->GetMainFrame())->IsFocused());
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

  // TODO(nick,mlamouri): Add calls to RFHI::IsFocused here once they're not
  // flaky. See http://crbug.com/452631, http://crbug.com/464033, etc.
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

// Test that even if the frame is focused in the frame tree but its
// RenderWidgetHost is not focused, it is not considered as focused.
// flaky http://crbug.com/452631
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       DISABLED_IsFocused_Widget) {
  EXPECT_TRUE(
      NavigateToURL(shell(), GetTestUrl("render_frame_host", "focus.html")));
  WebContents* web_contents = shell()->web_contents();

  // A second window is created and navigated. It takes the focus.
  Shell* new_shell = CreateBrowser();
  EXPECT_TRUE(
      NavigateToURL(new_shell, GetTestUrl("render_frame_host", "focus.html")));
  EXPECT_TRUE(ToRFHI(new_shell->web_contents()->GetMainFrame())->IsFocused());

  // The first opened window is no longer focused. The main frame is still the
  // focused frame in the frame tree but as far as RFH is concerned, the frame
  // is not focused.
  EXPECT_EQ(web_contents->GetMainFrame(), web_contents->GetFocusedFrame());

#if defined(OS_MACOSX)
  EXPECT_TRUE(ToRFHI(web_contents->GetMainFrame())->IsFocused());
#else
  EXPECT_FALSE(ToRFHI(web_contents->GetMainFrame())->IsFocused());
#endif
}

// Test that a frame is visible/hidden depending on its WebContents visibility
// state.
// Flaky on Mac.  http://crbug.com/467670
#if defined(OS_MACOSX)
#define MAYBE_GetVisibilityState_Basic DISABLED_GetVisibilityState_Basic
#else
#define MAYBE_GetVisibilityState_Basic GetVisibilityState_Basic
#endif
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       MAYBE_GetVisibilityState_Basic) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL("data:text/html,foo")));
  WebContents* web_contents = shell()->web_contents();

  EXPECT_EQ(blink::WebPageVisibilityStateVisible,
            web_contents->GetMainFrame()->GetVisibilityState());

  web_contents->WasHidden();
  EXPECT_EQ(blink::WebPageVisibilityStateHidden,
            web_contents->GetMainFrame()->GetVisibilityState());
}

// Test that a frame visibility can be overridden by the ContentBrowserClient.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       GetVisibilityState_Override) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL("data:text/html,foo")));
  WebContents* web_contents = shell()->web_contents();

  PrerenderTestContentBrowserClient new_client;
  ContentBrowserClient* old_client = SetBrowserClientForTesting(&new_client);

  EXPECT_EQ(blink::WebPageVisibilityStateVisible,
            web_contents->GetMainFrame()->GetVisibilityState());

  new_client.EnableVisibilityOverride(blink::WebPageVisibilityStatePrerender);
  EXPECT_EQ(blink::WebPageVisibilityStatePrerender,
            web_contents->GetMainFrame()->GetVisibilityState());

  SetBrowserClientForTesting(old_client);
}

}  // namespace content
