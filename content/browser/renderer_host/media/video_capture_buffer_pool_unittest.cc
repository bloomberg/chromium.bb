// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit test for VideoCaptureBufferPool.

#include "content/browser/renderer_host/media/video_capture_buffer_pool.h"

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/renderer_host/media/video_capture_controller.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class VideoCaptureBufferPoolTest : public testing::Test {
 protected:
  VideoCaptureBufferPoolTest()
      : expected_dropped_id_(0),
        pool_(new VideoCaptureBufferPool(3)) {}

  void ExpectDroppedId(int expected_dropped_id) {
    expected_dropped_id_ = expected_dropped_id;
  }

  scoped_refptr<media::VideoFrame> ReserveI420VideoFrame(
      const gfx::Size& size) {
    // To verify that ReserveI420VideoFrame always sets |buffer_id_to_drop|,
    // initialize it to something different than the expected value.
    int buffer_id_to_drop = ~expected_dropped_id_;
    scoped_refptr<media::VideoFrame> frame =
        pool_->ReserveI420VideoFrame(size, 0, &buffer_id_to_drop);
    EXPECT_EQ(expected_dropped_id_, buffer_id_to_drop)
        << "Unexpected buffer reallocation result.";
    return frame;
  }

  int expected_dropped_id_;
  scoped_refptr<VideoCaptureBufferPool> pool_;

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoCaptureBufferPoolTest);
};

