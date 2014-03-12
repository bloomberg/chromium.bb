// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/frame_processor_base.h"

#include "base/stl_util.h"
#include "media/base/buffers.h"

namespace media {

MseTrackBuffer::MseTrackBuffer(ChunkDemuxerStream* stream)
    : needs_random_access_point_(true),
      stream_(stream) {
  DCHECK(stream_);
}

MseTrackBuffer::~MseTrackBuffer() {
  DVLOG(2) << __FUNCTION__ << "()";
}

void MseTrackBuffer::Reset() {
  DVLOG(2) << __FUNCTION__ << "()";

  needs_random_access_point_ = true;
}

FrameProcessorBase::FrameProcessorBase(
    const IncreaseDurationCB& increase_duration_cb)
    : sequence_mode_(false),
      increase_duration_cb_(increase_duration_cb) {
  DCHECK(!increase_duration_cb_.is_null());
}

FrameProcessorBase::~FrameProcessorBase() {
  DVLOG(2) << __FUNCTION__ << "()";

  STLDeleteValues(&track_buffers_);
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

}  // namespace media
