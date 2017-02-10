// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/cursor_renderer_mac.h"

#include <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "base/memory/ptr_util.h"
#include "base/test/simple_test_tick_clock.h"
#include "media/base/video_frame.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/mac/coordinate_conversion.h"
#include "ui/gfx/test/ui_cocoa_test_helper.h"

namespace content {

const int kTestViewWidth = 800;
const int kTestViewHeight = 600;

class CursorRendererMacTest : public ui::CocoaTest {
 public:
  CursorRendererMacTest() {}
  ~CursorRendererMacTest() override{};

  void SetUp() override {
    ui::CocoaTest::SetUp();
    base::scoped_nsobject<NSView> view([[NSView alloc]
        initWithFrame:NSMakeRect(0, 0, kTestViewWidth, kTestViewHeight)]);
    view_ = view.get();
    [[test_window() contentView] addSubview:view_];

    cursor_renderer_.reset(new CursorRendererMac(view_));
  }

  void TearDown() override {
    cursor_renderer_.reset();
    ui::CocoaTest::TearDown();
  }

  void SetTickClock(base::SimpleTestTickClock* clock) {
    cursor_renderer_->tick_clock_ = clock;
  }

  bool CursorDisplayed() { return cursor_renderer_->cursor_displayed_; }

  void RenderCursorOnVideoFrame(media::VideoFrame* target) {
    cursor_renderer_->RenderOnVideoFrame(target);
  }

  void SnapshotCursorState(gfx::Rect region_in_frame) {
    cursor_renderer_->SnapshotCursorState(region_in_frame);
  }

  // Here the |point| is in Aura coordinates (the origin (0, 0) is at top-left
  // of the view). To move the cursor to that point by Quartz Display service,
  // it needs to be converted into Cocoa coordinates (the origin is at
  // bottom-left of the main screen) first, and then info Quartz coordinates
  // (the origin is at top-left of the main display).
  void MoveMouseCursorWithinWindow(gfx::Point point) {
    point.set_y(kTestViewHeight - point.y());
    CGWarpMouseCursorPosition(gfx::ScreenPointToNSPoint(point));
    cursor_renderer_->OnMouseEvent();
  }

  void MoveMouseCursorWithinWindow() {
    CGWarpMouseCursorPosition(
        gfx::ScreenPointToNSPoint(gfx::Point(50, kTestViewHeight - 50)));
    cursor_renderer_->OnMouseEvent();

    CGWarpMouseCursorPosition(
        gfx::ScreenPointToNSPoint(gfx::Point(100, kTestViewHeight - 100)));
    cursor_renderer_->OnMouseEvent();
  }

  void MoveMouseCursorOutsideWindow() {
    CGWarpMouseCursorPosition(CGPointMake(1000, 200));
    cursor_renderer_->OnMouseEvent();
  }

  // A very simple test of whether there are any non-zero pixels
  // in the region |rect| within |frame|.
  bool NonZeroPixelsInRegion(scoped_refptr<media::VideoFrame> frame,
                             gfx::Rect rect) {
    bool y_found = false, u_found = false, v_found = false;
    for (int y = rect.y(); y < rect.bottom(); ++y) {
      uint8_t* yplane = frame->data(media::VideoFrame::kYPlane) +
                        y * frame->row_bytes(media::VideoFrame::kYPlane);
      uint8_t* uplane = frame->data(media::VideoFrame::kUPlane) +
                        (y / 2) * frame->row_bytes(media::VideoFrame::kUPlane);
      uint8_t* vplane = frame->data(media::VideoFrame::kVPlane) +
                        (y / 2) * frame->row_bytes(media::VideoFrame::kVPlane);
      for (int x = rect.x(); x < rect.right(); ++x) {
        if (yplane[x] != 0)
          y_found = true;
        if (uplane[x / 2])
          u_found = true;
        if (vplane[x / 2])
          v_found = true;
      }
    }
    return (y_found && u_found && v_found);
  }

 protected:
  NSView* view_;
  std::unique_ptr<CursorRendererMac> cursor_renderer_;
};

TEST_F(CursorRendererMacTest, CursorDuringMouseMovement) {
  // Keep window activated.
  [test_window() setPretendIsKeyWindow:YES];

  EXPECT_FALSE(CursorDisplayed());

  base::SimpleTestTickClock clock;
  SetTickClock(&clock);

  // Cursor displayed after mouse movement.
  MoveMouseCursorWithinWindow();
  EXPECT_TRUE(CursorDisplayed());

  // Cursor not be displayed after idle period.
  clock.SetNowTicks(base::TimeTicks::Now());
  clock.Advance(base::TimeDelta::FromSeconds(5));
  SnapshotCursorState(gfx::Rect(10, 10, 200, 200));
  EXPECT_FALSE(CursorDisplayed());
  clock.SetNowTicks(base::TimeTicks::Now());

  // Cursor displayed with mouse movement following idle period.
  MoveMouseCursorWithinWindow();
  SnapshotCursorState(gfx::Rect(10, 10, 200, 200));
  EXPECT_TRUE(CursorDisplayed());

  // Cursor not displayed if mouse outside the window
  MoveMouseCursorOutsideWindow();
  SnapshotCursorState(gfx::Rect(10, 10, 200, 200));
  EXPECT_FALSE(CursorDisplayed());
}

TEST_F(CursorRendererMacTest, CursorOnActiveWindow) {
  EXPECT_FALSE(CursorDisplayed());

  // Cursor displayed after mouse movement.
  [test_window() setPretendIsKeyWindow:YES];
  MoveMouseCursorWithinWindow();
  EXPECT_TRUE(CursorDisplayed());

  // Cursor not be displayed if window is not activated.
  [test_window() setPretendIsKeyWindow:NO];
  SnapshotCursorState(gfx::Rect(10, 10, 200, 200));
  EXPECT_FALSE(CursorDisplayed());

  // Cursor is displayed again if window is activated again.
  [test_window() setPretendIsKeyWindow:YES];
  MoveMouseCursorWithinWindow();
  SnapshotCursorState(gfx::Rect(10, 10, 200, 200));
  EXPECT_TRUE(CursorDisplayed());
}

TEST_F(CursorRendererMacTest, CursorRenderedOnFrame) {
  // Keep window activated.
  [test_window() setPretendIsKeyWindow:YES];

  EXPECT_FALSE(CursorDisplayed());

  gfx::Size size(kTestViewWidth, kTestViewHeight);
  scoped_refptr<media::VideoFrame> frame =
      media::VideoFrame::CreateZeroInitializedFrame(media::PIXEL_FORMAT_YV12,
                                                    size, gfx::Rect(size), size,
                                                    base::TimeDelta());

  MoveMouseCursorWithinWindow(gfx::Point(60, 60));
  SnapshotCursorState(gfx::Rect(size));
  EXPECT_TRUE(CursorDisplayed());

  EXPECT_FALSE(NonZeroPixelsInRegion(frame, gfx::Rect(50, 50, 70, 70)));
  RenderCursorOnVideoFrame(frame.get());
  EXPECT_TRUE(NonZeroPixelsInRegion(frame, gfx::Rect(50, 50, 70, 70)));
}

}  // namespace content
