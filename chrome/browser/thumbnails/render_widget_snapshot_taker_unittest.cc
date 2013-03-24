// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/thumbnails/render_widget_snapshot_taker.h"

#include "chrome/test/base/chrome_render_view_host_test_harness.h"

class RenderWidgetSnapshotTakerTest : public ChromeRenderViewHostTestHarness {
 public:
  RenderWidgetSnapshotTakerTest() : snapshot_ready_called_(false) {}

  void SnapshotReady(const SkBitmap& bitmap) {
    snapshot_ready_called_ = true;
  }

  bool snapshot_ready_called() const {
    return snapshot_ready_called_;
  }

 private:
  bool snapshot_ready_called_;
};

#if defined(USE_X11)
// RenderWidgetHost::AskForSnapshot is not implemented for X11
// (http://crbug.com/89777).
#define MAYBE_WidgetDidReceivePaintAtSizeAck \
    DISABLED_WidgetDidReceivePaintAtSizeAck
#else
#define MAYBE_WidgetDidReceivePaintAtSizeAck WidgetDidReceivePaintAtSizeAck
#endif

// Just checks the callback runs in WidgetDidReceivePaintAtSizeAck.
TEST_F(RenderWidgetSnapshotTakerTest, MAYBE_WidgetDidReceivePaintAtSizeAck) {
  RenderWidgetSnapshotTaker snapshot_taker;
  const gfx::Size size(100, 100);
  snapshot_taker.AskForSnapshot(
      rvh(),
      base::Bind(&RenderWidgetSnapshotTakerTest::SnapshotReady,
                 base::Unretained(this)),
      size,
      size);
  EXPECT_EQ(1U, snapshot_taker.callback_map_.size());
  const int sequence_num = 1;
  snapshot_taker.WidgetDidReceivePaintAtSizeAck(
      content::RenderViewHostTestHarness::rvh(),
      sequence_num,
      size);
  EXPECT_TRUE(snapshot_taker.callback_map_.empty());
  EXPECT_TRUE(snapshot_ready_called());
}

#if defined(USE_X11)
// RenderWidgetHost::AskForSnapshot is not implemented for X11
// (http://crbug.com/89777).
#define MAYBE_WidgetDidReceivePaintAtSizeAckFail \
    DISABLED_WidgetDidReceivePaintAtSizeAckFail
#else
#define MAYBE_WidgetDidReceivePaintAtSizeAckFail \
    WidgetDidReceivePaintAtSizeAckFail
#endif

// Checks the case where RenderWidgetSnapshotTaker receives an ack with a wrong
// size. The should result in a failure and the callback should not be called.
TEST_F(RenderWidgetSnapshotTakerTest,
    MAYBE_WidgetDidReceivePaintAtSizeAckFail) {
  RenderWidgetSnapshotTaker snapshot_taker;
  const gfx::Size size(100, 100);
  snapshot_taker.AskForSnapshot(
      rvh(),
      base::Bind(&RenderWidgetSnapshotTakerTest::SnapshotReady,
                 base::Unretained(this)),
      size,
      size);
  EXPECT_EQ(1U, snapshot_taker.callback_map_.size());
  const int sequence_num = 1;
  const gfx::Size size2(200, 200);
  snapshot_taker.WidgetDidReceivePaintAtSizeAck(
      content::RenderViewHostTestHarness::rvh(),
      sequence_num,
      size2);
  EXPECT_FALSE(snapshot_taker.callback_map_.empty());
  EXPECT_FALSE(snapshot_ready_called());
}
