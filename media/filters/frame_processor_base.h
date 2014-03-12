// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_FRAME_PROCESSOR_BASE_H_
#define MEDIA_FILTERS_FRAME_PROCESSOR_BASE_H_

#include <map>

#include "base/basictypes.h"
#include "base/time/time.h"
#include "media/base/media_export.h"
#include "media/base/stream_parser.h"
#include "media/filters/chunk_demuxer.h"

namespace media {

// Helper class to capture per-track details needed by a frame processor. Some
// of this information may be duplicated in the short-term in the associated
// ChunkDemuxerStream and SourceBufferStream for a track.
// This parallels the MSE spec each of a SourceBuffer's Track Buffers at
// http://www.w3.org/TR/media-source/#track-buffers.
class MseTrackBuffer {
 public:
  explicit MseTrackBuffer(ChunkDemuxerStream* stream);
  ~MseTrackBuffer();

  // Get/set |needs_random_access_point_|.
  bool needs_random_access_point() const {
    return needs_random_access_point_;
  }
  void set_needs_random_access_point(bool needs_random_access_point) {
    needs_random_access_point_ = needs_random_access_point;
  }

  // Gets a pointer to this track's ChunkDemuxerStream.
  ChunkDemuxerStream* stream() const { return stream_; }

  // Sets |needs_random_access_point_| to true.
  // TODO(wolenetz): Add the rest of the new coded frame processing algorithm
  // track buffer attributes and reset them here. See http://crbug.com/249422.
  void Reset();

 private:
  // Keeps track of whether the track buffer is waiting for a random access
  // point coded frame. Initially set to true to indicate that a random access
  // point coded frame is needed before anything can be added to the track
  // buffer.
  bool needs_random_access_point_;

  // Pointer to the stream associated with this track. The stream is not owned
  // by |this|.
  ChunkDemuxerStream* const stream_;

  DISALLOW_COPY_AND_ASSIGN(MseTrackBuffer);
};

// Abstract interface for helper class implementation of Media Source
// Extension's coded frame processing algorithm.
// TODO(wolenetz): Once the new FrameProcessor implementation stabilizes, remove
// LegacyFrameProcessor and fold this interface into FrameProcessor. See
// http://crbug.com/249422.
class MEDIA_EXPORT FrameProcessorBase {
 public:
  // TODO(wolenetz/acolwell): Ensure that all TrackIds are coherent and unique
  // for each track buffer. For now, special track identifiers are used for each
  // of audio and video here, and text TrackIds are assumed to be non-negative.
  // See http://crbug.com/341581.
  enum {
    kAudioTrackId = -2,
    kVideoTrackId = -3
  };

  // Callback signature used to notify ChunkDemuxer of timestamps that may cause
  // the duration to be updated.
  typedef base::Callback<void(
      base::TimeDelta, ChunkDemuxerStream*)> IncreaseDurationCB;

  virtual ~FrameProcessorBase();

  // Get/set the current append mode, which if true means "sequence" and if
  // false means "segments".
  // See http://www.w3.org/TR/media-source/#widl-SourceBuffer-mode.
  bool sequence_mode() { return sequence_mode_; }
  virtual void SetSequenceMode(bool sequence_mode) = 0;

  // Processes buffers in |audio_buffers|, |video_buffers|, and |text_map|.
  // Returns true on success or false on failure which indicates decode error.
  // |append_window_start| and |append_window_end| correspond to the MSE spec's
  // similarly named source buffer attributes that are used in coded frame
  // processing.
  // |*new_media_segment| tracks whether the next buffers processed within the
  // append window represent the start of a new media segment. This method may
  // both use and update this flag.
  // Uses |*timestamp_offset| according to the coded frame processing algorithm,
  // including updating it as required in 'sequence' mode frame processing.
  virtual bool ProcessFrames(const StreamParser::BufferQueue& audio_buffers,
                             const StreamParser::BufferQueue& video_buffers,
                             const StreamParser::TextBufferQueueMap& text_map,
                             base::TimeDelta append_window_start,
                             base::TimeDelta append_window_end,
                             bool* new_media_segment,
                             base::TimeDelta* timestamp_offset) = 0;

  // Adds a new track with unique track ID |id|.
  // If |id| has previously been added, returns false to indicate error.
  // Otherwise, returns true, indicating future ProcessFrames() will emit
  // frames for the track |id| to |stream|.
  bool AddTrack(StreamParser::TrackId id, ChunkDemuxerStream* stream);

  // Resets state for the coded frame processing algorithm as described in steps
  // 2-5 of the MSE Reset Parser State algorithm described at
  // http://www.w3.org/TR/media-source/#sourcebuffer-reset-parser-state
  void Reset();

 protected:
  typedef std::map<StreamParser::TrackId, MseTrackBuffer*> TrackBufferMap;

  explicit FrameProcessorBase(const IncreaseDurationCB& increase_duration_cb);

  // If |track_buffers_| contains |id|, returns a pointer to the associated
  // MseTrackBuffer. Otherwise, returns NULL.
  MseTrackBuffer* FindTrack(StreamParser::TrackId id);

  // The AppendMode of the associated SourceBuffer.
  // See SetSequenceMode() for interpretation of |sequence_mode_|.
  // Per http://www.w3.org/TR/media-source/#widl-SourceBuffer-mode:
  // Controls how a sequence of media segments are handled. This is initially
  // set to false ("segments").
  bool sequence_mode_;

  IncreaseDurationCB increase_duration_cb_;

  // TrackId-indexed map of each track's stream.
  TrackBufferMap track_buffers_;
};

}  // namespace media

#endif  // MEDIA_FILTERS_FRAME_PROCESSOR_BASE_H_
