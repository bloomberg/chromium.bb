// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_contents_view_aura.h"

#include "base/command_line.h"
#include "base/test/scoped_command_line.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display_switches.h"

namespace content {

class WebContentsViewAuraTest : public RenderViewHostTestHarness {
 public:
  WebContentsViewAuraTest() = default;
  ~WebContentsViewAuraTest() override = default;

  WebContentsViewAura* view() {
    WebContentsImpl* contents = static_cast<WebContentsImpl*>(web_contents());
    return static_cast<WebContentsViewAura*>(contents->GetView());
  }
};

TEST_F(WebContentsViewAuraTest, EnableDisableOverscroll) {
  WebContentsViewAura* wcva = view();
  wcva->SetOverscrollControllerEnabled(false);
  EXPECT_FALSE(wcva->navigation_overlay_);
  wcva->SetOverscrollControllerEnabled(true);
  EXPECT_TRUE(wcva->navigation_overlay_);
}

TEST_F(WebContentsViewAuraTest, ScreenInfoColorDepth) {
  WebContentsView* web_contents_view = view();

  ScreenInfo screen_info;
  web_contents_view->GetScreenInfo(&screen_info);
  EXPECT_EQ(24u, screen_info.depth);
  EXPECT_EQ(8u, screen_info.depth_per_component);
}

}  // namespace content
