// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/aura/overscroll_navigation_overlay.h"

#include "content/browser/web_contents/aura/image_window_delegate.h"
#include "content/common/view_messages.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"

namespace content {

class OverscrollNavigationOverlayTest : public RenderViewHostImplTestHarness {
 public:
  OverscrollNavigationOverlayTest() {}
  virtual ~OverscrollNavigationOverlayTest() {}

  scoped_ptr<aura::Window> CreateOverlayWinow(ImageWindowDelegate* delegate) {
    return make_scoped_ptr(
        aura::test::CreateTestWindowWithDelegate(delegate, 0,
            gfx::Rect(root_window()->bounds().size()),
            root_window()));
  }

  gfx::Image CreateDummyScreenshot() {
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, 1, 1);
    bitmap.allocPixels();
    bitmap.eraseColor(SK_ColorWHITE);
    return gfx::Image::CreateFrom1xBitmap(bitmap);
  }

 protected:
  // RenderViewHostImplTestHarness:
  virtual void SetUp() OVERRIDE {
    RenderViewHostImplTestHarness::SetUp();

    const GURL first("https://www.google.com");
    contents()->NavigateAndCommit(first);
    EXPECT_TRUE(controller().GetVisibleEntry());
    EXPECT_FALSE(controller().CanGoBack());

    const GURL second("http://www.chromium.org");
    contents()->NavigateAndCommit(second);
    EXPECT_TRUE(controller().CanGoBack());

    // Turn on compositing.
    ViewHostMsg_DidActivateAcceleratedCompositing msg(
        test_rvh()->GetRoutingID(), true);
    RenderViewHostTester::TestOnMessageReceived(test_rvh(), msg);

    // Receive a paint update. This is necessary to make sure the size is set
    // correctly in RenderWidgetHostImpl.
    ViewHostMsg_UpdateRect_Params params;
    memset(&params, 0, sizeof(params));
    params.view_size = gfx::Size(10, 10);
    params.bitmap_rect = gfx::Rect(params.view_size);
    params.scroll_rect = gfx::Rect();
    params.needs_ack = false;
    ViewHostMsg_UpdateRect rect(test_rvh()->GetRoutingID(), params);
    RenderViewHostTester::TestOnMessageReceived(test_rvh(), rect);

    // Reset pending flags for size/paint.
    test_rvh()->ResetSizeAndRepaintPendingFlags();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(OverscrollNavigationOverlayTest);
};

TEST_F(OverscrollNavigationOverlayTest, FirstVisuallyNonEmptyPaint_NoImage) {
  // Create the overlay, and set the contents of the overlay window.
  scoped_ptr<OverscrollNavigationOverlay> overlay(
      new OverscrollNavigationOverlay(contents()));
  ImageWindowDelegate* image_delegate = new ImageWindowDelegate();
  overlay->SetOverlayWindow(CreateOverlayWinow(image_delegate),
                            image_delegate);
  overlay->StartObserving();

  EXPECT_FALSE(overlay->received_paint_update_);
  EXPECT_TRUE(overlay->web_contents());

  // Receive a paint update.
  ViewHostMsg_DidFirstVisuallyNonEmptyPaint msg(test_rvh()->GetRoutingID(), 0);
  RenderViewHostTester::TestOnMessageReceived(test_rvh(), msg);
  EXPECT_TRUE(overlay->received_paint_update_);
  EXPECT_FALSE(overlay->loading_complete_);

  // The paint update will hide the overlay, although the page hasn't completely
  // loaded yet. This is because the image-delegate doesn't have an image set.
  EXPECT_FALSE(overlay->web_contents());
}

TEST_F(OverscrollNavigationOverlayTest, FirstVisuallyNonEmptyPaint_WithImage) {
  // Create the overlay, and set the contents of the overlay window.
  scoped_ptr<OverscrollNavigationOverlay> overlay(
      new OverscrollNavigationOverlay(contents()));
  ImageWindowDelegate* image_delegate = new ImageWindowDelegate();
  image_delegate->SetImage(CreateDummyScreenshot());
  overlay->SetOverlayWindow(CreateOverlayWinow(image_delegate),
                            image_delegate);
  overlay->StartObserving();

  EXPECT_FALSE(overlay->received_paint_update_);
  EXPECT_TRUE(overlay->web_contents());

  // Receive a paint update.
  ViewHostMsg_DidFirstVisuallyNonEmptyPaint msg(test_rvh()->GetRoutingID(), 0);
  RenderViewHostTester::TestOnMessageReceived(test_rvh(), msg);
  EXPECT_TRUE(overlay->received_paint_update_);
  EXPECT_FALSE(overlay->loading_complete_);
  EXPECT_TRUE(overlay->web_contents());

  contents()->TestSetIsLoading(false);
  EXPECT_TRUE(overlay->loading_complete_);
  EXPECT_FALSE(overlay->web_contents());
}

TEST_F(OverscrollNavigationOverlayTest, PaintUpdateWithoutNonEmptyPaint) {
  // Create the overlay, and set the contents of the overlay window.
  scoped_ptr<OverscrollNavigationOverlay> overlay(
      new OverscrollNavigationOverlay(contents()));
  ImageWindowDelegate* image_delegate = new ImageWindowDelegate();
  image_delegate->SetImage(CreateDummyScreenshot());
  overlay->SetOverlayWindow(CreateOverlayWinow(image_delegate),
                            image_delegate);
  overlay->StartObserving();

  process()->sink().ClearMessages();

  // The page load is complete, but the overlay should still be visible, because
  // there hasn't been any paint update.
  // This should also send a repaint request to the renderer, so that the
  // renderer repaints the contents.
  contents()->TestSetIsLoading(false);
  EXPECT_FALSE(overlay->received_paint_update_);
  EXPECT_TRUE(overlay->loading_complete_);
  EXPECT_TRUE(overlay->web_contents());
  EXPECT_TRUE(process()->sink().GetFirstMessageMatching(ViewMsg_Repaint::ID));

  // Receive a repaint ack update. This should hide the overlay.
  ViewHostMsg_UpdateRect_Params params;
  memset(&params, 0, sizeof(params));
  params.view_size = gfx::Size(10, 10);
  params.bitmap_rect = gfx::Rect(params.view_size);
  params.scroll_rect = gfx::Rect();
  params.needs_ack = false;
  params.flags = ViewHostMsg_UpdateRect_Flags::IS_REPAINT_ACK;
  ViewHostMsg_UpdateRect rect(test_rvh()->GetRoutingID(), params);
  RenderViewHostTester::TestOnMessageReceived(test_rvh(), rect);
  EXPECT_TRUE(overlay->received_paint_update_);
  EXPECT_FALSE(overlay->web_contents());
}

}  // namespace content
