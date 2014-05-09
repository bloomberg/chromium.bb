// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/frame_processor_base.h"

#include "base/stl_util.h"
#include "media/base/buffers.h"

namespace media {

MseTrackBuffer::MseTrackBuffer(ChunkDemuxerStream* stream)
    : last_decode_timestamp_(kNoTimestamp()),
      last_frame_duration_(kNoTimestamp()),
      highest_presentation_timestamp_(kNoTimestamp()),
      needs_random_access_point_(true),
      stream_(stream) {
  DCHECK(stream_);
}

MseTrackBuffer::~MseTrackBuffer() {
  DVLOG(2) << __FUNCTION__ << "()";
}

void MseTrackBuffer::Reset() {
  DVLOG(2) << __FUNCTION__ << "()";

  last_decode_timestamp_ = kNoTimestamp();
  last_frame_duration_ = kNoTimestamp();
  highest_presentation_timestamp_ = kNoTimestamp();
  needs_random_access_point_ = true;
}

void MseTrackBuffer::SetHighestPresentationTimestampIfIncreased(
    base::TimeDelta timestamp) {
  if (highest_presentation_timestamp_ == kNoTimestamp() ||
      timestamp > highest_presentation_timestamp_) {
    highest_presentation_timestamp_ = timestamp;
  }
}

FrameProcessorBase::FrameProcessorBase()
    : sequence_mode_(false),
      group_start_timestamp_(kNoTimestamp()) {}

FrameProcessorBase::~FrameProcessorBase() {
  DVLOG(2) << __FUNCTION__ << "()";

  STLDeleteValues(&track_buffers_);
}

void FrameProcessorBase::SetGroupStartTimestampIfInSequenceMode(
    base::TimeDelta timestamp_offset) {
  DVLOG(2) << __FUNCTION__ << "(" << timestamp_offset.InSecondsF() << ")";
  DCHECK(kNoTimestamp() != timestamp_offset);
  if (sequence_mode_)
    group_start_timestamp_ = timestamp_offset;
}

bool FrameProcessorBase::AddTrack(StreamParser::TrackId id,
                                  ChunkDemuxerStream* stream) {
  DVLOG(2) << __FUNCTION__ << "(): id=" << id;

  MseTrackBuffer* existing_track = FindTrack(id);
  DCHECK(!existing_track);
  if (existing_track)
    return false;

  track_buffers_[id] = new MseTrackBuffer(stream);
  return true;
}

void FrameProcessorBase::Reset() {
  DVLOG(2) << __FUNCTION__ << "()";
  for (TrackBufferMap::iterator itr = track_buffers_.begin();
       itr != track_buffers_.end(); ++itr) {
    itr->second->Reset();
  }
}

MseTrackBuffer* FrameProcessorBase::FindTrack(StreamParser::TrackId id) {
  TrackBufferMap::iterator itr = track_buffers_.find(id);
  if (itr == track_buffers_.end())
    return NULL;

  return itr->second;
}

void FrameProcessorBase::NotifyNewMediaSegmentStarting(
    base::TimeDelta segment_timestamp) {
  DVLOG(2) << __FUNCTION__ << "(" << segment_timestamp.InSecondsF() << ")";

  for (TrackBufferMap::iterator itr = track_buffers_.begin();
       itr != track_buffers_.end();
       ++itr) {
    itr->second->stream()->OnNewMediaSegment(segment_timestamp);
  }
}

}  // namespace media
