// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit test for VideoCaptureBufferPool.

#include "content/browser/renderer_host/media/video_capture_buffer_pool.h"

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TEST(VideoCaptureBufferPoolTest, BufferPool) {
  const gfx::Size size = gfx::Size(640, 480);
  scoped_refptr<media::VideoFrame> non_pool_frame =
      media::VideoFrame::CreateFrame(media::VideoFrame::YV12, size,
                                     gfx::Rect(size), size, base::TimeDelta());
  scoped_refptr<VideoCaptureBufferPool> pool =
      new VideoCaptureBufferPool(size, 3);

  ASSERT_EQ(460800u, pool->GetMemorySize());

  ASSERT_TRUE(pool->Allocate());
  scoped_refptr<media::VideoFrame> frame1 = pool->ReserveForProducer(0);
  ASSERT_TRUE(NULL != frame1.get());
  ASSERT_EQ(size, frame1->coded_size());
  scoped_refptr<media::VideoFrame> frame2 = pool->ReserveForProducer(0);
  ASSERT_TRUE(NULL != frame2.get());
  ASSERT_EQ(size, frame2->coded_size());
  scoped_refptr<media::VideoFrame> frame3 = pool->ReserveForProducer(0);
  ASSERT_TRUE(NULL != frame3.get());

  // Touch the memory.
  media::FillYUV(frame1, 0x11, 0x22, 0x33);
  media::FillYUV(frame2, 0x44, 0x55, 0x66);
  media::FillYUV(frame3, 0x77, 0x88, 0x99);

  // Fourth frame should fail.
  ASSERT_EQ(NULL, pool->ReserveForProducer(0).get()) << "Pool should be empty";

  // Release 1st frame and retry; this should succeed.
  frame1 = NULL;
  scoped_refptr<media::VideoFrame> frame4 = pool->ReserveForProducer(0);
  ASSERT_TRUE(NULL != frame4.get());

  ASSERT_EQ(NULL, pool->ReserveForProducer(0).get()) << "Pool should be empty";

  // Validate the IDs
  int buffer_id2 = pool->RecognizeReservedBuffer(frame2);
  ASSERT_NE(0, buffer_id2);
  int buffer_id3 = pool->RecognizeReservedBuffer(frame3);
  ASSERT_NE(0, buffer_id3);
  int buffer_id4 = pool->RecognizeReservedBuffer(frame4);
  ASSERT_NE(0, buffer_id4);
  int buffer_id_non_pool = pool->RecognizeReservedBuffer(non_pool_frame);
  ASSERT_EQ(0, buffer_id_non_pool);

  ASSERT_FALSE(pool->IsAnyBufferHeldForConsumers());

  // Deliver a frame.
  pool->HoldForConsumers(frame3, buffer_id3, 2);

  ASSERT_TRUE(pool->IsAnyBufferHeldForConsumers());
  ASSERT_EQ(NULL, pool->ReserveForProducer(0).get()) << "Pool should be empty";
  frame3 = NULL;   // Old producer releases frame. Should be a noop.
  ASSERT_TRUE(pool->IsAnyBufferHeldForConsumers());
  ASSERT_EQ(NULL, pool->ReserveForProducer(0).get()) << "Pool should be empty";
  frame2 = NULL;  // Active producer releases frame. Should free a frame.
  buffer_id2 = 0;

  ASSERT_TRUE(pool->IsAnyBufferHeldForConsumers());
  frame1 = pool->ReserveForProducer(0);
  ASSERT_TRUE(NULL != frame1.get());
  ASSERT_EQ(NULL, pool->ReserveForProducer(0).get()) << "Pool should be empty";
  ASSERT_TRUE(pool->IsAnyBufferHeldForConsumers());

  // First consumer finishes.
  pool->RelinquishConsumerHold(buffer_id3, 1);
  ASSERT_EQ(NULL, pool->ReserveForProducer(0).get()) << "Pool should be empty";
  ASSERT_TRUE(pool->IsAnyBufferHeldForConsumers());

  // Second consumer finishes. This should free that frame.
  pool->RelinquishConsumerHold(buffer_id3, 1);
  ASSERT_FALSE(pool->IsAnyBufferHeldForConsumers());
  frame3 = pool->ReserveForProducer(0);
  ASSERT_TRUE(NULL != frame3);
  ASSERT_FALSE(pool->IsAnyBufferHeldForConsumers());
  ASSERT_EQ(NULL, pool->ReserveForProducer(0).get()) << "Pool should be empty";

  // Now deliver & consume frame1, but don't release the VideoFrame.
  int buffer_id1 = pool->RecognizeReservedBuffer(frame1);
  ASSERT_NE(0, buffer_id1);
  pool->HoldForConsumers(frame1, buffer_id1, 5);
  ASSERT_TRUE(pool->IsAnyBufferHeldForConsumers());
  pool->RelinquishConsumerHold(buffer_id1, 5);
  ASSERT_FALSE(pool->IsAnyBufferHeldForConsumers());

  // Even though the consumer is done with the buffer at |buffer_id1|, it cannot
  // be re-allocated to the producer, because |frame1| still references it. But
  // when |frame1| goes away, we should be able to re-reserve the buffer (and
  // the ID ought to be the same).
  ASSERT_EQ(NULL, pool->ReserveForProducer(0).get()) << "Pool should be empty";
  frame1 = NULL;  // Should free the frame.
  frame2 = pool->ReserveForProducer(0);
  ASSERT_TRUE(NULL != frame2);
  ASSERT_EQ(buffer_id1, pool->RecognizeReservedBuffer(frame2));
  ASSERT_EQ(NULL, pool->ReserveForProducer(0).get()) << "Pool should be empty";

  // For good measure, do one more cycle of free/realloc without delivery, now
  // that this buffer has been through the consumer-hold cycle.
  frame2 = NULL;
  frame1 = pool->ReserveForProducer(0);
  ASSERT_TRUE(NULL != frame1);
  ASSERT_EQ(buffer_id1, pool->RecognizeReservedBuffer(frame1));
  ASSERT_EQ(NULL, pool->ReserveForProducer(0).get()) << "Pool should be empty";

  // Tear down the pool, writing into the frames. The VideoFrame should
  // preserve the lifetime of the underlying memory.
  frame3 = NULL;
  pool = NULL;

  // Touch the memory.
  media::FillYUV(frame1, 0x11, 0x22, 0x33);
  media::FillYUV(frame4, 0x44, 0x55, 0x66);

  frame1 = NULL;

  media::FillYUV(frame4, 0x44, 0x55, 0x66);
  frame4 = NULL;
}

} // namespace content
