// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_write_queue.h"

#include <cstddef>

#include "base/logging.h"
#include "net/spdy/spdy_buffer.h"
#include "net/spdy/spdy_buffer_producer.h"
#include "net/spdy/spdy_stream.h"

namespace net {

SpdyWriteQueue::PendingWrite::PendingWrite() : frame_producer(NULL) {}

SpdyWriteQueue::PendingWrite::PendingWrite(
    SpdyFrameType frame_type,
    SpdyBufferProducer* frame_producer,
    const scoped_refptr<SpdyStream>& stream)
    : frame_type(frame_type),
      frame_producer(frame_producer),
      stream(stream) {}

SpdyWriteQueue::PendingWrite::~PendingWrite() {}

SpdyWriteQueue::SpdyWriteQueue() {}

SpdyWriteQueue::~SpdyWriteQueue() {
  Clear();
}

void SpdyWriteQueue::Enqueue(RequestPriority priority,
                             SpdyFrameType frame_type,
                             scoped_ptr<SpdyBufferProducer> frame_producer,
                             const scoped_refptr<SpdyStream>& stream) {
  if (stream.get()) {
    DCHECK_EQ(stream->priority(), priority);
  }
  queue_[priority].push_back(
      PendingWrite(frame_type, frame_producer.release(), stream));
}

bool SpdyWriteQueue::Dequeue(SpdyFrameType* frame_type,
                             scoped_ptr<SpdyBufferProducer>* frame_producer,
                             scoped_refptr<SpdyStream>* stream) {
  for (int i = NUM_PRIORITIES - 1; i >= 0; --i) {
    if (!queue_[i].empty()) {
      PendingWrite pending_write = queue_[i].front();
      queue_[i].pop_front();
      *frame_type = pending_write.frame_type;
      frame_producer->reset(pending_write.frame_producer);
      *stream = pending_write.stream;
      return true;
    }
  }
  return false;
}

void SpdyWriteQueue::RemovePendingWritesForStream(
    const scoped_refptr<SpdyStream>& stream) {
  DCHECK(stream.get());
  if (DCHECK_IS_ON()) {
    // |stream| should not have pending writes in a queue not matching
    // its priority.
    for (int i = 0; i < NUM_PRIORITIES; ++i) {
      if (stream->priority() == i)
        continue;
      for (std::deque<PendingWrite>::const_iterator it = queue_[i].begin();
           it != queue_[i].end(); ++it) {
        DCHECK_NE(it->stream, stream);
      }
    }
  }

  // Do the actual deletion and removal, preserving FIFO-ness.
  std::deque<PendingWrite>* queue = &queue_[stream->priority()];
  std::deque<PendingWrite>::iterator out_it = queue->begin();
  for (std::deque<PendingWrite>::const_iterator it = queue->begin();
       it != queue->end(); ++it) {
    if (it->stream == stream) {
      delete it->frame_producer;
    } else {
      *out_it = *it;
      ++out_it;
    }
  }
  queue->erase(out_it, queue->end());
}

void SpdyWriteQueue::RemovePendingWritesForStreamsAfter(
    SpdyStreamId last_good_stream_id) {
  for (int i = 0; i < NUM_PRIORITIES; ++i) {
    // Do the actual deletion and removal, preserving FIFO-ness.
    std::deque<PendingWrite>* queue = &queue_[i];
    std::deque<PendingWrite>::iterator out_it = queue->begin();
    for (std::deque<PendingWrite>::const_iterator it = queue->begin();
         it != queue->end(); ++it) {
      if (it->stream && (it->stream->stream_id() > last_good_stream_id ||
                         it->stream->stream_id() == 0)) {
        delete it->frame_producer;
      } else {
        *out_it = *it;
        ++out_it;
      }
    }
    queue->erase(out_it, queue->end());
  }
}

void SpdyWriteQueue::Clear() {
  for (int i = 0; i < NUM_PRIORITIES; ++i) {
    for (std::deque<PendingWrite>::iterator it = queue_[i].begin();
         it != queue_[i].end(); ++it) {
      delete it->frame_producer;
    }
    queue_[i].clear();
  }
}

}  // namespace net
