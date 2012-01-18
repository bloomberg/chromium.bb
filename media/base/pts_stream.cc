// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "media/base/buffers.h"
#include "media/base/pts_stream.h"

namespace media {

PtsStream::PtsStream() {}

PtsStream::~PtsStream() {}

void PtsStream::Initialize(const base::TimeDelta& frame_duration) {
  default_duration_ = frame_duration;
  current_pts_ = base::TimeDelta();
  current_duration_ = base::TimeDelta();
}

void PtsStream::Seek(const base::TimeDelta& timestamp) {
  current_pts_ = timestamp;
  current_duration_ = base::TimeDelta();
  Flush();
}

void PtsStream::Flush() {
  while (!pts_heap_.IsEmpty())
    pts_heap_.Pop();
}

void PtsStream::EnqueuePts(StreamSample* sample) {
  DCHECK(sample);
  if (!sample->IsEndOfStream() && sample->GetTimestamp() != kNoTimestamp()) {
    pts_heap_.Push(sample->GetTimestamp());
  }
}

void PtsStream::UpdatePtsAndDuration(StreamSample* sample) {
  // First search the |sample| for the pts. This is the most authoritative.
  // Make a special exclusion for the value pts == 0.  Though this is
  // technically a valid value, it seems a number of FFmpeg codecs will
  // mistakenly always set pts to 0.
  //
  // TODO(scherkus): FFmpegVideoDecodeEngine should be able to detect this
  // situation and set the timestamp to kInvalidTimestamp.
  DCHECK(sample);
  base::TimeDelta timestamp = sample->GetTimestamp();
  if (timestamp != kNoTimestamp() &&
      timestamp.ToInternalValue() != 0) {
    current_pts_ = timestamp;
    // We need to clean up the timestamp we pushed onto the |pts_heap_|.
    if (!pts_heap_.IsEmpty())
      pts_heap_.Pop();
  } else if (!pts_heap_.IsEmpty()) {
    // If the frame did not have pts, try to get the pts from the |pts_heap|.
    current_pts_ = pts_heap_.Top();
    pts_heap_.Pop();
  } else if (current_pts_ != kNoTimestamp()) {
    // Guess assuming this frame was the same as the last frame.
    current_pts_ = current_pts_ + current_duration_;
  } else {
    // Now we really have no clue!!!  Mark an invalid timestamp and let the
    // video renderer handle it (i.e., drop frame).
    current_pts_ = kNoTimestamp();
  }

  // Fill in the duration, using the frame itself as the authoratative source.
  base::TimeDelta duration = sample->GetDuration();
  if (duration != kNoTimestamp() &&
      duration.ToInternalValue() != 0) {
    current_duration_ = duration;
  } else {
    // Otherwise assume a normal frame duration.
    current_duration_ = default_duration_;
  }
}

}  // namespace media
