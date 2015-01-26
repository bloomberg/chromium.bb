// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"

namespace content {

namespace {

RenderFrameHostImpl* ToRFHI(RenderFrameHost* render_frame_host) {
  return static_cast<RenderFrameHostImpl*>(render_frame_host);
}

}  // anonymous namespace

using RenderFrameHostImplBrowserTest = ContentBrowserTest;

// Test that when creating a new window, the main frame is correctly focused.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest, IsFocused_AtLoad) {
  EXPECT_TRUE(
      NavigateToURL(shell(), GetTestUrl("render_frame_host", "focus.html")));

  // The main frame should be focused.
  WebContents* web_contents = shell()->web_contents();
  EXPECT_TRUE(ToRFHI(web_contents->GetMainFrame())->IsFocused());
}

// Test that if the content changes the focused frame, it is correctly exposed.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest, IsFocused_Change) {
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

// Test that even if the frame is focused in the frame tree but its
// RenderWidgetHost is not focused, it is not considered as focused.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest, IsFocused_Widget) {
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

}  // namespace content
