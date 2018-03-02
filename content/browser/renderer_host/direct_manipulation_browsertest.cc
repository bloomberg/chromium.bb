// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/win/direct_manipulation.h"

#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "base/win/windows_version.h"
#include "content/browser/renderer_host/legacy_render_widget_host_win.h"
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ui_base_features.h"
#include "url/gurl.h"

namespace content {

class DirectManipulationBrowserTest : public ContentBrowserTest {
 public:
  DirectManipulationBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kPrecisionTouchpad);
  }

  ~DirectManipulationBrowserTest() override {}

  LegacyRenderWidgetHostHWND* GetLegacyRenderWidgetHostHWND() {
    RenderWidgetHostViewAura* rwhva = static_cast<RenderWidgetHostViewAura*>(
        shell()->web_contents()->GetRenderWidgetHostView());
    return rwhva->legacy_render_widget_host_HWND_;
  }

  void SimulatePointerHitTest() {
    LegacyRenderWidgetHostHWND* lrwhh = GetLegacyRenderWidgetHostHWND();
    ASSERT_TRUE(lrwhh);

    lrwhh->direct_manipulation_helper_->need_poll_events_ = true;
    lrwhh->CreateAnimationObserver();
  }

  void UpdateParent(HWND hwnd) {
    LegacyRenderWidgetHostHWND* lrwhh = GetLegacyRenderWidgetHostHWND();
    ASSERT_TRUE(lrwhh);

    lrwhh->UpdateParent(hwnd);
  }

  bool HasCompositorAnimationObserver(LegacyRenderWidgetHostHWND* lrwhh) {
    return lrwhh->compositor_animation_observer_ != nullptr;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(DirectManipulationBrowserTest);
};

// Ensure the AnimationObserver destroy when hwnd reparent to other hwnd.
IN_PROC_BROWSER_TEST_F(DirectManipulationBrowserTest, HWNDReparent) {
  if (base::win::GetVersion() < base::win::VERSION_WIN10)
    return;

  NavigateToURL(shell(), GURL(url::kAboutBlankURL));

  LegacyRenderWidgetHostHWND* lrwhh = GetLegacyRenderWidgetHostHWND();
  ASSERT_TRUE(lrwhh);

  // The observer should not create before it needed.
  ASSERT_TRUE(!HasCompositorAnimationObserver(lrwhh));

  // Add AnimationObserver to tab to simulate direct manipulation start.
  SimulatePointerHitTest();
  ASSERT_TRUE(HasCompositorAnimationObserver(lrwhh));

  // Create another browser.
  Shell* shell2 = CreateBrowser();
  NavigateToURL(shell2, GURL(url::kAboutBlankURL));

  // Move to the tab to browser2.
  UpdateParent(
      shell2->window()->GetRootWindow()->GetHost()->GetAcceleratedWidget());

  shell()->Close();

  // The animation observer should be removed.
  EXPECT_FALSE(HasCompositorAnimationObserver(lrwhh));
}

}  // namespace content
