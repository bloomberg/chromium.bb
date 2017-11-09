// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/video_capture/interprocess_frame_pool.h"

#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

using media::VideoFrame;

namespace viz {
namespace {

constexpr gfx::Size kSize = gfx::Size(32, 18);
constexpr media::VideoPixelFormat kFormat = media::PIXEL_FORMAT_I420;

TEST(InterprocessFramePoolTest, FramesConfiguredCorrectly) {
  InterprocessFramePool pool(1);
  const scoped_refptr<media::VideoFrame> frame =
      pool.ReserveVideoFrame(kSize, kFormat);
  ASSERT_TRUE(frame);
  ASSERT_EQ(kSize, frame->coded_size());
  ASSERT_EQ(gfx::Rect(kSize), frame->visible_rect());
  ASSERT_EQ(kSize, frame->natural_size());
  ASSERT_EQ(frame->storage_type(), VideoFrame::STORAGE_MOJO_SHARED_BUFFER);
  ASSERT_TRUE(frame->IsMappable());
}

TEST(InterprocessFramePoolTest, ReachesCapacityLimit) {
  InterprocessFramePool pool(2);
  scoped_refptr<media::VideoFrame> frames[5];

  // Reserve two frames from a pool of capacity 2.
  frames[0] = pool.ReserveVideoFrame(kSize, kFormat);
  ASSERT_TRUE(frames[0]);
  frames[1] = pool.ReserveVideoFrame(kSize, kFormat);
  ASSERT_TRUE(frames[1]);

  // Now, try to reserve a third frame. This should fail (return null).
  frames[2] = pool.ReserveVideoFrame(kSize, kFormat);
  ASSERT_FALSE(frames[2]);

  // Release the first frame. Then, retry reserving a frame. This should
  // succeed.
  frames[0] = nullptr;
  frames[3] = pool.ReserveVideoFrame(kSize, kFormat);
  ASSERT_TRUE(frames[3]);

  // Finally, try to reserve yet another frame. This should fail.
  frames[4] = pool.ReserveVideoFrame(kSize, kFormat);
  ASSERT_FALSE(frames[4]);
}

// Returns true iff each plane of the given |frame| is filled with
// |values[plane]|.
bool PlanesAreFilledWithValues(const VideoFrame& frame, const uint8_t* values) {
  static_assert(VideoFrame::kUPlane == (VideoFrame::kYPlane + 1) &&
                    VideoFrame::kVPlane == (VideoFrame::kUPlane + 1),
                "enum values changed, will break code below");
  for (int plane = VideoFrame::kYPlane; plane <= VideoFrame::kVPlane; ++plane) {
    const uint8_t expected_value = values[plane - VideoFrame::kYPlane];
    for (int y = 0; y < frame.rows(plane); ++y) {
      const uint8_t* row = frame.visible_data(plane) + y * frame.stride(plane);
      for (int x = 0; x < frame.row_bytes(plane); ++x) {
        EXPECT_EQ(expected_value, row[x])
            << "at row " << y << " in plane " << plane;
        if (expected_value != row[x])
          return false;
      }
    }
  }
  return true;
}

TEST(InterprocessFramePoolTest, ResurrectsDeliveredFramesOnly) {
  InterprocessFramePool pool(2);

  // Reserve a frame, populate it, but release it before delivery.
  scoped_refptr<media::VideoFrame> frame =
      pool.ReserveVideoFrame(kSize, kFormat);
  ASSERT_TRUE(frame);
  media::FillYUV(frame.get(), 0x11, 0x22, 0x33);
  frame = nullptr;

  // The pool should fail to resurrect the last frame because it was never
  // delivered.
  frame = pool.ResurrectLastVideoFrame(kSize, kFormat);
  ASSERT_FALSE(frame);

  // Reserve a frame and populate it with different color values; only this
  // time, deliver it before releasing it.
  frame = pool.ReserveVideoFrame(kSize, kFormat);
  ASSERT_TRUE(frame);
  const uint8_t kValues[3] = {0x44, 0x55, 0x66};
  media::FillYUV(frame.get(), kValues[0], kValues[1], kValues[2]);
  base::OnceClosure post_delivery_callback =
      pool.HoldFrameForDelivery(frame.get());
  frame = nullptr;
  std::move(post_delivery_callback).Run();

  // Confirm that the last frame can be resurrected repeatedly.
  for (int i = 0; i < 3; ++i) {
    frame = pool.ResurrectLastVideoFrame(kSize, kFormat);
    ASSERT_TRUE(frame);
    ASSERT_TRUE(PlanesAreFilledWithValues(*frame, kValues));
    frame = nullptr;
  }

  // A frame that is being delivered cannot be resurrected.
  for (int i = 0; i < 2; ++i) {
    if (i == 0) {  // Test this for a resurrected frame.
      frame = pool.ResurrectLastVideoFrame(kSize, kFormat);
      ASSERT_TRUE(frame);
      ASSERT_TRUE(PlanesAreFilledWithValues(*frame, kValues));
    } else {  // Test this for a normal frame.
      frame = pool.ReserveVideoFrame(kSize, kFormat);
      ASSERT_TRUE(frame);
      media::FillYUV(frame.get(), 0x77, 0x88, 0x99);
    }
    post_delivery_callback = pool.HoldFrameForDelivery(frame.get());
    frame = nullptr;
    scoped_refptr<media::VideoFrame> should_be_null =
        pool.ResurrectLastVideoFrame(kSize, kFormat);
    ASSERT_FALSE(should_be_null);
    std::move(post_delivery_callback).Run();
  }

  // Finally, reserve a frame, populate it, and don't deliver it. Expect that,
  // still, undelivered frames cannot be resurrected.
  frame = pool.ReserveVideoFrame(kSize, kFormat);
  ASSERT_TRUE(frame);
  media::FillYUV(frame.get(), 0xaa, 0xbb, 0xcc);
  frame = nullptr;
  frame = pool.ResurrectLastVideoFrame(kSize, kFormat);
  ASSERT_FALSE(frame);
}

TEST(InterprocessFramePoolTest, ReportsCorrectUtilization) {
  InterprocessFramePool pool(2);
  ASSERT_EQ(0.0f, pool.GetUtilization());

  // Run through a typical sequence twice: Once for normal frame reservation,
  // and the second time for a resurrected frame.
  for (int i = 0; i < 2; ++i) {
    // Reserve the frame and expect 1/2 the pool to be utilized.
    scoped_refptr<media::VideoFrame> frame =
        (i == 0) ? pool.ReserveVideoFrame(kSize, kFormat)
                 : pool.ResurrectLastVideoFrame(kSize, kFormat);
    ASSERT_TRUE(frame);
    ASSERT_EQ(0.5f, pool.GetUtilization());

    // Hold the frame for delivery. This should not change the utilization.
    base::OnceClosure post_delivery_callback =
        pool.HoldFrameForDelivery(frame.get());
    ASSERT_EQ(0.5f, pool.GetUtilization());

    // Release the frame. Since it is being held for delivery, this should not
    // change the utilization.
    frame = nullptr;
    ASSERT_EQ(0.5f, pool.GetUtilization());

    // Finally, run the callback to indicate the frame has been delivered. This
    // should cause the utilization to go back down to zero.
    std::move(post_delivery_callback).Run();
    ASSERT_EQ(0.0f, pool.GetUtilization());
  }
}

}  // namespace
}  // namespace viz
