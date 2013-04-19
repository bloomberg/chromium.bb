// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_write_queue.h"

#include <cstddef>
#include <cstring>
#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "net/base/net_log.h"
#include "net/base/request_priority.h"
#include "net/spdy/spdy_buffer_producer.h"
#include "net/spdy/spdy_stream.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

class SpdyWriteQueueTest : public ::testing::Test {};

// Makes a SpdyFrameProducer producing a frame with the data in the
// given string.
scoped_ptr<SpdyBufferProducer> StringToProducer(const std::string& s) {
  scoped_ptr<char[]> data(new char[s.size()]);
  std::memcpy(data.get(), s.data(), s.size());
  return scoped_ptr<SpdyBufferProducer>(
      new SimpleBufferProducer(
          scoped_ptr<SpdyBuffer>(
              new SpdyBuffer(
                  scoped_ptr<SpdyFrame>(
                      new SpdyFrame(data.release(), s.size(), true))))));
}

// Makes a SpdyBufferProducer producing a frame with the data in the
// given int (converted to a string).
scoped_ptr<SpdyBufferProducer> IntToProducer(int i) {
  return StringToProducer(base::IntToString(i));
}

// Produces a frame with the given producer and returns a copy of its
// data as a string.
std::string ProducerToString(scoped_ptr<SpdyBufferProducer> producer) {
  scoped_ptr<SpdyBuffer> buffer = producer->ProduceBuffer();
  return std::string(buffer->GetRemainingData(), buffer->GetRemainingSize());
}

// Produces a frame with the given producer and returns a copy of its
// data as an int (converted from a string).
int ProducerToInt(scoped_ptr<SpdyBufferProducer> producer) {
  int i = 0;
  EXPECT_TRUE(base::StringToInt(ProducerToString(producer.Pass()), &i));
  return i;
}

// Makes a SpdyStream with the given priority and a NULL SpdySession
// -- be careful to not call any functions that expect the session to
// be there.
SpdyStream* MakeTestStream(RequestPriority priority) {
  return new SpdyStream(
      NULL, std::string(), priority, 0, 0, false, BoundNetLog());
}

// Add some frame producers of different priority. The producers
// should be dequeued in priority order with their associated stream.
TEST_F(SpdyWriteQueueTest, DequeuesByPriority) {
  SpdyWriteQueue write_queue;

  scoped_ptr<SpdyBufferProducer> producer_low = StringToProducer("LOW");
  scoped_ptr<SpdyBufferProducer> producer_medium = StringToProducer("MEDIUM");
  scoped_ptr<SpdyBufferProducer> producer_highest = StringToProducer("HIGHEST");

  // A NULL stream should still work.
  scoped_refptr<SpdyStream> stream_low(NULL);
  scoped_refptr<SpdyStream> stream_medium(MakeTestStream(MEDIUM));
  scoped_refptr<SpdyStream> stream_highest(MakeTestStream(HIGHEST));

  write_queue.Enqueue(
      LOW, SYN_STREAM, producer_low.Pass(), stream_low);
  write_queue.Enqueue(
      MEDIUM, SYN_REPLY, producer_medium.Pass(), stream_medium);
  write_queue.Enqueue(
      HIGHEST, RST_STREAM, producer_highest.Pass(), stream_highest);

  SpdyFrameType frame_type = DATA;
  scoped_ptr<SpdyBufferProducer> frame_producer;
  scoped_refptr<SpdyStream> stream;
  ASSERT_TRUE(write_queue.Dequeue(&frame_type, &frame_producer, &stream));
  EXPECT_EQ(RST_STREAM, frame_type);
  EXPECT_EQ("HIGHEST", ProducerToString(frame_producer.Pass()));
  EXPECT_EQ(stream_highest, stream);

  ASSERT_TRUE(write_queue.Dequeue(&frame_type, &frame_producer, &stream));
  EXPECT_EQ(SYN_REPLY, frame_type);
  EXPECT_EQ("MEDIUM", ProducerToString(frame_producer.Pass()));
  EXPECT_EQ(stream_medium, stream);

  ASSERT_TRUE(write_queue.Dequeue(&frame_type, &frame_producer, &stream));
  EXPECT_EQ(SYN_STREAM, frame_type);
  EXPECT_EQ("LOW", ProducerToString(frame_producer.Pass()));
  EXPECT_EQ(stream_low, stream);

  EXPECT_FALSE(write_queue.Dequeue(&frame_type, &frame_producer, &stream));
}

// Add some frame producers with the same priority. The producers
// should be dequeued in FIFO order with their associated stream.
TEST_F(SpdyWriteQueueTest, DequeuesFIFO) {
  SpdyWriteQueue write_queue;

  scoped_ptr<SpdyBufferProducer> producer1 = IntToProducer(1);
  scoped_ptr<SpdyBufferProducer> producer2 = IntToProducer(2);
  scoped_ptr<SpdyBufferProducer> producer3 = IntToProducer(3);

  scoped_refptr<SpdyStream> stream1(MakeTestStream(DEFAULT_PRIORITY));
  scoped_refptr<SpdyStream> stream2(MakeTestStream(DEFAULT_PRIORITY));
  scoped_refptr<SpdyStream> stream3(MakeTestStream(DEFAULT_PRIORITY));

  write_queue.Enqueue(DEFAULT_PRIORITY, SYN_STREAM, producer1.Pass(), stream1);
  write_queue.Enqueue(DEFAULT_PRIORITY, SYN_REPLY, producer2.Pass(), stream2);
  write_queue.Enqueue(DEFAULT_PRIORITY, RST_STREAM, producer3.Pass(), stream3);

  SpdyFrameType frame_type = DATA;
  scoped_ptr<SpdyBufferProducer> frame_producer;
  scoped_refptr<SpdyStream> stream;
  ASSERT_TRUE(write_queue.Dequeue(&frame_type, &frame_producer, &stream));
  EXPECT_EQ(SYN_STREAM, frame_type);
  EXPECT_EQ(1, ProducerToInt(frame_producer.Pass()));
  EXPECT_EQ(stream1, stream);

  ASSERT_TRUE(write_queue.Dequeue(&frame_type, &frame_producer, &stream));
  EXPECT_EQ(SYN_REPLY, frame_type);
  EXPECT_EQ(2, ProducerToInt(frame_producer.Pass()));
  EXPECT_EQ(stream2, stream);

  ASSERT_TRUE(write_queue.Dequeue(&frame_type, &frame_producer, &stream));
  EXPECT_EQ(RST_STREAM, frame_type);
  EXPECT_EQ(3, ProducerToInt(frame_producer.Pass()));
  EXPECT_EQ(stream3, stream);

  EXPECT_FALSE(write_queue.Dequeue(&frame_type, &frame_producer, &stream));
}

