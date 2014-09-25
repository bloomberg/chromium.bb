// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/source_buffer_stream.h"

#include <algorithm>
#include <map>

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "media/base/audio_splicer.h"
#include "media/filters/source_buffer_platform.h"
#include "media/filters/source_buffer_range.h"

namespace media {

// Helper method that returns true if |ranges| is sorted in increasing order,
// false otherwise.
static bool IsRangeListSorted(
    const std::list<media::SourceBufferRange*>& ranges) {
  DecodeTimestamp prev = kNoDecodeTimestamp();
  for (std::list<SourceBufferRange*>::const_iterator itr =
       ranges.begin(); itr != ranges.end(); ++itr) {
    if (prev != kNoDecodeTimestamp() && prev >= (*itr)->GetStartTimestamp())
      return false;
    prev = (*itr)->GetEndTimestamp();
  }
  return true;
}

// Returns an estimate of how far from the beginning or end of a range a buffer
// can be to still be considered in the range, given the |approximate_duration|
// of a buffer in the stream.
// TODO(wolenetz): Once all stream parsers emit accurate frame durations, use
// logic like FrameProcessor (2*last_frame_duration + last_decode_timestamp)
// instead of an overall maximum interbuffer delta for range discontinuity
// detection, and adjust similarly for splice frame discontinuity detection.
// See http://crbug.com/351489 and http://crbug.com/351166.
static base::TimeDelta ComputeFudgeRoom(base::TimeDelta approximate_duration) {
  // Because we do not know exactly when is the next timestamp, any buffer
  // that starts within 2x the approximate duration of a buffer is considered
  // within this range.
  return 2 * approximate_duration;
}

// An arbitrarily-chosen number to estimate the duration of a buffer if none
// is set and there's not enough information to get a better estimate.
static int kDefaultBufferDurationInMs = 125;

// The amount of time the beginning of the buffered data can differ from the
// start time in order to still be considered the start of stream.
static base::TimeDelta kSeekToStartFudgeRoom() {
  return base::TimeDelta::FromMilliseconds(1000);
}

static SourceBufferRange::GapPolicy TypeToGapPolicy(
    SourceBufferStream::Type type) {
  switch (type) {
    case SourceBufferStream::kAudio:
    case SourceBufferStream::kVideo:
      return SourceBufferRange::NO_GAPS_ALLOWED;
    case SourceBufferStream::kText:
      return SourceBufferRange::ALLOW_GAPS;
  }

  NOTREACHED();
  return SourceBufferRange::NO_GAPS_ALLOWED;
}

SourceBufferStream::SourceBufferStream(const AudioDecoderConfig& audio_config,
                                       const LogCB& log_cb,
                                       bool splice_frames_enabled)
    : log_cb_(log_cb),
      current_config_index_(0),
      append_config_index_(0),
      seek_pending_(false),
      end_of_stream_(false),
      seek_buffer_timestamp_(kNoTimestamp()),
      selected_range_(NULL),
      media_segment_start_time_(kNoDecodeTimestamp()),
      range_for_next_append_(ranges_.end()),
      new_media_segment_(false),
      last_appended_buffer_timestamp_(kNoDecodeTimestamp()),
      last_appended_buffer_is_keyframe_(false),
      last_output_buffer_timestamp_(kNoDecodeTimestamp()),
      max_interbuffer_distance_(kNoTimestamp()),
      memory_limit_(kSourceBufferAudioMemoryLimit),
      config_change_pending_(false),
      splice_buffers_index_(0),
      pending_buffers_complete_(false),
      splice_frames_enabled_(splice_frames_enabled) {
  DCHECK(audio_config.IsValidConfig());
  audio_configs_.push_back(audio_config);
}

SourceBufferStream::SourceBufferStream(const VideoDecoderConfig& video_config,
                                       const LogCB& log_cb,
                                       bool splice_frames_enabled)
    : log_cb_(log_cb),
      current_config_index_(0),
      append_config_index_(0),
      seek_pending_(false),
      end_of_stream_(false),
      seek_buffer_timestamp_(kNoTimestamp()),
      selected_range_(NULL),
      media_segment_start_time_(kNoDecodeTimestamp()),
      range_for_next_append_(ranges_.end()),
      new_media_segment_(false),
      last_appended_buffer_timestamp_(kNoDecodeTimestamp()),
      last_appended_buffer_is_keyframe_(false),
      last_output_buffer_timestamp_(kNoDecodeTimestamp()),
      max_interbuffer_distance_(kNoTimestamp()),
      memory_limit_(kSourceBufferVideoMemoryLimit),
      config_change_pending_(false),
      splice_buffers_index_(0),
      pending_buffers_complete_(false),
      splice_frames_enabled_(splice_frames_enabled) {
  DCHECK(video_config.IsValidConfig());
  video_configs_.push_back(video_config);
}

SourceBufferStream::SourceBufferStream(const TextTrackConfig& text_config,
                                       const LogCB& log_cb,
                                       bool splice_frames_enabled)
    : log_cb_(log_cb),
      current_config_index_(0),
      append_config_index_(0),
      text_track_config_(text_config),
      seek_pending_(false),
      end_of_stream_(false),
      seek_buffer_timestamp_(kNoTimestamp()),
      selected_range_(NULL),
      media_segment_start_time_(kNoDecodeTimestamp()),
      range_for_next_append_(ranges_.end()),
      new_media_segment_(false),
      last_appended_buffer_timestamp_(kNoDecodeTimestamp()),
      last_appended_buffer_is_keyframe_(false),
      last_output_buffer_timestamp_(kNoDecodeTimestamp()),
      max_interbuffer_distance_(kNoTimestamp()),
      memory_limit_(kSourceBufferAudioMemoryLimit),
      config_change_pending_(false),
      splice_buffers_index_(0),
      pending_buffers_complete_(false),
      splice_frames_enabled_(splice_frames_enabled) {}

SourceBufferStream::~SourceBufferStream() {
  while (!ranges_.empty()) {
    delete ranges_.front();
    ranges_.pop_front();
  }
}

void SourceBufferStream::OnNewMediaSegment(
    DecodeTimestamp media_segment_start_time) {
  DVLOG(1) << __FUNCTION__ << "(" << media_segment_start_time.InSecondsF()
           << ")";
  DCHECK(!end_of_stream_);
  media_segment_start_time_ = media_segment_start_time;
  new_media_segment_ = true;

  RangeList::iterator last_range = range_for_next_append_;
  range_for_next_append_ = FindExistingRangeFor(media_segment_start_time);

  // Only reset |last_appended_buffer_timestamp_| if this new media segment is
  // not adjacent to the previous media segment appended to the stream.
  if (range_for_next_append_ == ranges_.end() ||
      !AreAdjacentInSequence(last_appended_buffer_timestamp_,
                             media_segment_start_time)) {
    last_appended_buffer_timestamp_ = kNoDecodeTimestamp();
    last_appended_buffer_is_keyframe_ = false;
    DVLOG(3) << __FUNCTION__ << " next appended buffers will be in a new range";
  } else if (last_range != ranges_.end()) {
    DCHECK(last_range == range_for_next_append_);
    DVLOG(3) << __FUNCTION__ << " next appended buffers will continue range "
             << "unless intervening remove makes discontinuity";
  }
}

bool SourceBufferStream::Append(const BufferQueue& buffers) {
  TRACE_EVENT2("media", "SourceBufferStream::Append",
               "stream type", GetStreamTypeName(),
               "buffers to append", buffers.size());

  DCHECK(!buffers.empty());
  DCHECK(media_segment_start_time_ != kNoDecodeTimestamp());
  DCHECK(media_segment_start_time_ <= buffers.front()->GetDecodeTimestamp());
  DCHECK(!end_of_stream_);

  // New media segments must begin with a keyframe.
  if (new_media_segment_ && !buffers.front()->IsKeyframe()) {
    MEDIA_LOG(log_cb_) << "Media segment did not begin with keyframe.";
    return false;
  }

  // Buffers within a media segment should be monotonically increasing.
  if (!IsMonotonicallyIncreasing(buffers))
    return false;

  if (media_segment_start_time_ < DecodeTimestamp() ||
      buffers.front()->GetDecodeTimestamp() < DecodeTimestamp()) {
    MEDIA_LOG(log_cb_)
        << "Cannot append a media segment with negative timestamps.";
    return false;
  }

  if (!IsNextTimestampValid(buffers.front()->GetDecodeTimestamp(),
                            buffers.front()->IsKeyframe())) {
    MEDIA_LOG(log_cb_) << "Invalid same timestamp construct detected at time "
                       << buffers.front()->GetDecodeTimestamp().InSecondsF();

    return false;
  }

  UpdateMaxInterbufferDistance(buffers);
  SetConfigIds(buffers);

  // Save a snapshot of stream state before range modifications are made.
  DecodeTimestamp next_buffer_timestamp = GetNextBufferTimestamp();
  BufferQueue deleted_buffers;

  PrepareRangesForNextAppend(buffers, &deleted_buffers);

  // If there's a range for |buffers|, insert |buffers| accordingly. Otherwise,
  // create a new range with |buffers|.
  if (range_for_next_append_ != ranges_.end()) {
    (*range_for_next_append_)->AppendBuffersToEnd(buffers);
    last_appended_buffer_timestamp_ = buffers.back()->GetDecodeTimestamp();
    last_appended_buffer_is_keyframe_ = buffers.back()->IsKeyframe();
  } else {
    DecodeTimestamp new_range_start_time = std::min(
        media_segment_start_time_, buffers.front()->GetDecodeTimestamp());
    const BufferQueue* buffers_for_new_range = &buffers;
    BufferQueue trimmed_buffers;

    // If the new range is not being created because of a new media
    // segment, then we must make sure that we start with a keyframe.
    // This can happen if the GOP in the previous append gets destroyed
    // by a Remove() call.
    if (!new_media_segment_) {
      BufferQueue::const_iterator itr = buffers.begin();

      // Scan past all the non-keyframes.
      while (itr != buffers.end() && !(*itr)->IsKeyframe()) {
        ++itr;
      }

      // If we didn't find a keyframe, then update the last appended
      // buffer state and return.
      if (itr == buffers.end()) {
        last_appended_buffer_timestamp_ = buffers.back()->GetDecodeTimestamp();
        last_appended_buffer_is_keyframe_ = buffers.back()->IsKeyframe();
        return true;
      } else if (itr != buffers.begin()) {
        // Copy the first keyframe and everything after it into
        // |trimmed_buffers|.
        trimmed_buffers.assign(itr, buffers.end());
        buffers_for_new_range = &trimmed_buffers;
      }

      new_range_start_time =
          buffers_for_new_range->front()->GetDecodeTimestamp();
    }

    range_for_next_append_ =
        AddToRanges(new SourceBufferRange(
            TypeToGapPolicy(GetType()),
            *buffers_for_new_range, new_range_start_time,
            base::Bind(&SourceBufferStream::GetMaxInterbufferDistance,
                       base::Unretained(this))));
    last_appended_buffer_timestamp_ =
        buffers_for_new_range->back()->GetDecodeTimestamp();
    last_appended_buffer_is_keyframe_ =
        buffers_for_new_range->back()->IsKeyframe();
  }

  new_media_segment_ = false;

  MergeWithAdjacentRangeIfNecessary(range_for_next_append_);

  // Seek to try to fulfill a previous call to Seek().
  if (seek_pending_) {
    DCHECK(!selected_range_);
    DCHECK(deleted_buffers.empty());
    Seek(seek_buffer_timestamp_);
  }

  if (!deleted_buffers.empty()) {
    DecodeTimestamp start_of_deleted =
        deleted_buffers.front()->GetDecodeTimestamp();

    DCHECK(track_buffer_.empty() ||
           track_buffer_.back()->GetDecodeTimestamp() < start_of_deleted)
        << "decode timestamp "
        << track_buffer_.back()->GetDecodeTimestamp().InSecondsF() << " sec"
        << ", start_of_deleted " << start_of_deleted.InSecondsF()<< " sec";

    track_buffer_.insert(track_buffer_.end(), deleted_buffers.begin(),
                         deleted_buffers.end());
  }

  // Prune any extra buffers in |track_buffer_| if new keyframes
  // are appended to the range covered by |track_buffer_|.
  if (!track_buffer_.empty()) {
    DecodeTimestamp keyframe_timestamp =
        FindKeyframeAfterTimestamp(track_buffer_.front()->GetDecodeTimestamp());
    if (keyframe_timestamp != kNoDecodeTimestamp())
      PruneTrackBuffer(keyframe_timestamp);
  }

  SetSelectedRangeIfNeeded(next_buffer_timestamp);

  GarbageCollectIfNeeded();

  DCHECK(IsRangeListSorted(ranges_));
  DCHECK(OnlySelectedRangeIsSeeked());
  return true;
}

void SourceBufferStream::Remove(base::TimeDelta start, base::TimeDelta end,
                                base::TimeDelta duration) {
  DVLOG(1) << __FUNCTION__ << "(" << start.InSecondsF()
           << ", " << end.InSecondsF()
           << ", " << duration.InSecondsF() << ")";
  DCHECK(start >= base::TimeDelta()) << start.InSecondsF();
  DCHECK(start < end) << "start " << start.InSecondsF()
                      << " end " << end.InSecondsF();
  DCHECK(duration != kNoTimestamp());

  DecodeTimestamp start_dts = DecodeTimestamp::FromPresentationTime(start);
  DecodeTimestamp end_dts = DecodeTimestamp::FromPresentationTime(end);
  DecodeTimestamp remove_end_timestamp =
      DecodeTimestamp::FromPresentationTime(duration);
  DecodeTimestamp keyframe_timestamp = FindKeyframeAfterTimestamp(end_dts);
  if (keyframe_timestamp != kNoDecodeTimestamp()) {
    remove_end_timestamp = keyframe_timestamp;
  } else if (end_dts < remove_end_timestamp) {
    remove_end_timestamp = end_dts;
  }

  BufferQueue deleted_buffers;
  RemoveInternal(start_dts, remove_end_timestamp, false, &deleted_buffers);

  if (!deleted_buffers.empty())
    SetSelectedRangeIfNeeded(deleted_buffers.front()->GetDecodeTimestamp());
}

void SourceBufferStream::RemoveInternal(
    DecodeTimestamp start, DecodeTimestamp end, bool is_exclusive,
    BufferQueue* deleted_buffers) {
  DVLOG(1) << __FUNCTION__ << "(" << start.InSecondsF()
           << ", " << end.InSecondsF()
           << ", " << is_exclusive << ")";

  DCHECK(start >= DecodeTimestamp());
  DCHECK(start < end) << "start " << start.InSecondsF()
                      << " end " << end.InSecondsF();
  DCHECK(deleted_buffers);

  RangeList::iterator itr = ranges_.begin();

  while (itr != ranges_.end()) {
    SourceBufferRange* range = *itr;
    if (range->GetStartTimestamp() >= end)
      break;

    // Split off any remaining end piece and add it to |ranges_|.
    SourceBufferRange* new_range = range->SplitRange(end, is_exclusive);
    if (new_range) {
      itr = ranges_.insert(++itr, new_range);
      --itr;

      // Update the selected range if the next buffer position was transferred
      // to |new_range|.
      if (new_range->HasNextBufferPosition())
        SetSelectedRange(new_range);
    }

    // Truncate the current range so that it only contains data before
    // the removal range.
    BufferQueue saved_buffers;
    bool delete_range = range->TruncateAt(start, &saved_buffers, is_exclusive);

    // Check to see if the current playback position was removed and
    // update the selected range appropriately.
    if (!saved_buffers.empty()) {
      DCHECK(!range->HasNextBufferPosition());
      DCHECK(deleted_buffers->empty());

      *deleted_buffers = saved_buffers;
    }

    if (range == selected_range_ && !range->HasNextBufferPosition())
      SetSelectedRange(NULL);

    // If the current range now is completely covered by the removal
    // range then delete it and move on.
    if (delete_range) {
      DeleteAndRemoveRange(&itr);
      continue;
    }

    // Clear |range_for_next_append_| if we determine that the removal
    // operation makes it impossible for the next append to be added
    // to the current range.
    if (range_for_next_append_ != ranges_.end() &&
        *range_for_next_append_ == range &&
        last_appended_buffer_timestamp_ != kNoDecodeTimestamp()) {
      DecodeTimestamp potential_next_append_timestamp =
          last_appended_buffer_timestamp_ +
          base::TimeDelta::FromInternalValue(1);

      if (!range->BelongsToRange(potential_next_append_timestamp)) {
        DVLOG(1) << "Resetting range_for_next_append_ since the next append"
                 <<  " can't add to the current range.";
        range_for_next_append_ =
            FindExistingRangeFor(potential_next_append_timestamp);
      }
    }

    // Move on to the next range.
    ++itr;
  }

  DCHECK(IsRangeListSorted(ranges_));
  DCHECK(OnlySelectedRangeIsSeeked());
  DVLOG(1) << __FUNCTION__ << " : done";
}

void SourceBufferStream::ResetSeekState() {
  SetSelectedRange(NULL);
  track_buffer_.clear();
  config_change_pending_ = false;
  last_output_buffer_timestamp_ = kNoDecodeTimestamp();
  splice_buffers_index_ = 0;
  pending_buffer_ = NULL;
  pending_buffers_complete_ = false;
}

bool SourceBufferStream::ShouldSeekToStartOfBuffered(
    base::TimeDelta seek_timestamp) const {
  if (ranges_.empty())
    return false;
  base::TimeDelta beginning_of_buffered =
      ranges_.front()->GetStartTimestamp().ToPresentationTime();
  return (seek_timestamp <= beginning_of_buffered &&
          beginning_of_buffered < kSeekToStartFudgeRoom());
}

bool SourceBufferStream::IsMonotonicallyIncreasing(
    const BufferQueue& buffers) const {
  DCHECK(!buffers.empty());
  DecodeTimestamp prev_timestamp = last_appended_buffer_timestamp_;
  bool prev_is_keyframe = last_appended_buffer_is_keyframe_;
  for (BufferQueue::const_iterator itr = buffers.begin();
       itr != buffers.end(); ++itr) {
    DecodeTimestamp current_timestamp = (*itr)->GetDecodeTimestamp();
    bool current_is_keyframe = (*itr)->IsKeyframe();
    DCHECK(current_timestamp != kNoDecodeTimestamp());
    DCHECK((*itr)->duration() >= base::TimeDelta())
        << "Packet with invalid duration."
        << " pts " << (*itr)->timestamp().InSecondsF()
        << " dts " << (*itr)->GetDecodeTimestamp().InSecondsF()
        << " dur " << (*itr)->duration().InSecondsF();

    if (prev_timestamp != kNoDecodeTimestamp()) {
      if (current_timestamp < prev_timestamp) {
        MEDIA_LOG(log_cb_) << "Buffers were not monotonically increasing.";
        return false;
      }

      if (current_timestamp == prev_timestamp &&
          !SourceBufferRange::AllowSameTimestamp(prev_is_keyframe,
                                                 current_is_keyframe)) {
        MEDIA_LOG(log_cb_) << "Unexpected combination of buffers with the"
                           << " same timestamp detected at "
                           << current_timestamp.InSecondsF();
        return false;
      }
    }

    prev_timestamp = current_timestamp;
    prev_is_keyframe = current_is_keyframe;
  }
  return true;
}

bool SourceBufferStream::IsNextTimestampValid(
    DecodeTimestamp next_timestamp, bool next_is_keyframe) const {
  return (last_appended_buffer_timestamp_ != next_timestamp) ||
      new_media_segment_ ||
      SourceBufferRange::AllowSameTimestamp(last_appended_buffer_is_keyframe_,
                                            next_is_keyframe);
}


bool SourceBufferStream::OnlySelectedRangeIsSeeked() const {
  for (RangeList::const_iterator itr = ranges_.begin();
       itr != ranges_.end(); ++itr) {
    if ((*itr)->HasNextBufferPosition() && (*itr) != selected_range_)
      return false;
  }
  return !selected_range_ || selected_range_->HasNextBufferPosition();
}

void SourceBufferStream::UpdateMaxInterbufferDistance(
    const BufferQueue& buffers) {
  DCHECK(!buffers.empty());
  DecodeTimestamp prev_timestamp = last_appended_buffer_timestamp_;
  for (BufferQueue::const_iterator itr = buffers.begin();
       itr != buffers.end(); ++itr) {
    DecodeTimestamp current_timestamp = (*itr)->GetDecodeTimestamp();
    DCHECK(current_timestamp != kNoDecodeTimestamp());

    base::TimeDelta interbuffer_distance = (*itr)->duration();
    DCHECK(interbuffer_distance >= base::TimeDelta());

    if (prev_timestamp != kNoDecodeTimestamp()) {
      interbuffer_distance =
          std::max(current_timestamp - prev_timestamp, interbuffer_distance);
    }

    if (interbuffer_distance > base::TimeDelta()) {
      if (max_interbuffer_distance_ == kNoTimestamp()) {
        max_interbuffer_distance_ = interbuffer_distance;
      } else {
        max_interbuffer_distance_ =
            std::max(max_interbuffer_distance_, interbuffer_distance);
      }
    }
    prev_timestamp = current_timestamp;
  }
}

void SourceBufferStream::SetConfigIds(const BufferQueue& buffers) {
  for (BufferQueue::const_iterator itr = buffers.begin();
       itr != buffers.end(); ++itr) {
    (*itr)->SetConfigId(append_config_index_);
  }
}

void SourceBufferStream::GarbageCollectIfNeeded() {
  // Compute size of |ranges_|.
  int ranges_size = 0;
  for (RangeList::iterator itr = ranges_.begin(); itr != ranges_.end(); ++itr)
    ranges_size += (*itr)->size_in_bytes();

  // Return if we're under or at the memory limit.
  if (ranges_size <= memory_limit_)
    return;

  int bytes_to_free = ranges_size - memory_limit_;

  // Begin deleting after the last appended buffer.
  int bytes_freed = FreeBuffersAfterLastAppended(bytes_to_free);

  // Begin deleting from the front.
  if (bytes_to_free - bytes_freed > 0)
    bytes_freed += FreeBuffers(bytes_to_free - bytes_freed, false);

  // Begin deleting from the back.
  if (bytes_to_free - bytes_freed > 0)
    FreeBuffers(bytes_to_free - bytes_freed, true);
}

int SourceBufferStream::FreeBuffersAfterLastAppended(int total_bytes_to_free) {
  DecodeTimestamp next_buffer_timestamp = GetNextBufferTimestamp();
  if (last_appended_buffer_timestamp_ == kNoDecodeTimestamp() ||
      next_buffer_timestamp == kNoDecodeTimestamp() ||
      last_appended_buffer_timestamp_ >= next_buffer_timestamp) {
    return 0;
  }

  DecodeTimestamp remove_range_start = last_appended_buffer_timestamp_;
  if (last_appended_buffer_is_keyframe_)
    remove_range_start += GetMaxInterbufferDistance();

  DecodeTimestamp remove_range_start_keyframe = FindKeyframeAfterTimestamp(
      remove_range_start);
  if (remove_range_start_keyframe != kNoDecodeTimestamp())
    remove_range_start = remove_range_start_keyframe;
  if (remove_range_start >= next_buffer_timestamp)
    return 0;

  DecodeTimestamp remove_range_end;
  int bytes_freed = GetRemovalRange(
      remove_range_start, next_buffer_timestamp, total_bytes_to_free,
      &remove_range_end);
  if (bytes_freed > 0) {
    Remove(remove_range_start.ToPresentationTime(),
           remove_range_end.ToPresentationTime(),
           next_buffer_timestamp.ToPresentationTime());
  }

  return bytes_freed;
}

int SourceBufferStream::GetRemovalRange(
    DecodeTimestamp start_timestamp, DecodeTimestamp end_timestamp,
    int total_bytes_to_free, DecodeTimestamp* removal_end_timestamp) {
  DCHECK(start_timestamp >= DecodeTimestamp()) << start_timestamp.InSecondsF();
  DCHECK(start_timestamp < end_timestamp)
      << "start " << start_timestamp.InSecondsF()
      << ", end " << end_timestamp.InSecondsF();

  int bytes_to_free = total_bytes_to_free;
  int bytes_freed = 0;

  for (RangeList::iterator itr = ranges_.begin();
       itr != ranges_.end() && bytes_to_free > 0; ++itr) {
    SourceBufferRange* range = *itr;
    if (range->GetStartTimestamp() >= end_timestamp)
      break;
    if (range->GetEndTimestamp() < start_timestamp)
      continue;

    int bytes_removed = range->GetRemovalGOP(
        start_timestamp, end_timestamp, bytes_to_free, removal_end_timestamp);
    bytes_to_free -= bytes_removed;
    bytes_freed += bytes_removed;
  }
  return bytes_freed;
}

int SourceBufferStream::FreeBuffers(int total_bytes_to_free,
                                    bool reverse_direction) {
  TRACE_EVENT2("media", "SourceBufferStream::FreeBuffers",
               "total bytes to free", total_bytes_to_free,
               "reverse direction", reverse_direction);

  DCHECK_GT(total_bytes_to_free, 0);
  int bytes_to_free = total_bytes_to_free;
  int bytes_freed = 0;

  // This range will save the last GOP appended to |range_for_next_append_|
  // if the buffers surrounding it get deleted during garbage collection.
  SourceBufferRange* new_range_for_append = NULL;

  while (!ranges_.empty() && bytes_to_free > 0) {
    SourceBufferRange* current_range = NULL;
    BufferQueue buffers;
    int bytes_deleted = 0;

    if (reverse_direction) {
      current_range = ranges_.back();
      if (current_range->LastGOPContainsNextBufferPosition()) {
        DCHECK_EQ(current_range, selected_range_);
        break;
      }
      bytes_deleted = current_range->DeleteGOPFromBack(&buffers);
    } else {
      current_range = ranges_.front();
      if (current_range->FirstGOPContainsNextBufferPosition()) {
        DCHECK_EQ(current_range, selected_range_);
        break;
      }
      bytes_deleted = current_range->DeleteGOPFromFront(&buffers);
    }

    // Check to see if we've just deleted the GOP that was last appended.
    DecodeTimestamp end_timestamp = buffers.back()->GetDecodeTimestamp();
    if (end_timestamp == last_appended_buffer_timestamp_) {
      DCHECK(last_appended_buffer_timestamp_ != kNoDecodeTimestamp());
      DCHECK(!new_range_for_append);
      // Create a new range containing these buffers.
      new_range_for_append = new SourceBufferRange(
          TypeToGapPolicy(GetType()),
          buffers, kNoDecodeTimestamp(),
          base::Bind(&SourceBufferStream::GetMaxInterbufferDistance,
                     base::Unretained(this)));
      range_for_next_append_ = ranges_.end();
    } else {
      bytes_to_free -= bytes_deleted;
      bytes_freed += bytes_deleted;
    }

    if (current_range->size_in_bytes() == 0) {
      DCHECK_NE(current_range, selected_range_);
      DCHECK(range_for_next_append_ == ranges_.end() ||
             *range_for_next_append_ != current_range);
      delete current_range;
      reverse_direction ? ranges_.pop_back() : ranges_.pop_front();
    }
  }

  // Insert |new_range_for_append| into |ranges_|, if applicable.
  if (new_range_for_append) {
    range_for_next_append_ = AddToRanges(new_range_for_append);
    DCHECK(range_for_next_append_ != ranges_.end());

    // Check to see if we need to merge |new_range_for_append| with the range
    // before or after it. |new_range_for_append| is created whenever the last
    // GOP appended is encountered, regardless of whether any buffers after it
    // are ultimately deleted. Merging is necessary if there were no buffers
    // (or very few buffers) deleted after creating |new_range_for_append|.
    if (range_for_next_append_ != ranges_.begin()) {
      RangeList::iterator range_before_next = range_for_next_append_;
      --range_before_next;
      MergeWithAdjacentRangeIfNecessary(range_before_next);
    }
    MergeWithAdjacentRangeIfNecessary(range_for_next_append_);
  }
  return bytes_freed;
}

void SourceBufferStream::PrepareRangesForNextAppend(
    const BufferQueue& new_buffers, BufferQueue* deleted_buffers) {
  DCHECK(deleted_buffers);

  bool temporarily_select_range = false;
  if (!track_buffer_.empty()) {
    DecodeTimestamp tb_timestamp = track_buffer_.back()->GetDecodeTimestamp();
    DecodeTimestamp seek_timestamp = FindKeyframeAfterTimestamp(tb_timestamp);
    if (seek_timestamp != kNoDecodeTimestamp() &&
        seek_timestamp < new_buffers.front()->GetDecodeTimestamp() &&
        range_for_next_append_ != ranges_.end() &&
        (*range_for_next_append_)->BelongsToRange(seek_timestamp)) {
      DCHECK(tb_timestamp < seek_timestamp);
      DCHECK(!selected_range_);
      DCHECK(!(*range_for_next_append_)->HasNextBufferPosition());

      // If there are GOPs between the end of the track buffer and the
      // beginning of the new buffers, then temporarily seek the range
      // so that the buffers between these two times will be deposited in
      // |deleted_buffers| as if they were part of the current playback
      // position.
      // TODO(acolwell): Figure out a more elegant way to do this.
      SeekAndSetSelectedRange(*range_for_next_append_, seek_timestamp);
      temporarily_select_range = true;
    }
  }

  // Handle splices between the existing buffers and the new buffers.  If a
  // splice is generated the timestamp and duration of the first buffer in
  // |new_buffers| will be modified.
  if (splice_frames_enabled_)
    GenerateSpliceFrame(new_buffers);

  DecodeTimestamp prev_timestamp = last_appended_buffer_timestamp_;
  bool prev_is_keyframe = last_appended_buffer_is_keyframe_;
  DecodeTimestamp next_timestamp = new_buffers.front()->GetDecodeTimestamp();
  bool next_is_keyframe = new_buffers.front()->IsKeyframe();

  if (prev_timestamp != kNoDecodeTimestamp() &&
      prev_timestamp != next_timestamp) {
    // Clean up the old buffers between the last appended buffer and the
    // beginning of |new_buffers|.
    RemoveInternal(prev_timestamp, next_timestamp, true, deleted_buffers);
  }

  // Make the delete range exclusive if we are dealing with an allowed same
  // timestamp situation. This prevents the first buffer in the current append
  // from deleting the last buffer in the previous append if both buffers
  // have the same timestamp.
  //
  // The delete range should never be exclusive if a splice frame was generated
  // because we don't generate splice frames for same timestamp situations.
  DCHECK(new_buffers.front()->splice_timestamp() !=
         new_buffers.front()->timestamp());
  const bool is_exclusive =
      new_buffers.front()->splice_buffers().empty() &&
      prev_timestamp == next_timestamp &&
      SourceBufferRange::AllowSameTimestamp(prev_is_keyframe, next_is_keyframe);

  // Delete the buffers that |new_buffers| overlaps.
  DecodeTimestamp start = new_buffers.front()->GetDecodeTimestamp();
  DecodeTimestamp end = new_buffers.back()->GetDecodeTimestamp();
  base::TimeDelta duration = new_buffers.back()->duration();

  if (duration != kNoTimestamp() && duration > base::TimeDelta()) {
    end += duration;
  } else {
    // TODO(acolwell): Ensure all buffers actually have proper
    // duration info so that this hack isn't needed.
    // http://crbug.com/312836
    end += base::TimeDelta::FromInternalValue(1);
  }

  RemoveInternal(start, end, is_exclusive, deleted_buffers);

  // Restore the range seek state if necessary.
  if (temporarily_select_range)
    SetSelectedRange(NULL);
}

bool SourceBufferStream::AreAdjacentInSequence(
    DecodeTimestamp first_timestamp, DecodeTimestamp second_timestamp) const {
  return first_timestamp < second_timestamp &&
      second_timestamp <=
      first_timestamp + ComputeFudgeRoom(GetMaxInterbufferDistance());
}

void SourceBufferStream::PruneTrackBuffer(const DecodeTimestamp timestamp) {
  // If we don't have the next timestamp, we don't have anything to delete.
  if (timestamp == kNoDecodeTimestamp())
    return;

  while (!track_buffer_.empty() &&
         track_buffer_.back()->GetDecodeTimestamp() >= timestamp) {
    track_buffer_.pop_back();
  }
}

void SourceBufferStream::MergeWithAdjacentRangeIfNecessary(
    const RangeList::iterator& range_with_new_buffers_itr) {
  DCHECK(range_with_new_buffers_itr != ranges_.end());

  SourceBufferRange* range_with_new_buffers = *range_with_new_buffers_itr;
  RangeList::iterator next_range_itr = range_with_new_buffers_itr;
  ++next_range_itr;

  if (next_range_itr == ranges_.end() ||
      !range_with_new_buffers->CanAppendRangeToEnd(**next_range_itr)) {
    return;
  }

  bool transfer_current_position = selected_range_ == *next_range_itr;
  range_with_new_buffers->AppendRangeToEnd(**next_range_itr,
                                           transfer_current_position);
  // Update |selected_range_| pointer if |range| has become selected after
  // merges.
  if (transfer_current_position)
    SetSelectedRange(range_with_new_buffers);

  if (next_range_itr == range_for_next_append_)
    range_for_next_append_ = range_with_new_buffers_itr;

  DeleteAndRemoveRange(&next_range_itr);
}

void SourceBufferStream::Seek(base::TimeDelta timestamp) {
  DCHECK(timestamp >= base::TimeDelta());
  ResetSeekState();

  if (ShouldSeekToStartOfBuffered(timestamp)) {
    ranges_.front()->SeekToStart();
    SetSelectedRange(ranges_.front());
    seek_pending_ = false;
    return;
  }

  seek_buffer_timestamp_ = timestamp;
  seek_pending_ = true;

  DecodeTimestamp seek_dts = DecodeTimestamp::FromPresentationTime(timestamp);

  RangeList::iterator itr = ranges_.end();
  for (itr = ranges_.begin(); itr != ranges_.end(); ++itr) {
    if ((*itr)->CanSeekTo(seek_dts))
      break;
  }

  if (itr == ranges_.end())
    return;

  SeekAndSetSelectedRange(*itr, seek_dts);
  seek_pending_ = false;
}

bool SourceBufferStream::IsSeekPending() const {
  return !(end_of_stream_ && IsEndSelected()) && seek_pending_;
}

void SourceBufferStream::OnSetDuration(base::TimeDelta duration) {
  DecodeTimestamp duration_dts =
      DecodeTimestamp::FromPresentationTime(duration);

  RangeList::iterator itr = ranges_.end();
  for (itr = ranges_.begin(); itr != ranges_.end(); ++itr) {
    if ((*itr)->GetEndTimestamp() > duration_dts)
      break;
  }
  if (itr == ranges_.end())
    return;

  // Need to partially truncate this range.
  if ((*itr)->GetStartTimestamp() < duration_dts) {
    bool delete_range = (*itr)->TruncateAt(duration_dts, NULL, false);
    if ((*itr == selected_range_) && !selected_range_->HasNextBufferPosition())
      SetSelectedRange(NULL);

    if (delete_range) {
      DeleteAndRemoveRange(&itr);
    } else {
      ++itr;
    }
  }

  // Delete all ranges that begin after |duration_dts|.
  while (itr != ranges_.end()) {
    // If we're about to delete the selected range, also reset the seek state.
    DCHECK((*itr)->GetStartTimestamp() >= duration_dts);
    if (*itr == selected_range_)
      ResetSeekState();
    DeleteAndRemoveRange(&itr);
  }
}

SourceBufferStream::Status SourceBufferStream::GetNextBuffer(
    scoped_refptr<StreamParserBuffer>* out_buffer) {
  if (!pending_buffer_.get()) {
    const SourceBufferStream::Status status = GetNextBufferInternal(out_buffer);
    if (status != SourceBufferStream::kSuccess || !SetPendingBuffer(out_buffer))
      return status;
  }

  if (!pending_buffer_->splice_buffers().empty())
    return HandleNextBufferWithSplice(out_buffer);

  DCHECK(pending_buffer_->preroll_buffer().get());
  return HandleNextBufferWithPreroll(out_buffer);
}

SourceBufferStream::Status SourceBufferStream::HandleNextBufferWithSplice(
    scoped_refptr<StreamParserBuffer>* out_buffer) {
  const BufferQueue& splice_buffers = pending_buffer_->splice_buffers();
  const size_t last_splice_buffer_index = splice_buffers.size() - 1;

  // Are there any splice buffers left to hand out?  The last buffer should be
  // handed out separately since it represents the first post-splice buffer.
  if (splice_buffers_index_ < last_splice_buffer_index) {
    // Account for config changes which occur between fade out buffers.
    if (current_config_index_ !=
        splice_buffers[splice_buffers_index_]->GetConfigId()) {
      config_change_pending_ = true;
      DVLOG(1) << "Config change (splice buffer config ID does not match).";
      return SourceBufferStream::kConfigChange;
    }

    // Every pre splice buffer must have the same splice_timestamp().
    DCHECK(pending_buffer_->splice_timestamp() ==
           splice_buffers[splice_buffers_index_]->splice_timestamp());

    // No pre splice buffers should have preroll.
    DCHECK(!splice_buffers[splice_buffers_index_]->preroll_buffer().get());

    *out_buffer = splice_buffers[splice_buffers_index_++];
    return SourceBufferStream::kSuccess;
  }

  // Did we hand out the last pre-splice buffer on the previous call?
  if (!pending_buffers_complete_) {
    DCHECK_EQ(splice_buffers_index_, last_splice_buffer_index);
    pending_buffers_complete_ = true;
    config_change_pending_ = true;
    DVLOG(1) << "Config change (forced for fade in of splice frame).";
    return SourceBufferStream::kConfigChange;
  }

  // All pre-splice buffers have been handed out and a config change completed,
  // so hand out the final buffer for fade in.  Because a config change is
  // always issued prior to handing out this buffer, any changes in config id
  // have been inherently handled.
  DCHECK(pending_buffers_complete_);
  DCHECK_EQ(splice_buffers_index_, splice_buffers.size() - 1);
  DCHECK(splice_buffers.back()->splice_timestamp() == kNoTimestamp());
  *out_buffer = splice_buffers.back();
  pending_buffer_ = NULL;

  // If the last splice buffer has preroll, hand off to the preroll handler.
  return SetPendingBuffer(out_buffer) ? HandleNextBufferWithPreroll(out_buffer)
                                      : SourceBufferStream::kSuccess;
}

SourceBufferStream::Status SourceBufferStream::HandleNextBufferWithPreroll(
    scoped_refptr<StreamParserBuffer>* out_buffer) {
  // Any config change should have already been handled.
  DCHECK_EQ(current_config_index_, pending_buffer_->GetConfigId());

  // Check if the preroll buffer has already been handed out.
  if (!pending_buffers_complete_) {
    pending_buffers_complete_ = true;
    *out_buffer = pending_buffer_->preroll_buffer();
    return SourceBufferStream::kSuccess;
  }

  // Preroll complete, hand out the final buffer.
  *out_buffer = pending_buffer_;
  pending_buffer_ = NULL;
  return SourceBufferStream::kSuccess;
}

SourceBufferStream::Status SourceBufferStream::GetNextBufferInternal(
    scoped_refptr<StreamParserBuffer>* out_buffer) {
  CHECK(!config_change_pending_);

  if (!track_buffer_.empty()) {
    DCHECK(!selected_range_);
    scoped_refptr<StreamParserBuffer>& next_buffer = track_buffer_.front();

    // If the next buffer is an audio splice frame, the next effective config id
    // comes from the first splice buffer.
    if (next_buffer->GetSpliceBufferConfigId(0) != current_config_index_) {
      config_change_pending_ = true;
      DVLOG(1) << "Config change (track buffer config ID does not match).";
      return kConfigChange;
    }

    *out_buffer = next_buffer;
    track_buffer_.pop_front();
    last_output_buffer_timestamp_ = (*out_buffer)->GetDecodeTimestamp();

    // If the track buffer becomes empty, then try to set the selected range
    // based on the timestamp of this buffer being returned.
    if (track_buffer_.empty())
      SetSelectedRangeIfNeeded(last_output_buffer_timestamp_);

    return kSuccess;
  }

  if (!selected_range_ || !selected_range_->HasNextBuffer()) {
    if (end_of_stream_ && IsEndSelected())
      return kEndOfStream;
    return kNeedBuffer;
  }

  if (selected_range_->GetNextConfigId() != current_config_index_) {
    config_change_pending_ = true;
    DVLOG(1) << "Config change (selected range config ID does not match).";
    return kConfigChange;
  }

  CHECK(selected_range_->GetNextBuffer(out_buffer));
  last_output_buffer_timestamp_ = (*out_buffer)->GetDecodeTimestamp();
  return kSuccess;
}

DecodeTimestamp SourceBufferStream::GetNextBufferTimestamp() {
  if (!track_buffer_.empty())
    return track_buffer_.front()->GetDecodeTimestamp();

  if (!selected_range_)
    return kNoDecodeTimestamp();

  DCHECK(selected_range_->HasNextBufferPosition());
  return selected_range_->GetNextTimestamp();
}

SourceBufferStream::RangeList::iterator
SourceBufferStream::FindExistingRangeFor(DecodeTimestamp start_timestamp) {
  for (RangeList::iterator itr = ranges_.begin(); itr != ranges_.end(); ++itr) {
    if ((*itr)->BelongsToRange(start_timestamp))
      return itr;
  }
  return ranges_.end();
}

SourceBufferStream::RangeList::iterator
SourceBufferStream::AddToRanges(SourceBufferRange* new_range) {
  DecodeTimestamp start_timestamp = new_range->GetStartTimestamp();
  RangeList::iterator itr = ranges_.end();
  for (itr = ranges_.begin(); itr != ranges_.end(); ++itr) {
    if ((*itr)->GetStartTimestamp() > start_timestamp)
      break;
  }
  return ranges_.insert(itr, new_range);
}

SourceBufferStream::RangeList::iterator
SourceBufferStream::GetSelectedRangeItr() {
  DCHECK(selected_range_);
  RangeList::iterator itr = ranges_.end();
  for (itr = ranges_.begin(); itr != ranges_.end(); ++itr) {
    if (*itr == selected_range_)
      break;
  }
  DCHECK(itr != ranges_.end());
  return itr;
}

void SourceBufferStream::SeekAndSetSelectedRange(
    SourceBufferRange* range, DecodeTimestamp seek_timestamp) {
  if (range)
    range->Seek(seek_timestamp);
  SetSelectedRange(range);
}

void SourceBufferStream::SetSelectedRange(SourceBufferRange* range) {
  DVLOG(1) << __FUNCTION__ << " : " << selected_range_ << " -> " << range;
  if (selected_range_)
    selected_range_->ResetNextBufferPosition();
  DCHECK(!range || range->HasNextBufferPosition());
  selected_range_ = range;
}

Ranges<base::TimeDelta> SourceBufferStream::GetBufferedTime() const {
  Ranges<base::TimeDelta> ranges;
  for (RangeList::const_iterator itr = ranges_.begin();
       itr != ranges_.end(); ++itr) {
    ranges.Add((*itr)->GetStartTimestamp().ToPresentationTime(),
               (*itr)->GetBufferedEndTimestamp().ToPresentationTime());
  }
  return ranges;
}

base::TimeDelta SourceBufferStream::GetBufferedDuration() const {
  if (ranges_.empty())
    return base::TimeDelta();

  return ranges_.back()->GetBufferedEndTimestamp().ToPresentationTime();
}

void SourceBufferStream::MarkEndOfStream() {
  DCHECK(!end_of_stream_);
  end_of_stream_ = true;
}

void SourceBufferStream::UnmarkEndOfStream() {
  DCHECK(end_of_stream_);
  end_of_stream_ = false;
}

bool SourceBufferStream::IsEndSelected() const {
  if (ranges_.empty())
    return true;

  if (seek_pending_) {
    base::TimeDelta last_range_end_time =
        ranges_.back()->GetBufferedEndTimestamp().ToPresentationTime();
    return seek_buffer_timestamp_ >= last_range_end_time;
  }

  return selected_range_ == ranges_.back();
}

const AudioDecoderConfig& SourceBufferStream::GetCurrentAudioDecoderConfig() {
  if (config_change_pending_)
    CompleteConfigChange();
  return audio_configs_[current_config_index_];
}

const VideoDecoderConfig& SourceBufferStream::GetCurrentVideoDecoderConfig() {
  if (config_change_pending_)
    CompleteConfigChange();
  return video_configs_[current_config_index_];
}

const TextTrackConfig& SourceBufferStream::GetCurrentTextTrackConfig() {
  return text_track_config_;
}

base::TimeDelta SourceBufferStream::GetMaxInterbufferDistance() const {
  if (max_interbuffer_distance_ == kNoTimestamp())
    return base::TimeDelta::FromMilliseconds(kDefaultBufferDurationInMs);
  return max_interbuffer_distance_;
}

bool SourceBufferStream::UpdateAudioConfig(const AudioDecoderConfig& config) {
  DCHECK(!audio_configs_.empty());
  DCHECK(video_configs_.empty());
  DVLOG(3) << "UpdateAudioConfig.";

  if (audio_configs_[0].codec() != config.codec()) {
    MEDIA_LOG(log_cb_) << "Audio codec changes not allowed.";
    return false;
  }

  if (audio_configs_[0].is_encrypted() != config.is_encrypted()) {
    MEDIA_LOG(log_cb_) << "Audio encryption changes not allowed.";
    return false;
  }

  // Check to see if the new config matches an existing one.
  for (size_t i = 0; i < audio_configs_.size(); ++i) {
    if (config.Matches(audio_configs_[i])) {
      append_config_index_ = i;
      return true;
    }
  }

  // No matches found so let's add this one to the list.
  append_config_index_ = audio_configs_.size();
  DVLOG(2) << "New audio config - index: " << append_config_index_;
  audio_configs_.resize(audio_configs_.size() + 1);
  audio_configs_[append_config_index_] = config;
  return true;
}

bool SourceBufferStream::UpdateVideoConfig(const VideoDecoderConfig& config) {
  DCHECK(!video_configs_.empty());
  DCHECK(audio_configs_.empty());
  DVLOG(3) << "UpdateVideoConfig.";

  if (video_configs_[0].codec() != config.codec()) {
    MEDIA_LOG(log_cb_) << "Video codec changes not allowed.";
    return false;
  }

  if (video_configs_[0].is_encrypted() != config.is_encrypted()) {
    MEDIA_LOG(log_cb_) << "Video encryption changes not allowed.";
    return false;
  }

  // Check to see if the new config matches an existing one.
  for (size_t i = 0; i < video_configs_.size(); ++i) {
    if (config.Matches(video_configs_[i])) {
      append_config_index_ = i;
      return true;
    }
  }

  // No matches found so let's add this one to the list.
  append_config_index_ = video_configs_.size();
  DVLOG(2) << "New video config - index: " << append_config_index_;
  video_configs_.resize(video_configs_.size() + 1);
  video_configs_[append_config_index_] = config;
  return true;
}

void SourceBufferStream::CompleteConfigChange() {
  config_change_pending_ = false;

  if (pending_buffer_.get()) {
    current_config_index_ =
        pending_buffer_->GetSpliceBufferConfigId(splice_buffers_index_);
    return;
  }

  if (!track_buffer_.empty()) {
    current_config_index_ = track_buffer_.front()->GetSpliceBufferConfigId(0);
    return;
  }

  if (selected_range_ && selected_range_->HasNextBuffer())
    current_config_index_ = selected_range_->GetNextConfigId();
}

void SourceBufferStream::SetSelectedRangeIfNeeded(
    const DecodeTimestamp timestamp) {
  DVLOG(1) << __FUNCTION__ << "(" << timestamp.InSecondsF() << ")";

  if (selected_range_) {
    DCHECK(track_buffer_.empty());
    return;
  }

  if (!track_buffer_.empty()) {
    DCHECK(!selected_range_);
    return;
  }

  DecodeTimestamp start_timestamp = timestamp;

  // If the next buffer timestamp is not known then use a timestamp just after
  // the timestamp on the last buffer returned by GetNextBuffer().
  if (start_timestamp == kNoDecodeTimestamp()) {
    if (last_output_buffer_timestamp_ == kNoDecodeTimestamp())
      return;

    start_timestamp = last_output_buffer_timestamp_ +
        base::TimeDelta::FromInternalValue(1);
  }

  DecodeTimestamp seek_timestamp =
      FindNewSelectedRangeSeekTimestamp(start_timestamp);

  // If we don't have buffered data to seek to, then return.
  if (seek_timestamp == kNoDecodeTimestamp())
    return;

  DCHECK(track_buffer_.empty());
  SeekAndSetSelectedRange(*FindExistingRangeFor(seek_timestamp),
                          seek_timestamp);
}

DecodeTimestamp SourceBufferStream::FindNewSelectedRangeSeekTimestamp(
    const DecodeTimestamp start_timestamp) {
  DCHECK(start_timestamp != kNoDecodeTimestamp());
  DCHECK(start_timestamp >= DecodeTimestamp());

  RangeList::iterator itr = ranges_.begin();

  for (; itr != ranges_.end(); ++itr) {
    if ((*itr)->GetEndTimestamp() >= start_timestamp) {
      break;
    }
  }

  if (itr == ranges_.end())
    return kNoDecodeTimestamp();

  // First check for a keyframe timestamp >= |start_timestamp|
  // in the current range.
  DecodeTimestamp keyframe_timestamp =
      (*itr)->NextKeyframeTimestamp(start_timestamp);

  if (keyframe_timestamp != kNoDecodeTimestamp())
    return keyframe_timestamp;

  // If a keyframe was not found then look for a keyframe that is
  // "close enough" in the current or next range.
  DecodeTimestamp end_timestamp =
      start_timestamp + ComputeFudgeRoom(GetMaxInterbufferDistance());
  DCHECK(start_timestamp < end_timestamp);

  // Make sure the current range doesn't start beyond |end_timestamp|.
  if ((*itr)->GetStartTimestamp() >= end_timestamp)
    return kNoDecodeTimestamp();

  keyframe_timestamp = (*itr)->KeyframeBeforeTimestamp(end_timestamp);

  // Check to see if the keyframe is within the acceptable range
  // (|start_timestamp|, |end_timestamp|].
  if (keyframe_timestamp != kNoDecodeTimestamp() &&
      start_timestamp < keyframe_timestamp  &&
      keyframe_timestamp <= end_timestamp) {
    return keyframe_timestamp;
  }

  // If |end_timestamp| is within this range, then no other checks are
  // necessary.
  if (end_timestamp <= (*itr)->GetEndTimestamp())
    return kNoDecodeTimestamp();

  // Move on to the next range.
  ++itr;

  // Return early if the next range does not contain |end_timestamp|.
  if (itr == ranges_.end() || (*itr)->GetStartTimestamp() >= end_timestamp)
    return kNoDecodeTimestamp();

  keyframe_timestamp = (*itr)->KeyframeBeforeTimestamp(end_timestamp);

  // Check to see if the keyframe is within the acceptable range
  // (|start_timestamp|, |end_timestamp|].
  if (keyframe_timestamp != kNoDecodeTimestamp() &&
      start_timestamp < keyframe_timestamp  &&
      keyframe_timestamp <= end_timestamp) {
    return keyframe_timestamp;
  }

  return kNoDecodeTimestamp();
}

DecodeTimestamp SourceBufferStream::FindKeyframeAfterTimestamp(
    const DecodeTimestamp timestamp) {
  DCHECK(timestamp != kNoDecodeTimestamp());

  RangeList::iterator itr = FindExistingRangeFor(timestamp);

  if (itr == ranges_.end())
    return kNoDecodeTimestamp();

  // First check for a keyframe timestamp >= |timestamp|
  // in the current range.
  return (*itr)->NextKeyframeTimestamp(timestamp);
}

std::string SourceBufferStream::GetStreamTypeName() const {
  switch (GetType()) {
    case kAudio:
      return "AUDIO";
    case kVideo:
      return "VIDEO";
    case kText:
      return "TEXT";
  }
  NOTREACHED();
  return "";
}

SourceBufferStream::Type SourceBufferStream::GetType() const {
  if (!audio_configs_.empty())
    return kAudio;
  if (!video_configs_.empty())
    return kVideo;
  DCHECK_NE(text_track_config_.kind(), kTextNone);
  return kText;
}

void SourceBufferStream::DeleteAndRemoveRange(RangeList::iterator* itr) {
  DVLOG(1) << __FUNCTION__;

  DCHECK(*itr != ranges_.end());
  if (**itr == selected_range_) {
    DVLOG(1) << __FUNCTION__ << " deleting selected range.";
    SetSelectedRange(NULL);
  }

  if (*itr == range_for_next_append_) {
    DVLOG(1) << __FUNCTION__ << " deleting range_for_next_append_.";
    range_for_next_append_ = ranges_.end();
    last_appended_buffer_timestamp_ = kNoDecodeTimestamp();
    last_appended_buffer_is_keyframe_ = false;
  }

  delete **itr;
  *itr = ranges_.erase(*itr);
}

void SourceBufferStream::GenerateSpliceFrame(const BufferQueue& new_buffers) {
  DCHECK(!new_buffers.empty());

  // Splice frames are only supported for audio.
  if (GetType() != kAudio)
    return;

  // Find the overlapped range (if any).
  const base::TimeDelta splice_timestamp = new_buffers.front()->timestamp();
  const DecodeTimestamp splice_dts =
      DecodeTimestamp::FromPresentationTime(splice_timestamp);
  RangeList::iterator range_itr = FindExistingRangeFor(splice_dts);
  if (range_itr == ranges_.end())
    return;

  const DecodeTimestamp max_splice_end_dts =
      splice_dts + base::TimeDelta::FromMilliseconds(
          AudioSplicer::kCrossfadeDurationInMilliseconds);

  // Find all buffers involved before the splice point.
  BufferQueue pre_splice_buffers;
  if (!(*range_itr)->GetBuffersInRange(
          splice_dts, max_splice_end_dts, &pre_splice_buffers)) {
    return;
  }

  // If there are gaps in the timeline, it's possible that we only find buffers
  // after the splice point but within the splice range.  For simplicity, we do
  // not generate splice frames in this case.
  //
  // We also do not want to generate splices if the first new buffer replaces an
  // existing buffer exactly.
  if (pre_splice_buffers.front()->timestamp() >= splice_timestamp)
    return;

  // If any |pre_splice_buffers| are already splices or preroll, do not generate
  // a splice.
  for (size_t i = 0; i < pre_splice_buffers.size(); ++i) {
    const BufferQueue& original_splice_buffers =
        pre_splice_buffers[i]->splice_buffers();
    if (!original_splice_buffers.empty()) {
      DVLOG(1) << "Can't generate splice: overlapped buffers contain a "
                  "pre-existing splice.";
      return;
    }

    if (pre_splice_buffers[i]->preroll_buffer().get()) {
      DVLOG(1) << "Can't generate splice: overlapped buffers contain preroll.";
      return;
    }
  }

  // Don't generate splice frames which represent less than two frames, since we
  // need at least that much to generate a crossfade.  Per the spec, make this
  // check using the sample rate of the overlapping buffers.
  const base::TimeDelta splice_duration =
      pre_splice_buffers.back()->timestamp() +
      pre_splice_buffers.back()->duration() - splice_timestamp;
  const base::TimeDelta minimum_splice_duration = base::TimeDelta::FromSecondsD(
      2.0 / audio_configs_[append_config_index_].samples_per_second());
  if (splice_duration < minimum_splice_duration) {
    DVLOG(1) << "Can't generate splice: not enough samples for crossfade; have "
             << splice_duration.InMicroseconds() << " us, but need "
             << minimum_splice_duration.InMicroseconds() << " us.";
    return;
  }

  new_buffers.front()->ConvertToSpliceBuffer(pre_splice_buffers);
}

bool SourceBufferStream::SetPendingBuffer(
    scoped_refptr<StreamParserBuffer>* out_buffer) {
  DCHECK(out_buffer->get());
  DCHECK(!pending_buffer_.get());

  const bool have_splice_buffers = !(*out_buffer)->splice_buffers().empty();
  const bool have_preroll_buffer = !!(*out_buffer)->preroll_buffer().get();

  if (!have_splice_buffers && !have_preroll_buffer)
    return false;

  DCHECK_NE(have_splice_buffers, have_preroll_buffer);
  splice_buffers_index_ = 0;
  pending_buffer_.swap(*out_buffer);
  pending_buffers_complete_ = false;
  return true;
}

}  // namespace media
