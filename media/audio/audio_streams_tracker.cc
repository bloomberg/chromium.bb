// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_streams_tracker.h"

#include <limits>

namespace media {

AudioStreamsTracker::AudioStreamsTracker()
    : current_stream_count_(0), max_stream_count_(0) {
  thread_checker_.DetachFromThread();
}

AudioStreamsTracker::~AudioStreamsTracker() {
  DCHECK_EQ(current_stream_count_, 0u);
}

void AudioStreamsTracker::IncreaseStreamCount() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(current_stream_count_, std::numeric_limits<size_t>::max());
  ++current_stream_count_;
  if (current_stream_count_ > max_stream_count_)
    max_stream_count_ = current_stream_count_;
}

void AudioStreamsTracker::DecreaseStreamCount(size_t count) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_GE(current_stream_count_, count);
  current_stream_count_ -= count;
}

void AudioStreamsTracker::ResetMaxStreamCount() {
  DCHECK(thread_checker_.CalledOnValidThread());
  max_stream_count_ = current_stream_count_;
}

size_t AudioStreamsTracker::max_stream_count() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return max_stream_count_;
}

}  // namespace media