// Enqueue a bunch of writes and then call
// RemovePendingWritesForStream() on one of the streams. No dequeued
// write should be for that stream.
TEST_F(SpdyWriteQueueTest, RemovePendingWritesForStream) {
  SpdyWriteQueue write_queue;

  scoped_refptr<SpdyStream> stream1(MakeTestStream(DEFAULT_PRIORITY));
  scoped_refptr<SpdyStream> stream2(MakeTestStream(DEFAULT_PRIORITY));

  for (int i = 0; i < 100; ++i) {
    scoped_refptr<SpdyStream> stream = ((i % 3) == 0) ? stream1 : stream2;
    write_queue.Enqueue(DEFAULT_PRIORITY, SYN_STREAM, IntToProducer(i), stream);
  }

  write_queue.RemovePendingWritesForStream(stream2);

  for (int i = 0; i < 100; i += 3) {
    SpdyFrameType frame_type = DATA;
    scoped_ptr<SpdyBufferProducer> frame_producer;
    scoped_refptr<SpdyStream> stream;
    ASSERT_TRUE(write_queue.Dequeue(&frame_type, &frame_producer, &stream));
    EXPECT_EQ(SYN_STREAM, frame_type);
    EXPECT_EQ(i, ProducerToInt(frame_producer.Pass()));
    EXPECT_EQ(stream1, stream);
  }

  SpdyFrameType frame_type = DATA;
  scoped_ptr<SpdyBufferProducer> frame_producer;
  scoped_refptr<SpdyStream> stream;
  EXPECT_FALSE(write_queue.Dequeue(&frame_type, &frame_producer, &stream));
}

// Enqueue a bunch of writes and then call
// RemovePendingWritesForStreamsAfter(). No dequeued write should be for
// those streams without a stream id, or with a stream_id after that
// argument.
TEST_F(SpdyWriteQueueTest, RemovePendingWritesForStreamsAfter) {
  SpdyWriteQueue write_queue;

  scoped_refptr<SpdyStream> stream1(MakeTestStream(DEFAULT_PRIORITY));
  stream1->set_stream_id(1);
  scoped_refptr<SpdyStream> stream2(MakeTestStream(DEFAULT_PRIORITY));
  stream2->set_stream_id(3);
  scoped_refptr<SpdyStream> stream3(MakeTestStream(DEFAULT_PRIORITY));
  stream3->set_stream_id(5);
  // No stream id assigned.
  scoped_refptr<SpdyStream> stream4(MakeTestStream(DEFAULT_PRIORITY));
  scoped_refptr<SpdyStream> streams[] = {
    stream1, stream2, stream3, stream4
  };

  for (int i = 0; i < 100; ++i) {
    scoped_refptr<SpdyStream> stream = streams[i % arraysize(streams)];
    write_queue.Enqueue(DEFAULT_PRIORITY, SYN_STREAM, IntToProducer(i), stream);
  }

  write_queue.RemovePendingWritesForStreamsAfter(stream1->stream_id());

  for (int i = 0; i < 100; i += arraysize(streams)) {
    SpdyFrameType frame_type = DATA;
    scoped_ptr<SpdyBufferProducer> frame_producer;
    scoped_refptr<SpdyStream> stream;
    ASSERT_TRUE(write_queue.Dequeue(&frame_type, &frame_producer, &stream))
        << "Unable to Dequeue i: " << i;
    EXPECT_EQ(SYN_STREAM, frame_type);
    EXPECT_EQ(i, ProducerToInt(frame_producer.Pass()));
    EXPECT_EQ(stream1, stream);
  }

  SpdyFrameType frame_type = DATA;
  scoped_ptr<SpdyBufferProducer> frame_producer;
  scoped_refptr<SpdyStream> stream;
  EXPECT_FALSE(write_queue.Dequeue(&frame_type, &frame_producer, &stream));
}

// Enqueue a bunch of writes and then call Clear(). The write queue
// should clean up the memory properly, and Dequeue() should return
// false.
TEST_F(SpdyWriteQueueTest, Clear) {
  SpdyWriteQueue write_queue;

  for (int i = 0; i < 100; ++i) {
    write_queue.Enqueue(DEFAULT_PRIORITY, SYN_STREAM, IntToProducer(i), NULL);
  }

  write_queue.Clear();

  SpdyFrameType frame_type = DATA;
  scoped_ptr<SpdyBufferProducer> frame_producer;
  scoped_refptr<SpdyStream> stream;
  EXPECT_FALSE(write_queue.Dequeue(&frame_type, &frame_producer, &stream));
}

}  // namespace

}  // namespace net
