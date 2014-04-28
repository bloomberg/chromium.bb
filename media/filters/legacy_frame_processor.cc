// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/legacy_frame_processor.h"

#include "media/base/buffers.h"
#include "media/base/stream_parser_buffer.h"

namespace media {

LegacyFrameProcessor::LegacyFrameProcessor(
    const IncreaseDurationCB& increase_duration_cb)
    : increase_duration_cb_(increase_duration_cb) {
  DVLOG(2) << __FUNCTION__ << "()";
  DCHECK(!increase_duration_cb_.is_null());
}

LegacyFrameProcessor::~LegacyFrameProcessor() {
  DVLOG(2) << __FUNCTION__ << "()";
}

void LegacyFrameProcessor::SetSequenceMode(bool sequence_mode) {
  DVLOG(2) << __FUNCTION__ << "(" << sequence_mode << ")";

  sequence_mode_ = sequence_mode;
}

bool LegacyFrameProcessor::ProcessFrames(
    const StreamParser::BufferQueue& audio_buffers,
    const StreamParser::BufferQueue& video_buffers,
    const StreamParser::TextBufferQueueMap& text_map,
    base::TimeDelta append_window_start,
    base::TimeDelta append_window_end,
    bool* new_media_segment,
    base::TimeDelta* timestamp_offset) {
  DVLOG(2) << __FUNCTION__ << "()";
  DCHECK(new_media_segment);
  DCHECK(timestamp_offset);

  // NOTE: Legacy coded frame processing does not update timestamp offset.
  base::TimeDelta offset = *timestamp_offset;

  DCHECK(!audio_buffers.empty() || !video_buffers.empty() ||
         !text_map.empty());

  MseTrackBuffer* audio_track = FindTrack(kAudioTrackId);
  DCHECK(audio_buffers.empty() || audio_track);

  MseTrackBuffer* video_track = FindTrack(kVideoTrackId);
  DCHECK(video_buffers.empty() || video_track);

  // TODO(wolenetz): DCHECK + return false if any of these buffers have UNKNOWN
  // type() in upcoming coded frame processing compliant implementation. See
  // http://crbug.com/249422.

  StreamParser::BufferQueue filtered_audio;
  StreamParser::BufferQueue filtered_video;
  StreamParser::TextBufferQueueMap filtered_text;

  if (audio_track) {
    AdjustBufferTimestamps(audio_buffers, offset);
    FilterWithAppendWindow(append_window_start, append_window_end,
                           audio_buffers, audio_track,
                           new_media_segment, &filtered_audio);
  }

  if (video_track) {
    AdjustBufferTimestamps(video_buffers, offset);
    FilterWithAppendWindow(append_window_start, append_window_end,
                           video_buffers, video_track,
                           new_media_segment, &filtered_video);
  }

  // The earliest timestamp in the filtered buffers will be used for the segment
  // start timestamp.
  base::TimeDelta segment_timestamp = kInfiniteDuration();

  // Process any buffers for each of the text tracks in the map.
  for (StreamParser::TextBufferQueueMap::const_iterator itr = text_map.begin();
       itr != text_map.end();
       ++itr) {
    const StreamParser::BufferQueue& text_buffers = itr->second;
    if (text_buffers.empty())
      continue;

    if (!FilterTextBuffers(itr->first,
                           append_window_start,
                           append_window_end,
                           offset,
                           text_buffers,
                           new_media_segment,
                           &segment_timestamp,
                           &filtered_text)) {
      return false;
    }
  }

  if ((!filtered_audio.empty() || !filtered_video.empty() ||
       !filtered_text.empty()) &&
      *new_media_segment) {
    if (!filtered_audio.empty()) {
      segment_timestamp = std::min(filtered_audio.front()->GetDecodeTimestamp(),
                                   segment_timestamp);
    }

    if (!filtered_video.empty()) {
      segment_timestamp = std::min(filtered_video.front()->GetDecodeTimestamp(),
                                   segment_timestamp);
    }

    *new_media_segment = false;

    DCHECK(segment_timestamp != kInfiniteDuration());
    for (TrackBufferMap::iterator itr = track_buffers_.begin();
         itr != track_buffers_.end();
         ++itr) {
      itr->second->stream()->OnNewMediaSegment(segment_timestamp);
    }
  }

  if (!filtered_audio.empty() &&
      !AppendAndUpdateDuration(audio_track->stream(), filtered_audio)) {
    return false;
  }

  if (!filtered_video.empty() &&
      !AppendAndUpdateDuration(video_track->stream(), filtered_video)) {
    return false;
  }

  if (!filtered_text.empty()) {
    for (StreamParser::TextBufferQueueMap::const_iterator itr =
             filtered_text.begin();
         itr != filtered_text.end();
         ++itr) {
      MseTrackBuffer* track = FindTrack(itr->first);
      if (!track || !AppendAndUpdateDuration(track->stream(), itr->second))
        return false;
    }
  }

  return true;
}

void LegacyFrameProcessor::AdjustBufferTimestamps(
    const StreamParser::BufferQueue& buffers,
    base::TimeDelta timestamp_offset) {
  if (timestamp_offset == base::TimeDelta())
    return;

  for (StreamParser::BufferQueue::const_iterator itr = buffers.begin();
       itr != buffers.end(); ++itr) {
    (*itr)->SetDecodeTimestamp(
        (*itr)->GetDecodeTimestamp() + timestamp_offset);
    (*itr)->set_timestamp((*itr)->timestamp() + timestamp_offset);
  }
}

void LegacyFrameProcessor::FilterWithAppendWindow(
    base::TimeDelta append_window_start,
    base::TimeDelta append_window_end,
    const StreamParser::BufferQueue& buffers,
    MseTrackBuffer* track,
    bool* new_media_segment,
    StreamParser::BufferQueue* filtered_buffers) {
  DCHECK(track);
  DCHECK(new_media_segment);
  DCHECK(filtered_buffers);

  // This loop implements steps 1.9, 1.10, & 1.11 of the "Coded frame
  // processing loop" in the Media Source Extensions spec.
  // These steps filter out buffers that are not within the "append
  // window" and handles resyncing on the next random access point
  // (i.e., next keyframe) if a buffer gets dropped.
  for (StreamParser::BufferQueue::const_iterator itr = buffers.begin();
       itr != buffers.end(); ++itr) {
    // Filter out buffers that are outside the append window. Anytime
    // a buffer gets dropped we need to set |*needs_keyframe| to true
    // because we can only resume decoding at keyframes.
    base::TimeDelta presentation_timestamp = (*itr)->timestamp();

    // TODO(acolwell): Change |frame_end_timestamp| value to always be
    // |presentation_timestamp + (*itr)->duration()|, like the spec
    // requires, once frame durations are actually present in all buffers.
    // See http://crbug.com/351166.
    base::TimeDelta frame_end_timestamp = presentation_timestamp;
    base::TimeDelta frame_duration = (*itr)->duration();
    if (frame_duration > base::TimeDelta()) {
      DCHECK(frame_duration != kNoTimestamp());
      frame_end_timestamp += frame_duration;
    }

    if (presentation_timestamp < append_window_start ||
        frame_end_timestamp > append_window_end) {
      DVLOG(1) << "Dropping buffer outside append window."
               << " presentation_timestamp "
               << presentation_timestamp.InSecondsF();
      track->set_needs_random_access_point(true);

      // This triggers a discontinuity so we need to treat the next frames
      // appended within the append window as if they were the beginning of a
      // new segment.
      *new_media_segment = true;
      continue;
    }

    // If the track needs a keyframe, then filter out buffers until we
    // encounter the next keyframe.
    if (track->needs_random_access_point()) {
      if (!(*itr)->IsKeyframe()) {
        DVLOG(1) << "Dropping non-keyframe. presentation_timestamp "
                 << presentation_timestamp.InSecondsF();
        continue;
      }

      track->set_needs_random_access_point(false);
    }

    filtered_buffers->push_back(*itr);
  }
}

bool LegacyFrameProcessor::AppendAndUpdateDuration(
    ChunkDemuxerStream* stream,
    const StreamParser::BufferQueue& buffers) {
  DCHECK(!buffers.empty());

  if (!stream || !stream->Append(buffers))
    return false;

  // Approximate spec's "highest presentation end timestamp".
  base::TimeDelta highest_end_timestamp = stream->GetBufferedDuration();
  DCHECK(highest_end_timestamp > base::TimeDelta());

  increase_duration_cb_.Run(highest_end_timestamp);
  return true;
}

bool LegacyFrameProcessor::FilterTextBuffers(
    StreamParser::TrackId text_track_id,
    base::TimeDelta append_window_start,
    base::TimeDelta append_window_end,
    base::TimeDelta timestamp_offset,
    const StreamParser::BufferQueue& buffers,
    bool* new_media_segment,
    base::TimeDelta* lowest_segment_timestamp,
    StreamParser::TextBufferQueueMap* filtered_text) {
  DCHECK(!buffers.empty());
  DCHECK(text_track_id != kAudioTrackId && text_track_id != kVideoTrackId);
  DCHECK(new_media_segment);

  MseTrackBuffer* track = FindTrack(text_track_id);
  if (!track)
    return false;

  AdjustBufferTimestamps(buffers, timestamp_offset);

  StreamParser::BufferQueue filtered_buffers;
  FilterWithAppendWindow(append_window_start,
                         append_window_end,
                         buffers,
                         track,
                         new_media_segment,
                         &filtered_buffers);

  if (!filtered_buffers.empty()) {
    *lowest_segment_timestamp =
        std::min(*lowest_segment_timestamp,
                 filtered_buffers.front()->GetDecodeTimestamp());
    DCHECK(filtered_text->find(text_track_id) == filtered_text->end());
    filtered_text->insert(std::make_pair(text_track_id, filtered_buffers));
  }

  return true;
}

}  // namespace media