TEST_F(VideoCaptureBufferPoolTest, BufferPool) {
  const gfx::Size size_lo = gfx::Size(640, 480);
  const gfx::Size size_hi = gfx::Size(1024, 768);
  scoped_refptr<media::VideoFrame> non_pool_frame =
      media::VideoFrame::CreateFrame(media::VideoFrame::YV12, size_lo,
                                     gfx::Rect(size_lo), size_lo,
                                     base::TimeDelta());

  // Reallocation won't happen for the first part of the test.
  ExpectDroppedId(VideoCaptureBufferPool::kInvalidId);

  scoped_refptr<media::VideoFrame> frame1 = ReserveI420VideoFrame(size_lo);
  ASSERT_TRUE(NULL != frame1.get());
  ASSERT_EQ(size_lo, frame1->coded_size());
  scoped_refptr<media::VideoFrame> frame2 = ReserveI420VideoFrame(size_lo);
  ASSERT_TRUE(NULL != frame2.get());
  ASSERT_EQ(size_lo, frame2->coded_size());
  scoped_refptr<media::VideoFrame> frame3 = ReserveI420VideoFrame(size_lo);
  ASSERT_TRUE(NULL != frame3.get());

  // Touch the memory.
  media::FillYUV(frame1.get(), 0x11, 0x22, 0x33);
  media::FillYUV(frame2.get(), 0x44, 0x55, 0x66);
  media::FillYUV(frame3.get(), 0x77, 0x88, 0x99);

  // Fourth frame should fail.
  ASSERT_FALSE(ReserveI420VideoFrame(size_lo)) << "Pool should be empty";

  // Release 1st frame and retry; this should succeed.
  frame1 = NULL;
  scoped_refptr<media::VideoFrame> frame4 = ReserveI420VideoFrame(size_lo);
  ASSERT_TRUE(NULL != frame4.get());

  ASSERT_FALSE(ReserveI420VideoFrame(size_lo)) << "Pool should be empty";
  ASSERT_FALSE(ReserveI420VideoFrame(size_hi)) << "Pool should be empty";

  // Validate the IDs
  int buffer_id2 =
      pool_->RecognizeReservedBuffer(frame2->shared_memory_handle());
  ASSERT_EQ(1, buffer_id2);
  int buffer_id3 =
      pool_->RecognizeReservedBuffer(frame3->shared_memory_handle());
  base::SharedMemoryHandle memory_handle3 = frame3->shared_memory_handle();
  ASSERT_EQ(2, buffer_id3);
  int buffer_id4 =
      pool_->RecognizeReservedBuffer(frame4->shared_memory_handle());
  ASSERT_EQ(0, buffer_id4);
  int buffer_id_non_pool =
      pool_->RecognizeReservedBuffer(non_pool_frame->shared_memory_handle());
  ASSERT_EQ(VideoCaptureBufferPool::kInvalidId, buffer_id_non_pool);

  // Deliver a frame.
  pool_->HoldForConsumers(buffer_id3, 2);

  ASSERT_FALSE(ReserveI420VideoFrame(size_lo)) << "Pool should be empty";

  frame3 = NULL;   // Old producer releases frame. Should be a noop.
  ASSERT_FALSE(ReserveI420VideoFrame(size_lo)) << "Pool should be empty";
  ASSERT_FALSE(ReserveI420VideoFrame(size_hi)) << "Pool should be empty";

  frame2 = NULL;  // Active producer releases frame. Should free a frame.

  frame1 = ReserveI420VideoFrame(size_lo);
  ASSERT_TRUE(NULL != frame1.get());
  ASSERT_FALSE(ReserveI420VideoFrame(size_lo)) << "Pool should be empty";

  // First consumer finishes.
  pool_->RelinquishConsumerHold(buffer_id3, 1);
  ASSERT_FALSE(ReserveI420VideoFrame(size_lo)) << "Pool should be empty";

  // Second consumer finishes. This should free that frame.
  pool_->RelinquishConsumerHold(buffer_id3, 1);
  frame3 = ReserveI420VideoFrame(size_lo);
  ASSERT_TRUE(NULL != frame3.get());
  ASSERT_EQ(buffer_id3,
            pool_->RecognizeReservedBuffer(frame3->shared_memory_handle()))
      << "Buffer ID should be reused.";
  ASSERT_EQ(memory_handle3, frame3->shared_memory_handle());
  ASSERT_FALSE(ReserveI420VideoFrame(size_lo)) << "Pool should be empty";

  // Now deliver & consume frame1, but don't release the VideoFrame.
  int buffer_id1 =
      pool_->RecognizeReservedBuffer(frame1->shared_memory_handle());
  ASSERT_EQ(1, buffer_id1);
  pool_->HoldForConsumers(buffer_id1, 5);
  pool_->RelinquishConsumerHold(buffer_id1, 5);

  // Even though the consumer is done with the buffer at |buffer_id1|, it cannot
  // be re-allocated to the producer, because |frame1| still references it. But
  // when |frame1| goes away, we should be able to re-reserve the buffer (and
  // the ID ought to be the same).
  ASSERT_FALSE(ReserveI420VideoFrame(size_lo)) << "Pool should be empty";
  frame1 = NULL;  // Should free the frame.
  frame2 = ReserveI420VideoFrame(size_lo);
  ASSERT_TRUE(NULL != frame2.get());
  ASSERT_EQ(buffer_id1,
            pool_->RecognizeReservedBuffer(frame2->shared_memory_handle()));
  buffer_id2 = buffer_id1;
  ASSERT_FALSE(ReserveI420VideoFrame(size_lo)) << "Pool should be empty";

  // Now try reallocation with different resolutions. We expect reallocation
  // to occur only when the old buffer is too small.
  frame2 = NULL;
  ExpectDroppedId(buffer_id2);
  frame2 = ReserveI420VideoFrame(size_hi);
  ASSERT_TRUE(NULL != frame2.get());
  ASSERT_TRUE(frame2->coded_size() == size_hi);
  ASSERT_EQ(3, pool_->RecognizeReservedBuffer(frame2->shared_memory_handle()));
  base::SharedMemoryHandle memory_handle_hi = frame2->shared_memory_handle();
  frame2 = NULL;  // Frees it.
  ExpectDroppedId(VideoCaptureBufferPool::kInvalidId);
  frame2 = ReserveI420VideoFrame(size_lo);
  base::SharedMemoryHandle memory_handle_lo = frame2->shared_memory_handle();
  ASSERT_EQ(memory_handle_hi, memory_handle_lo)
      << "Decrease in resolution should not reallocate buffer";
  ASSERT_TRUE(NULL != frame2.get());
  ASSERT_TRUE(frame2->coded_size() == size_lo);
  ASSERT_EQ(3, pool_->RecognizeReservedBuffer(frame2->shared_memory_handle()));
  ASSERT_FALSE(ReserveI420VideoFrame(size_lo)) << "Pool should be empty";

  // Tear down the pool_, writing into the frames. The VideoFrame should
  // preserve the lifetime of the underlying memory.
  frame3 = NULL;
  pool_ = NULL;

  // Touch the memory.
  media::FillYUV(frame2.get(), 0x11, 0x22, 0x33);
  media::FillYUV(frame4.get(), 0x44, 0x55, 0x66);

  frame2 = NULL;

  media::FillYUV(frame4.get(), 0x44, 0x55, 0x66);
  frame4 = NULL;
}

} // namespace content
