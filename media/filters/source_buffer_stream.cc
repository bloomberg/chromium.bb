// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/source_buffer_stream.h"

#include <algorithm>
#include <map>
#include <sstream>
#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "media/base/media_switches.h"
#include "media/base/timestamp_constants.h"
#include "media/filters/source_buffer_platform.h"
#include "media/filters/source_buffer_range.h"

namespace media {

namespace {

// An arbitrarily-chosen number to estimate the duration of a buffer if none is
// set and there's not enough information to get a better estimate.
const int kDefaultBufferDurationInMs = 125;

// Limit the number of MEDIA_LOG() logs for track buffer time gaps.
const int kMaxTrackBufferGapWarningLogs = 20;

// Limit the number of MEDIA_LOG() logs for MSE GC algorithm warnings.
const int kMaxGarbageCollectAlgorithmWarningLogs = 20;

// Limit the number of MEDIA_LOG() logs for splice overlap trimming.
const int kMaxAudioSpliceLogs = 20;

// Limit the number of MEDIA_LOG() logs for same DTS for non-keyframe followed
// by keyframe. Prior to relaxing the "media segments must begin with a
// keyframe" requirement, we issued decode error for this situation. That was
// likely too strict, and now that the keyframe requirement is relaxed, we have
// no knowledge of media segment boundaries here. Now, we log but don't trigger
// decode error, since we allow these sequences which may cause extra decoder
// work or other side-effects.
const int kMaxStrangeSameTimestampsLogs = 20;

// Helper method that returns true if |ranges| is sorted in increasing order,
// false otherwise.
bool IsRangeListSorted(const std::list<media::SourceBufferRange*>& ranges) {
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
// detection.
// See http://crbug.com/351489 and http://crbug.com/351166.
base::TimeDelta ComputeFudgeRoom(base::TimeDelta approximate_duration) {
  // Because we do not know exactly when is the next timestamp, any buffer
  // that starts within 2x the approximate duration of a buffer is considered
  // within this range.
  return 2 * approximate_duration;
}

// The amount of time the beginning of the buffered data can differ from the
// start time in order to still be considered the start of stream.
base::TimeDelta kSeekToStartFudgeRoom() {
  return base::TimeDelta::FromMilliseconds(1000);
}

// Helper method for logging, converts a range into a readable string.
std::string RangeToString(const SourceBufferRange& range) {
  if (range.size_in_bytes() == 0) {
    return "[]";
  }
  std::stringstream ss;
  ss << "[" << range.GetStartTimestamp().InSecondsF()
     << ";" << range.GetEndTimestamp().InSecondsF()
     << "(" << range.GetBufferedEndTimestamp().InSecondsF() << ")]";
  return ss.str();
}

// Helper method for logging, converts a set of ranges into a readable string.
std::string RangesToString(const SourceBufferStream::RangeList& ranges) {
  if (ranges.empty())
    return "<EMPTY>";

  std::stringstream ss;
  for (const auto* range_ptr : ranges) {
    if (range_ptr != ranges.front())
      ss << " ";
    ss << RangeToString(*range_ptr);
  }
  return ss.str();
}

std::string BufferQueueToLogString(
    const SourceBufferStream::BufferQueue& buffers) {
  std::stringstream result;
  if (buffers.front()->GetDecodeTimestamp().InMicroseconds() ==
      buffers.front()->timestamp().InMicroseconds() &&
      buffers.back()->GetDecodeTimestamp().InMicroseconds() ==
      buffers.back()->timestamp().InMicroseconds()) {
    result << "dts/pts=[" << buffers.front()->timestamp().InSecondsF() << ";"
           << buffers.back()->timestamp().InSecondsF() << "(last frame dur="
           << buffers.back()->duration().InSecondsF() << ")]";
  } else {
    result << "dts=[" << buffers.front()->GetDecodeTimestamp().InSecondsF()
           << ";" << buffers.back()->GetDecodeTimestamp().InSecondsF()
           << "] pts=[" << buffers.front()->timestamp().InSecondsF() << ";"
           << buffers.back()->timestamp().InSecondsF() << "(last frame dur="
           << buffers.back()->duration().InSecondsF() << ")]";
  }
  return result.str();
}

SourceBufferRange::GapPolicy TypeToGapPolicy(SourceBufferStream::Type type) {
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

}  // namespace

SourceBufferStream::SourceBufferStream(const AudioDecoderConfig& audio_config,
                                       MediaLog* media_log)
    : media_log_(media_log),
      seek_buffer_timestamp_(kNoTimestamp),
      coded_frame_group_start_time_(kNoDecodeTimestamp()),
      range_for_next_append_(ranges_.end()),
      last_output_buffer_timestamp_(kNoDecodeTimestamp()),
      max_interbuffer_distance_(kNoTimestamp),
      memory_limit_(kSourceBufferAudioMemoryLimit) {
  DCHECK(audio_config.IsValidConfig());
  audio_configs_.push_back(audio_config);
}

SourceBufferStream::SourceBufferStream(const VideoDecoderConfig& video_config,
                                       MediaLog* media_log)
    : media_log_(media_log),
      seek_buffer_timestamp_(kNoTimestamp),
      coded_frame_group_start_time_(kNoDecodeTimestamp()),
      range_for_next_append_(ranges_.end()),
      last_output_buffer_timestamp_(kNoDecodeTimestamp()),
      max_interbuffer_distance_(kNoTimestamp),
      memory_limit_(kSourceBufferVideoMemoryLimit) {
  DCHECK(video_config.IsValidConfig());
  video_configs_.push_back(video_config);
}

SourceBufferStream::SourceBufferStream(const TextTrackConfig& text_config,
                                       MediaLog* media_log)
    : media_log_(media_log),
      text_track_config_(text_config),
      seek_buffer_timestamp_(kNoTimestamp),
      coded_frame_group_start_time_(kNoDecodeTimestamp()),
      range_for_next_append_(ranges_.end()),
      last_output_buffer_timestamp_(kNoDecodeTimestamp()),
      max_interbuffer_distance_(kNoTimestamp),
      memory_limit_(kSourceBufferAudioMemoryLimit) {}

SourceBufferStream::~SourceBufferStream() {
  while (!ranges_.empty()) {
    delete ranges_.front();
    ranges_.pop_front();
  }
}

void SourceBufferStream::OnStartOfCodedFrameGroup(
    DecodeTimestamp coded_frame_group_start_time) {
  DVLOG(1) << __func__ << " " << GetStreamTypeName() << " ("
           << coded_frame_group_start_time.InSecondsF() << ")";
  DCHECK(!end_of_stream_);
  coded_frame_group_start_time_ = coded_frame_group_start_time;
  new_coded_frame_group_ = true;

  RangeList::iterator last_range = range_for_next_append_;
  range_for_next_append_ = FindExistingRangeFor(coded_frame_group_start_time);

  // Only reset |last_appended_buffer_timestamp_| if this new coded frame group
  // is not adjacent to the previous coded frame group appended to the stream.
  if (range_for_next_append_ == ranges_.end() ||
      !AreAdjacentInSequence(last_appended_buffer_timestamp_,
                             coded_frame_group_start_time)) {
    ResetLastAppendedState();
    DVLOG(3) << __func__ << " next appended buffers will "
             << (range_for_next_append_ == ranges_.end()
                     ? "be in a new range"
                     : "overlap an existing range");
  } else if (last_range != ranges_.end()) {
    DCHECK(last_range == range_for_next_append_);
    DVLOG(3) << __func__ << " next appended buffers will continue range unless "
             << "intervening remove makes discontinuity";
  }
}

bool SourceBufferStream::Append(const BufferQueue& buffers) {
  TRACE_EVENT2("media", "SourceBufferStream::Append",
               "stream type", GetStreamTypeName(),
               "buffers to append", buffers.size());

  DCHECK(!buffers.empty());
  DCHECK(coded_frame_group_start_time_ != kNoDecodeTimestamp());
  DCHECK(coded_frame_group_start_time_ <=
         buffers.front()->GetDecodeTimestamp());
  DCHECK(!end_of_stream_);

  DVLOG(1) << __func__ << " " << GetStreamTypeName() << ": buffers "
           << BufferQueueToLogString(buffers);

  // New coded frame groups emitted by the coded frame processor must begin with
  // a keyframe. TODO(wolenetz): Change this to [DCHECK + MEDIA_LOG(ERROR...) +
  // return false] once the CHECK has baked in a stable release. See
  // https://crbug.com/580621.
  CHECK(!new_coded_frame_group_ || buffers.front()->is_key_frame());

  // Buffers within a coded frame group should be monotonically increasing.
  if (!IsMonotonicallyIncreasing(buffers)) {
    return false;
  }

  if (coded_frame_group_start_time_ < DecodeTimestamp() ||
      buffers.front()->GetDecodeTimestamp() < DecodeTimestamp()) {
    MEDIA_LOG(ERROR, media_log_)
        << "Cannot append a coded frame group with negative timestamps.";
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
    if (new_coded_frame_group_) {
      // If the first append to this stream in a new coded frame group continues
      // a previous range, use the new group's start time instead of the first
      // new buffer's timestamp as the proof of adjacency to the existing range.
      // A large gap (larger than our normal buffer adjacency test) can occur in
      // a muxed set of streams (which share a common coded frame group start
      // time) with a significantly jagged start across the streams.
      (*range_for_next_append_)
          ->AppendBuffersToEnd(buffers, coded_frame_group_start_time_);
    } else {
      // Otherwise, use the first new buffer's timestamp as the proof of
      // adjacency.
      (*range_for_next_append_)
          ->AppendBuffersToEnd(buffers, kNoDecodeTimestamp());
    }

    last_appended_buffer_timestamp_ = buffers.back()->GetDecodeTimestamp();
    last_appended_buffer_duration_ = buffers.back()->duration();
    last_appended_buffer_is_keyframe_ = buffers.back()->is_key_frame();
  } else {
    DecodeTimestamp new_range_start_time = std::min(
        coded_frame_group_start_time_, buffers.front()->GetDecodeTimestamp());
    const BufferQueue* buffers_for_new_range = &buffers;
    BufferQueue trimmed_buffers;

    // If the new range is not being created because of a new coded frame group,
    // then we must make sure that we start with a key frame.  This can happen
    // if the GOP in the previous append gets destroyed by a Remove() call.
    if (!new_coded_frame_group_) {
      BufferQueue::const_iterator itr = buffers.begin();

      // Scan past all the non-key-frames.
      while (itr != buffers.end() && !(*itr)->is_key_frame()) {
        ++itr;
      }

      // If we didn't find a key frame, then update the last appended
      // buffer state and return.
      if (itr == buffers.end()) {
        last_appended_buffer_timestamp_ = buffers.back()->GetDecodeTimestamp();
        last_appended_buffer_duration_ = buffers.back()->duration();
        last_appended_buffer_is_keyframe_ = buffers.back()->is_key_frame();
        DVLOG(1) << __func__ << " " << GetStreamTypeName()
                 << ": new buffers in the middle of coded frame group depend on"
                    " keyframe that has been removed, and contain no keyframes."
                    " Skipping further processing.";
        DVLOG(1) << __func__ << " " << GetStreamTypeName()
                 << ": done. ranges_=" << RangesToString(ranges_);
        return true;
      } else if (itr != buffers.begin()) {
        // Copy the first key frame and everything after it into
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
    last_appended_buffer_duration_ = buffers_for_new_range->back()->duration();
    last_appended_buffer_is_keyframe_ =
        buffers_for_new_range->back()->is_key_frame();
  }

  new_coded_frame_group_ = false;

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
    DVLOG(3) << __func__ << " " << GetStreamTypeName() << " Added "
             << deleted_buffers.size()
             << " buffers to track buffer. TB size is now "
             << track_buffer_.size();
  } else {
    DVLOG(3) << __func__ << " " << GetStreamTypeName()
             << " No deleted buffers for track buffer";
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

  DVLOG(1) << __func__ << " " << GetStreamTypeName()
           << ": done. ranges_=" << RangesToString(ranges_);
  DCHECK(IsRangeListSorted(ranges_));
  DCHECK(OnlySelectedRangeIsSeeked());
  return true;
}

void SourceBufferStream::Remove(base::TimeDelta start, base::TimeDelta end,
                                base::TimeDelta duration) {
  DVLOG(1) << __func__ << " " << GetStreamTypeName() << " ("
           << start.InSecondsF() << ", " << end.InSecondsF() << ", "
           << duration.InSecondsF() << ")";
  DCHECK(start >= base::TimeDelta()) << start.InSecondsF();
  DCHECK(start < end) << "start " << start.InSecondsF()
                      << " end " << end.InSecondsF();
  DCHECK(duration != kNoTimestamp);

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

  if (!deleted_buffers.empty()) {
    // Buffers for the current position have been removed.
    SetSelectedRangeIfNeeded(deleted_buffers.front()->GetDecodeTimestamp());
    if (last_output_buffer_timestamp_ == kNoDecodeTimestamp()) {
      // We just removed buffers for the current playback position for this
      // stream, yet we also had output no buffer since the last Seek.
      // Re-seek to prevent stall.
      DVLOG(1) << __func__ << " " << GetStreamTypeName() << ": re-seeking to "
               << seek_buffer_timestamp_
               << " to prevent stall if this time becomes buffered again";
      Seek(seek_buffer_timestamp_);
    }
  }
}

DecodeTimestamp SourceBufferStream::PotentialNextAppendTimestamp() const {
  // The next potential append will either be just at or after
  // |last_appended_buffer_timestamp_| (if known), or if unknown and we are
  // still at the beginning of a new coded frame group, then will be into the
  // range (if any) to which |coded_frame_group_start_time_| belongs.
  if (last_appended_buffer_timestamp_ != kNoDecodeTimestamp()) {
    // TODO(wolenetz): Determine if this +1us is still necessary. See
    // https://crbug.com/589295.
    return last_appended_buffer_timestamp_ +
           base::TimeDelta::FromInternalValue(1);
  }

  if (new_coded_frame_group_)
    return coded_frame_group_start_time_;

  // If we still don't know a potential next append timestamp, then we have
  // removed the ranged to which it previously belonged and have not completed a
  // subsequent append or received a subsequent OnStartOfCodedFrameGroup()
  // signal.
  return kNoDecodeTimestamp();
}

void SourceBufferStream::UpdateLastAppendStateForRemove(
    DecodeTimestamp remove_start,
    DecodeTimestamp remove_end,
    bool exclude_start) {
  // TODO(chcunningham): change exclude_start to include_start in this class and
  // SourceBufferRange. Negatives are hard to reason about.
  bool include_start = !exclude_start;

  // No need to check previous append's GOP if starting a new CFG. New CFG is
  // already required to begin with a key frame.
  if (new_coded_frame_group_)
    return;

  if (range_for_next_append_ != ranges_.end()) {
    if (last_appended_buffer_timestamp_ != kNoDecodeTimestamp()) {
      DCHECK((*range_for_next_append_)
                 ->BelongsToRange(last_appended_buffer_timestamp_));

      // Note start and end of last appended GOP.
      DecodeTimestamp gop_end = last_appended_buffer_timestamp_;
      DecodeTimestamp gop_start =
          (*range_for_next_append_)->KeyframeBeforeTimestamp(gop_end);

      // If last append is about to be disrupted, reset associated state so we
      // know to create a new range for future appends and require an initial
      // key frame.
      if (((include_start && remove_start == gop_end) ||
           remove_start < gop_end) &&
          remove_end > gop_start) {
        DVLOG(2) << __func__ << " " << GetStreamTypeName()
                 << " Resetting next append state for remove ("
                 << remove_start.InSecondsF() << ", " << remove_end.InSecondsF()
                 << ", " << exclude_start << ")";
        range_for_next_append_ = ranges_.end();
        ResetLastAppendedState();
      }
    } else {
      NOTREACHED() << __func__ << " " << GetStreamTypeName()
                   << " range_for_next_append_ set, but not tracking last"
                   << " append nor new coded frame group.";
    }
  }
}

void SourceBufferStream::RemoveInternal(DecodeTimestamp start,
                                        DecodeTimestamp end,
                                        bool exclude_start,
                                        BufferQueue* deleted_buffers) {
  DVLOG(2) << __func__ << " " << GetStreamTypeName() << " ("
           << start.InSecondsF() << ", " << end.InSecondsF() << ", "
           << exclude_start << ")";
  DVLOG(3) << __func__ << " " << GetStreamTypeName()
           << ": before remove ranges_=" << RangesToString(ranges_);

  DCHECK(start >= DecodeTimestamp());
  DCHECK(start < end) << "start " << start.InSecondsF()
                      << " end " << end.InSecondsF();
  DCHECK(deleted_buffers);

  // Doing this upfront simplifies decisions about range_for_next_append_ below.
  UpdateLastAppendStateForRemove(start, end, exclude_start);

  RangeList::iterator itr = ranges_.begin();
  while (itr != ranges_.end()) {
    SourceBufferRange* range = *itr;
    if (range->GetStartTimestamp() >= end)
      break;

    // Split off any remaining GOPs starting at or after |end| and add it to
    // |ranges_|.
    SourceBufferRange* new_range = range->SplitRange(end);
    if (new_range) {
      itr = ranges_.insert(++itr, new_range);

      // Update |range_for_next_append_| if it was previously |range| and should
      // be |new_range| now.
      if (range_for_next_append_ != ranges_.end() &&
          *range_for_next_append_ == range) {
        DecodeTimestamp potential_next_append_timestamp =
            PotentialNextAppendTimestamp();
        if (potential_next_append_timestamp != kNoDecodeTimestamp() &&
            new_range->BelongsToRange(potential_next_append_timestamp)) {
          range_for_next_append_ = itr;
        }
      }

      --itr;

      // Update the selected range if the next buffer position was transferred
      // to |new_range|.
      if (new_range->HasNextBufferPosition())
        SetSelectedRange(new_range);
    }

    // Truncate the current range so that it only contains data before
    // the removal range.
    BufferQueue saved_buffers;
    bool delete_range = range->TruncateAt(start, &saved_buffers, exclude_start);

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
        *range_for_next_append_ == range) {
      DecodeTimestamp potential_next_append_timestamp =
          PotentialNextAppendTimestamp();

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

  DVLOG(3) << __func__ << " " << GetStreamTypeName()
           << ": after remove ranges_=" << RangesToString(ranges_);

  DCHECK(IsRangeListSorted(ranges_));
  DCHECK(OnlySelectedRangeIsSeeked());
}

void SourceBufferStream::ResetSeekState() {
  SetSelectedRange(NULL);
  track_buffer_.clear();
  config_change_pending_ = false;
  last_output_buffer_timestamp_ = kNoDecodeTimestamp();
  just_exhausted_track_buffer_ = false;
  pending_buffer_ = NULL;
  pending_buffers_complete_ = false;
}

void SourceBufferStream::ResetLastAppendedState() {
  last_appended_buffer_timestamp_ = kNoDecodeTimestamp();
  last_appended_buffer_duration_ = kNoTimestamp;
  last_appended_buffer_is_keyframe_ = false;
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

bool SourceBufferStream::IsMonotonicallyIncreasing(const BufferQueue& buffers) {
  DCHECK(!buffers.empty());
  DecodeTimestamp prev_timestamp = last_appended_buffer_timestamp_;
  bool prev_is_keyframe = last_appended_buffer_is_keyframe_;
  for (BufferQueue::const_iterator itr = buffers.begin();
       itr != buffers.end(); ++itr) {
    DecodeTimestamp current_timestamp = (*itr)->GetDecodeTimestamp();
    bool current_is_keyframe = (*itr)->is_key_frame();
    DCHECK(current_timestamp != kNoDecodeTimestamp());
    DCHECK((*itr)->duration() >= base::TimeDelta())
        << "Packet with invalid duration."
        << " pts " << (*itr)->timestamp().InSecondsF()
        << " dts " << (*itr)->GetDecodeTimestamp().InSecondsF()
        << " dur " << (*itr)->duration().InSecondsF();

    if (prev_timestamp != kNoDecodeTimestamp()) {
      if (current_timestamp < prev_timestamp) {
        MEDIA_LOG(ERROR, media_log_)
            << "Buffers did not monotonically increase.";
        return false;
      }

      if (current_timestamp == prev_timestamp &&
          SourceBufferRange::IsUncommonSameTimestampSequence(
              prev_is_keyframe, current_is_keyframe)) {
        LIMITED_MEDIA_LOG(DEBUG, media_log_, num_strange_same_timestamps_logs_,
                          kMaxStrangeSameTimestampsLogs)
            << "Detected an append sequence with keyframe following a "
               "non-keyframe, both with the same decode timestamp of "
            << current_timestamp.InSecondsF();
      }
    }

    prev_timestamp = current_timestamp;
    prev_is_keyframe = current_is_keyframe;
  }
  return true;
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
      if (max_interbuffer_distance_ == kNoTimestamp) {
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

void SourceBufferStream::OnMemoryPressure(
    DecodeTimestamp media_time,
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level,
    bool force_instant_gc) {
  DVLOG(4) << __func__ << " level=" << memory_pressure_level;
  memory_pressure_level_ = memory_pressure_level;

  if (force_instant_gc)
    GarbageCollectIfNeeded(media_time, 0);
}

bool SourceBufferStream::GarbageCollectIfNeeded(DecodeTimestamp media_time,
                                                size_t newDataSize) {
  DCHECK(media_time != kNoDecodeTimestamp());
  // Garbage collection should only happen before/during appending new data,
  // which should not happen in end-of-stream state. Unless we also allow GC to
  // happen on memory pressure notifications, which might happen even in EOS
  // state.
  if (!base::FeatureList::IsEnabled(kMemoryPressureBasedSourceBufferGC))
    DCHECK(!end_of_stream_);
  // Compute size of |ranges_|.
  size_t ranges_size = GetBufferedSize();

  // Sanity and overflow checks
  if ((newDataSize > memory_limit_) ||
      (ranges_size + newDataSize < ranges_size)) {
    LIMITED_MEDIA_LOG(DEBUG, media_log_, num_garbage_collect_algorithm_logs_,
                      kMaxGarbageCollectAlgorithmWarningLogs)
        << GetStreamTypeName() << " stream: "
        << "new append of newDataSize=" << newDataSize
        << " bytes exceeds memory_limit_=" << memory_limit_
        << ", currently buffered ranges_size=" << ranges_size;
    return false;
  }

  size_t effective_memory_limit = memory_limit_;
  if (base::FeatureList::IsEnabled(kMemoryPressureBasedSourceBufferGC)) {
    switch (memory_pressure_level_) {
      case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE:
        effective_memory_limit = memory_limit_ / 2;
        break;
      case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL:
        effective_memory_limit = 0;
        break;
      case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE:
        break;
    }
  }

  // Return if we're under or at the memory limit.
  if (ranges_size + newDataSize <= effective_memory_limit)
    return true;

  size_t bytes_over_hard_memory_limit = 0;
  if (ranges_size + newDataSize > memory_limit_)
    bytes_over_hard_memory_limit = ranges_size + newDataSize - memory_limit_;

  size_t bytes_to_free = ranges_size + newDataSize - effective_memory_limit;

  DVLOG(2) << __func__ << " " << GetStreamTypeName()
           << ": Before GC media_time=" << media_time.InSecondsF()
           << " ranges_=" << RangesToString(ranges_)
           << " seek_pending_=" << seek_pending_
           << " ranges_size=" << ranges_size << " newDataSize=" << newDataSize
           << " memory_limit_=" << memory_limit_
           << " effective_memory_limit=" << effective_memory_limit
           << " last_appended_buffer_timestamp_="
           << last_appended_buffer_timestamp_.InSecondsF();

  if (selected_range_ && !seek_pending_ &&
      media_time > selected_range_->GetBufferedEndTimestamp()) {
    // Strictly speaking |media_time| (taken from HTMLMediaElement::currentTime)
    // should always be in the buffered ranges, but media::Pipeline uses audio
    // stream as the main time source, when audio is present.
    // In cases when audio and video streams have different buffered ranges, the
    // |media_time| value might be slightly outside of the video stream buffered
    // range. In those cases we need to clamp |media_time| value to the current
    // stream buffered ranges, to ensure the MSE garbage collection algorithm
    // works correctly (see crbug.com/563292 for details).
    DecodeTimestamp selected_buffered_end =
        selected_range_->GetBufferedEndTimestamp();

    DVLOG(2) << __func__ << " media_time " << media_time.InSecondsF()
             << " is outside of selected_range_=["
             << selected_range_->GetStartTimestamp().InSecondsF() << ";"
             << selected_buffered_end.InSecondsF()
             << "] clamping media_time to be "
             << selected_buffered_end.InSecondsF();
    media_time = selected_buffered_end;
  }

  size_t bytes_freed = 0;

  // If last appended buffer position was earlier than the current playback time
  // then try deleting data between last append and current media_time.
  if (last_appended_buffer_timestamp_ != kNoDecodeTimestamp() &&
      last_appended_buffer_duration_ != kNoTimestamp &&
      media_time >
          last_appended_buffer_timestamp_ + last_appended_buffer_duration_) {
    size_t between = FreeBuffersAfterLastAppended(bytes_to_free, media_time);
    DVLOG(3) << __func__ << " FreeBuffersAfterLastAppended "
             << " released " << between << " bytes"
             << " ranges_=" << RangesToString(ranges_);
    bytes_freed += between;

    // Some players start appending data at the new seek target position before
    // actually initiating the seek operation (i.e. they try to improve seek
    // performance by prebuffering some data at the seek target position and
    // initiating seek once enough data is pre-buffered. In those cases we'll
    // see that data is being appended at some new position, but there is no
    // pending seek reported yet. In this situation we need to try preserving
    // the most recently appended data, i.e. data belonging to the same buffered
    // range as the most recent append.
    if (range_for_next_append_ != ranges_.end()) {
      DCHECK((*range_for_next_append_)->GetStartTimestamp() <= media_time);
      media_time = (*range_for_next_append_)->GetStartTimestamp();
      DVLOG(3) << __func__ << " media_time adjusted to "
               << media_time.InSecondsF();
    }
  }

  // If there is an unsatisfied pending seek, we can safely remove all data that
  // is earlier than seek target, then remove from the back until we reach the
  // most recently appended GOP and then remove from the front if we still don't
  // have enough space for the upcoming append.
  if (bytes_freed < bytes_to_free && seek_pending_) {
    DCHECK(!ranges_.empty());
    // All data earlier than the seek target |media_time| can be removed safely
    size_t front = FreeBuffers(bytes_to_free - bytes_freed, media_time, false);
    DVLOG(3) << __func__ << " Removed " << front
             << " bytes from the front. ranges_=" << RangesToString(ranges_);
    bytes_freed += front;

    // If removing data earlier than |media_time| didn't free up enough space,
    // then try deleting from the back until we reach most recently appended GOP
    if (bytes_freed < bytes_to_free) {
      size_t back = FreeBuffers(bytes_to_free - bytes_freed, media_time, true);
      DVLOG(3) << __func__ << " Removed " << back
               << " bytes from the back. ranges_=" << RangesToString(ranges_);
      bytes_freed += back;
    }

    // If even that wasn't enough, then try greedily deleting from the front,
    // that should allow us to remove as much data as necessary to succeed.
    if (bytes_freed < bytes_to_free) {
      size_t front2 = FreeBuffers(bytes_to_free - bytes_freed,
                                  ranges_.back()->GetEndTimestamp(), false);
      DVLOG(3) << __func__ << " Removed " << front2
               << " bytes from the front. ranges_=" << RangesToString(ranges_);
      bytes_freed += front2;
    }
    DCHECK(bytes_freed >= bytes_to_free);
  }

  // Try removing data from the front of the SourceBuffer up to |media_time|
  // position.
  if (bytes_freed < bytes_to_free) {
    size_t front = FreeBuffers(bytes_to_free - bytes_freed, media_time, false);
    DVLOG(3) << __func__ << " Removed " << front
             << " bytes from the front. ranges_=" << RangesToString(ranges_);
    bytes_freed += front;
  }

  // Try removing data from the back of the SourceBuffer, until we reach the
  // most recent append position.
  if (bytes_freed < bytes_to_free) {
    size_t back = FreeBuffers(bytes_to_free - bytes_freed, media_time, true);
    DVLOG(3) << __func__ << " Removed " << back
             << " bytes from the back. ranges_=" << RangesToString(ranges_);
    bytes_freed += back;
  }

  DVLOG(2) << __func__ << " " << GetStreamTypeName()
           << ": After GC bytes_to_free=" << bytes_to_free
           << " bytes_freed=" << bytes_freed
           << " bytes_over_hard_memory_limit=" << bytes_over_hard_memory_limit
           << " ranges_=" << RangesToString(ranges_);

  return bytes_freed >= bytes_over_hard_memory_limit;
}

size_t SourceBufferStream::FreeBuffersAfterLastAppended(
    size_t total_bytes_to_free, DecodeTimestamp media_time) {
  DVLOG(4) << __func__ << " last_appended_buffer_timestamp_="
           << last_appended_buffer_timestamp_.InSecondsF()
           << " media_time=" << media_time.InSecondsF();

  DecodeTimestamp remove_range_start = last_appended_buffer_timestamp_;
  if (last_appended_buffer_is_keyframe_)
    remove_range_start += GetMaxInterbufferDistance();

  DecodeTimestamp remove_range_start_keyframe = FindKeyframeAfterTimestamp(
      remove_range_start);
  if (remove_range_start_keyframe != kNoDecodeTimestamp())
    remove_range_start = remove_range_start_keyframe;
  if (remove_range_start >= media_time)
    return 0;

  DecodeTimestamp remove_range_end;
  size_t bytes_freed = GetRemovalRange(remove_range_start,
                                       media_time,
                                       total_bytes_to_free,
                                       &remove_range_end);
  if (bytes_freed > 0) {
    DVLOG(4) << __func__ << " removing ["
             << remove_range_start.ToPresentationTime().InSecondsF() << ";"
             << remove_range_end.ToPresentationTime().InSecondsF() << "]";
    Remove(remove_range_start.ToPresentationTime(),
           remove_range_end.ToPresentationTime(),
           media_time.ToPresentationTime());
  }

  return bytes_freed;
}

size_t SourceBufferStream::GetRemovalRange(
    DecodeTimestamp start_timestamp, DecodeTimestamp end_timestamp,
    size_t total_bytes_to_free, DecodeTimestamp* removal_end_timestamp) {
  DCHECK(start_timestamp >= DecodeTimestamp()) << start_timestamp.InSecondsF();
  DCHECK(start_timestamp < end_timestamp)
      << "start " << start_timestamp.InSecondsF()
      << ", end " << end_timestamp.InSecondsF();

  size_t bytes_freed = 0;

  for (RangeList::iterator itr = ranges_.begin();
       itr != ranges_.end() && bytes_freed < total_bytes_to_free; ++itr) {
    SourceBufferRange* range = *itr;
    if (range->GetStartTimestamp() >= end_timestamp)
      break;
    if (range->GetEndTimestamp() < start_timestamp)
      continue;

    size_t bytes_to_free = total_bytes_to_free - bytes_freed;
    size_t bytes_removed = range->GetRemovalGOP(
        start_timestamp, end_timestamp, bytes_to_free, removal_end_timestamp);
    bytes_freed += bytes_removed;
  }
  return bytes_freed;
}

size_t SourceBufferStream::FreeBuffers(size_t total_bytes_to_free,
                                       DecodeTimestamp media_time,
                                       bool reverse_direction) {
  TRACE_EVENT2("media", "SourceBufferStream::FreeBuffers",
               "total bytes to free", total_bytes_to_free,
               "reverse direction", reverse_direction);

  DCHECK_GT(total_bytes_to_free, 0u);
  size_t bytes_freed = 0;

  // This range will save the last GOP appended to |range_for_next_append_|
  // if the buffers surrounding it get deleted during garbage collection.
  SourceBufferRange* new_range_for_append = NULL;

  while (!ranges_.empty() && bytes_freed < total_bytes_to_free) {
    SourceBufferRange* current_range = NULL;
    BufferQueue buffers;
    size_t bytes_deleted = 0;

    if (reverse_direction) {
      current_range = ranges_.back();
      DVLOG(5) << "current_range=" << RangeToString(*current_range);
      if (current_range->LastGOPContainsNextBufferPosition()) {
        DCHECK_EQ(current_range, selected_range_);
        DVLOG(5) << "current_range contains next read position, stopping GC";
        break;
      }
      DVLOG(5) << "Deleting GOP from back: " << RangeToString(*current_range);
      bytes_deleted = current_range->DeleteGOPFromBack(&buffers);
    } else {
      current_range = ranges_.front();
      DVLOG(5) << "current_range=" << RangeToString(*current_range);

      // FirstGOPEarlierThanMediaTime() is useful here especially if
      // |seek_pending_| (such that no range contains next buffer
      // position).
      // FirstGOPContainsNextBufferPosition() is useful here especially if
      // |!seek_pending_| to protect against DeleteGOPFromFront() if
      // FirstGOPEarlierThanMediaTime() was insufficient alone.
      if (!current_range->FirstGOPEarlierThanMediaTime(media_time) ||
          current_range->FirstGOPContainsNextBufferPosition()) {
        // We have removed all data up to the GOP that contains current playback
        // position, we can't delete any further.
        DVLOG(5) << "current_range contains playback position, stopping GC";
        break;
      }
      DVLOG(4) << "Deleting GOP from front: " << RangeToString(*current_range)
               << ", media_time: " << media_time.InMicroseconds()
               << ", current_range->HasNextBufferPosition(): "
               << current_range->HasNextBufferPosition();
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
      bytes_freed += bytes_deleted;
    }

    if (current_range->size_in_bytes() == 0) {
      DCHECK_NE(current_range, selected_range_);
      DCHECK(range_for_next_append_ == ranges_.end() ||
             *range_for_next_append_ != current_range);
      delete current_range;
      reverse_direction ? ranges_.pop_back() : ranges_.pop_front();
    }

    if (reverse_direction && new_range_for_append) {
      // We don't want to delete any further, or we'll be creating gaps
      break;
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

void SourceBufferStream::TrimSpliceOverlap(const BufferQueue& new_buffers) {
  DCHECK(!new_buffers.empty());
  DCHECK_EQ(kAudio, GetType());

  // Find the overlapped range (if any).
  const base::TimeDelta splice_timestamp = new_buffers.front()->timestamp();
  const DecodeTimestamp splice_dts =
      DecodeTimestamp::FromPresentationTime(splice_timestamp);
  RangeList::iterator range_itr = FindExistingRangeFor(splice_dts);
  if (range_itr == ranges_.end()) {
    DVLOG(3) << __func__ << " No splice trimming. No range overlap at time "
             << splice_timestamp.InMicroseconds();
    return;
  }

  // Search for overlapped buffer needs exclusive end value. Choosing smallest
  // possible value.
  const DecodeTimestamp end_dts =
      splice_dts + base::TimeDelta::FromInternalValue(1);

  // Find if new buffer's start would overlap an existing buffer.
  BufferQueue overlapped_buffers;
  if (!(*range_itr)
           ->GetBuffersInRange(splice_dts, end_dts, &overlapped_buffers)) {
    // Bail if no overlapped buffers found.
    DVLOG(3) << __func__ << " No splice trimming. No buffer overlap at time "
             << splice_timestamp.InMicroseconds();
    return;
  }

  // At most one buffer should exist containing the time of the newly appended
  // buffer's start. GetBuffersInRange does not currently return buffers with
  // zero duration.
  DCHECK_EQ(overlapped_buffers.size(), 1U)
      << __func__ << " Found more than one overlapped buffer";
  StreamParserBuffer* overlapped_buffer = overlapped_buffers.front().get();

  if (overlapped_buffer->timestamp() == splice_timestamp) {
    // Ignore buffers with the same start time. They will be completely removed
    // in PrepareRangesForNextAppend().
    DVLOG(3) << __func__ << " No splice trimming at time "
             << splice_timestamp.InMicroseconds()
             << ". Overlapped buffer will be completely removed.";
    return;
  }

  // Determine the duration of overlap.
  base::TimeDelta overlapped_end_time =
      overlapped_buffer->timestamp() + overlapped_buffer->duration();
  base::TimeDelta overlap_duration = overlapped_end_time - splice_timestamp;

  // At this point overlap should be non-empty (ruled out same-timestamp above).
  DCHECK_GT(overlap_duration, base::TimeDelta());

  // Don't trim for overlaps of less than one millisecond (which is frequently
  // the extent of timestamp resolution for poorly encoded media).
  if (overlap_duration < base::TimeDelta::FromMilliseconds(1)) {
    std::stringstream log_string;
    log_string << "Skipping audio splice trimming at PTS="
               << splice_timestamp.InMicroseconds() << "us. Found only "
               << overlap_duration.InMicroseconds()
               << "us of overlap, need at least 1000us. Multiple occurrences "
               << "may result in loss of A/V sync.";
    LIMITED_MEDIA_LOG(DEBUG, media_log_, num_splice_logs_, kMaxAudioSpliceLogs)
        << log_string.str();
    DVLOG(1) << __func__ << log_string.str();
    return;
  }

  // Trim overlap from the existing buffer.
  DecoderBuffer::DiscardPadding discard_padding =
      overlapped_buffer->discard_padding();
  discard_padding.second += overlap_duration;
  overlapped_buffer->set_discard_padding(discard_padding);
  overlapped_buffer->set_duration(overlapped_buffer->duration() -
                                  overlap_duration);

  std::stringstream log_string;
  log_string << "Audio buffer splice at PTS="
             << splice_timestamp.InMicroseconds()
             << "us. Trimmed tail of overlapped buffer (PTS="
             << overlapped_buffer->timestamp().InMicroseconds() << "us) by "
             << overlap_duration.InMicroseconds() << "us.";
  LIMITED_MEDIA_LOG(DEBUG, media_log_, num_splice_logs_, kMaxAudioSpliceLogs)
      << log_string.str();
  DVLOG(1) << __func__ << log_string.str();
}

void SourceBufferStream::PrepareRangesForNextAppend(
    const BufferQueue& new_buffers, BufferQueue* deleted_buffers) {
  DCHECK(deleted_buffers);

  if (GetType() == kAudio)
    TrimSpliceOverlap(new_buffers);

  base::TimeDelta prev_duration = last_appended_buffer_duration_;
  DecodeTimestamp prev_timestamp = last_appended_buffer_timestamp_;
  DecodeTimestamp next_timestamp = new_buffers.front()->GetDecodeTimestamp();

  // 1. Clean up the old buffers between the last appended buffer and the
  //    beginning of |new_buffers|.
  if (prev_timestamp != kNoDecodeTimestamp() &&
      prev_timestamp != next_timestamp) {
    RemoveInternal(prev_timestamp, next_timestamp, true, deleted_buffers);
  }

  // 2. Delete the buffers that |new_buffers| overlaps.
  if (new_coded_frame_group_) {
    // Extend the deletion range earlier to the coded frame group start time if
    // this is the first append in a new coded frame group.
    DCHECK(coded_frame_group_start_time_ != kNoDecodeTimestamp());
    next_timestamp = std::min(coded_frame_group_start_time_, next_timestamp);
  }

  // Exclude the start from removal to avoid deleting the last appended buffer
  // in cases where the timestamps match. Only do this when :
  //   A. Type is video. This may occur in cases of VP9 alt-ref frames or frames
  //      with incorrect timestamps. Removing a frame may break decode
  //      dependencies and there are no downsides to just keeping it (other than
  //      some throw-away decoder work).
  //   B. Type is text. TODO(chcunningham): Implement text splicing. See
  //      http://crbug.com/661408
  //   C. Type is audio and overlapped duration is 0. We've encountered Vorbis
  //      streams containing zero-duration buffers (i.e. no real overlap). For
  //      non-zero duration removing overlapped frames is important to preserve
  //      A/V sync (see AudioClock).
  const bool exclude_start = prev_timestamp == next_timestamp &&
                             (GetType() == kVideo || GetType() == kText ||
                              prev_duration == base::TimeDelta());

  // Set end time for remove to include the duration of last buffer. If the
  // duration is estimated, use 1 microsecond instead to ensure frames are not
  // accidentally removed due to over-estimation.
  DecodeTimestamp end = new_buffers.back()->GetDecodeTimestamp();
  base::TimeDelta duration = new_buffers.back()->duration();
  if (duration != kNoTimestamp && duration > base::TimeDelta() &&
      !new_buffers.back()->is_duration_estimated()) {
    end += duration;
  } else {
    // TODO(chcunningham): Emit warning when 0ms durations are not expected.
    // http://crbug.com/312836
    end += base::TimeDelta::FromInternalValue(1);
  }

  // Finally do the deletion of overlap.
  RemoveInternal(next_timestamp, end, exclude_start, deleted_buffers);
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
  DVLOG(3) << __func__ << " " << GetStreamTypeName()
           << " Removed all buffers with DTS >= " << timestamp.InSecondsF()
           << ". New track buffer size:" << track_buffer_.size();
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
  DVLOG(3) << __func__ << " " << GetStreamTypeName() << " merging "
           << RangeToString(*range_with_new_buffers) << " into "
           << RangeToString(**next_range_itr);
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
  DVLOG(1) << __func__ << " " << GetStreamTypeName() << " ("
           << timestamp.InSecondsF() << ")";
  ResetSeekState();

  seek_buffer_timestamp_ = timestamp;
  seek_pending_ = true;

  if (ShouldSeekToStartOfBuffered(timestamp)) {
    ranges_.front()->SeekToStart();
    SetSelectedRange(ranges_.front());
    seek_pending_ = false;
    return;
  }

  DecodeTimestamp seek_dts = DecodeTimestamp::FromPresentationTime(timestamp);

  RangeList::iterator itr = ranges_.end();
  for (itr = ranges_.begin(); itr != ranges_.end(); ++itr) {
    if ((*itr)->CanSeekTo(seek_dts))
      break;
  }

  if (itr == ranges_.end())
    return;

  if (!audio_configs_.empty()) {
    const auto& config = audio_configs_[(*itr)->GetConfigIdAtTime(seek_dts)];
    if (config.codec() == kCodecOpus) {
      DecodeTimestamp preroll_dts = std::max(seek_dts - config.seek_preroll(),
                                             (*itr)->GetStartTimestamp());
      if ((*itr)->CanSeekTo(preroll_dts) &&
          (*itr)->SameConfigThruRange(preroll_dts, seek_dts)) {
        seek_dts = preroll_dts;
      }
    }
  }

  SeekAndSetSelectedRange(*itr, seek_dts);
  seek_pending_ = false;
}

bool SourceBufferStream::IsSeekPending() const {
  return seek_pending_ && !IsEndOfStreamReached();
}

void SourceBufferStream::OnSetDuration(base::TimeDelta duration) {
  DVLOG(1) << __func__ << " " << GetStreamTypeName() << " ("
           << duration.InSecondsF() << ")";
  DCHECK(!end_of_stream_);

  if (ranges_.empty())
    return;

  DecodeTimestamp start = DecodeTimestamp::FromPresentationTime(duration);
  DecodeTimestamp end = ranges_.back()->GetBufferedEndTimestamp();

  // Trim the end if it exceeds the new duration.
  if (start < end) {
    BufferQueue deleted_buffers;
    RemoveInternal(start, end, false, &deleted_buffers);

    if (!deleted_buffers.empty()) {
      // Truncation removed current position. Clear selected range.
      SetSelectedRange(NULL);
    }
  }
}

SourceBufferStream::Status SourceBufferStream::GetNextBuffer(
    scoped_refptr<StreamParserBuffer>* out_buffer) {
  DVLOG(2) << __func__ << " " << GetStreamTypeName();
  if (!pending_buffer_.get()) {
    const SourceBufferStream::Status status = GetNextBufferInternal(out_buffer);
    if (status != SourceBufferStream::kSuccess ||
        !SetPendingBuffer(out_buffer)) {
      DVLOG(2) << __func__ << " " << GetStreamTypeName()
               << ": no pending buffer, returning status " << status;
      return status;
    }
  }

  DCHECK(pending_buffer_->preroll_buffer().get());

  const SourceBufferStream::Status status =
      HandleNextBufferWithPreroll(out_buffer);
  DVLOG(2) << __func__ << " " << GetStreamTypeName()
           << ": handled next buffer with preroll, returning status " << status;
  return status;
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
    if (next_buffer->GetConfigId() != current_config_index_) {
      config_change_pending_ = true;
      DVLOG(1) << "Config change (track buffer config ID does not match).";
      return kConfigChange;
    }

    DVLOG(3) << __func__ << " Next buffer coming from track_buffer_";
    *out_buffer = next_buffer;
    track_buffer_.pop_front();
    WarnIfTrackBufferExhaustionSkipsForward(*out_buffer);
    last_output_buffer_timestamp_ = (*out_buffer)->GetDecodeTimestamp();

    // If the track buffer becomes empty, then try to set the selected range
    // based on the timestamp of this buffer being returned.
    if (track_buffer_.empty()) {
      just_exhausted_track_buffer_ = true;
      SetSelectedRangeIfNeeded(last_output_buffer_timestamp_);
    }

    return kSuccess;
  }

  DCHECK(track_buffer_.empty());
  if (!selected_range_ || !selected_range_->HasNextBuffer()) {
    if (IsEndOfStreamReached()) {
      return kEndOfStream;
    }
    DVLOG(3) << __func__ << " " << GetStreamTypeName()
             << ": returning kNeedBuffer "
             << (selected_range_ ? "(selected range has no next buffer)"
                                 : "(no selected range)");
    return kNeedBuffer;
  }

  if (selected_range_->GetNextConfigId() != current_config_index_) {
    config_change_pending_ = true;
    DVLOG(1) << "Config change (selected range config ID does not match).";
    return kConfigChange;
  }

  CHECK(selected_range_->GetNextBuffer(out_buffer));
  WarnIfTrackBufferExhaustionSkipsForward(*out_buffer);
  last_output_buffer_timestamp_ = (*out_buffer)->GetDecodeTimestamp();
  return kSuccess;
}

void SourceBufferStream::WarnIfTrackBufferExhaustionSkipsForward(
    const scoped_refptr<StreamParserBuffer>& next_buffer) {
  if (!just_exhausted_track_buffer_)
    return;

  just_exhausted_track_buffer_ = false;
  DCHECK(next_buffer->is_key_frame());
  DecodeTimestamp next_output_buffer_timestamp =
      next_buffer->GetDecodeTimestamp();
  base::TimeDelta delta =
      next_output_buffer_timestamp - last_output_buffer_timestamp_;
  DCHECK_GE(delta, base::TimeDelta());
  if (delta > GetMaxInterbufferDistance()) {
    LIMITED_MEDIA_LOG(DEBUG, media_log_, num_track_buffer_gap_warning_logs_,
                      kMaxTrackBufferGapWarningLogs)
        << "Media append that overlapped current playback position caused time "
           "gap in playing "
        << GetStreamTypeName() << " stream because the next keyframe is "
        << delta.InMilliseconds() << "ms beyond last overlapped frame. Media "
                                     "may appear temporarily frozen.";
  }
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
  DVLOG(1) << __func__ << " " << GetStreamTypeName() << ": " << selected_range_
           << " " << (selected_range_ ? RangeToString(*selected_range_) : "")
           << " -> " << range << " " << (range ? RangeToString(*range) : "");
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

base::TimeDelta SourceBufferStream::GetHighestPresentationTimestamp() const {
  if (ranges_.empty())
    return base::TimeDelta();

  // TODO(wolenetz): Report actual highest PTS here, not DTS cast to PTS. See
  // https://crbug.com/398130.
  return ranges_.back()->GetEndTimestamp().ToPresentationTime();
}

base::TimeDelta SourceBufferStream::GetBufferedDuration() const {
  if (ranges_.empty())
    return base::TimeDelta();

  return ranges_.back()->GetBufferedEndTimestamp().ToPresentationTime();
}

size_t SourceBufferStream::GetBufferedSize() const {
  size_t ranges_size = 0;
  for (auto* range : ranges_)
    ranges_size += range->size_in_bytes();
  return ranges_size;
}

void SourceBufferStream::MarkEndOfStream() {
  DCHECK(!end_of_stream_);
  end_of_stream_ = true;
}

void SourceBufferStream::UnmarkEndOfStream() {
  DCHECK(end_of_stream_);
  end_of_stream_ = false;
}

bool SourceBufferStream::IsEndOfStreamReached() const {
  if (!end_of_stream_ || !track_buffer_.empty())
    return false;

  if (ranges_.empty())
    return true;

  if (seek_pending_) {
    base::TimeDelta last_range_end_time =
        ranges_.back()->GetBufferedEndTimestamp().ToPresentationTime();
    return seek_buffer_timestamp_ >= last_range_end_time;
  }

  if (!selected_range_)
    return true;

  return selected_range_ == ranges_.back();
}

const AudioDecoderConfig& SourceBufferStream::GetCurrentAudioDecoderConfig() {
  if (config_change_pending_)
    CompleteConfigChange();
  // Trying to track down crash. http://crbug.com/715761
  CHECK(current_config_index_ >= 0 &&
        static_cast<size_t>(current_config_index_) < audio_configs_.size());
  return audio_configs_[current_config_index_];
}

const VideoDecoderConfig& SourceBufferStream::GetCurrentVideoDecoderConfig() {
  if (config_change_pending_)
    CompleteConfigChange();
  // Trying to track down crash. http://crbug.com/715761
  CHECK(current_config_index_ >= 0 &&
        static_cast<size_t>(current_config_index_) < video_configs_.size());
  return video_configs_[current_config_index_];
}

const TextTrackConfig& SourceBufferStream::GetCurrentTextTrackConfig() {
  return text_track_config_;
}

base::TimeDelta SourceBufferStream::GetMaxInterbufferDistance() const {
  if (max_interbuffer_distance_ == kNoTimestamp)
    return base::TimeDelta::FromMilliseconds(kDefaultBufferDurationInMs);
  return max_interbuffer_distance_;
}

bool SourceBufferStream::UpdateAudioConfig(const AudioDecoderConfig& config) {
  DCHECK(!audio_configs_.empty());
  DCHECK(video_configs_.empty());
  DVLOG(3) << "UpdateAudioConfig.";

  if (audio_configs_[0].codec() != config.codec()) {
    MEDIA_LOG(ERROR, media_log_) << "Audio codec changes not allowed.";
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
    MEDIA_LOG(ERROR, media_log_) << "Video codec changes not allowed.";
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

  if (!track_buffer_.empty()) {
    current_config_index_ = track_buffer_.front()->GetConfigId();
    return;
  }

  if (selected_range_ && selected_range_->HasNextBuffer())
    current_config_index_ = selected_range_->GetNextConfigId();
}

void SourceBufferStream::SetSelectedRangeIfNeeded(
    const DecodeTimestamp timestamp) {
  DVLOG(2) << __func__ << " " << GetStreamTypeName() << "("
           << timestamp.InSecondsF() << ")";

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
    if (last_output_buffer_timestamp_ == kNoDecodeTimestamp()) {
      DVLOG(2) << __func__ << " " << GetStreamTypeName()
               << " no previous output timestamp";
      return;
    }

    start_timestamp = last_output_buffer_timestamp_ +
        base::TimeDelta::FromInternalValue(1);
  }

  DecodeTimestamp seek_timestamp =
      FindNewSelectedRangeSeekTimestamp(start_timestamp);

  // If we don't have buffered data to seek to, then return.
  if (seek_timestamp == kNoDecodeTimestamp()) {
    DVLOG(2) << __func__ << " " << GetStreamTypeName()
             << " couldn't find new selected range seek timestamp";
    return;
  }

  DCHECK(track_buffer_.empty());
  SeekAndSetSelectedRange(*FindExistingRangeFor(seek_timestamp),
                          seek_timestamp);
}

DecodeTimestamp SourceBufferStream::FindNewSelectedRangeSeekTimestamp(
    const DecodeTimestamp start_timestamp) {
  DCHECK(start_timestamp != kNoDecodeTimestamp());
  DCHECK(start_timestamp >= DecodeTimestamp());

  RangeList::iterator itr = ranges_.begin();

  // When checking a range to see if it has or begins soon enough after
  // |start_timestamp|, use the fudge room to determine "soon enough".
  DecodeTimestamp start_timestamp_plus_fudge =
      start_timestamp + ComputeFudgeRoom(GetMaxInterbufferDistance());

  // Multiple ranges could be within the fudge room, because the fudge room is
  // dynamic based on max inter-buffer distance seen so far. Optimistically
  // check the earliest ones first.
  for (; itr != ranges_.end(); ++itr) {
    DecodeTimestamp range_start = (*itr)->GetStartTimestamp();
    if (range_start >= start_timestamp_plus_fudge)
      break;
    if ((*itr)->GetEndTimestamp() < start_timestamp)
      continue;
    DecodeTimestamp search_timestamp = start_timestamp;
    if (start_timestamp < range_start &&
        start_timestamp_plus_fudge >= range_start) {
      search_timestamp = range_start;
    }
    DecodeTimestamp keyframe_timestamp =
        (*itr)->NextKeyframeTimestamp(search_timestamp);
    if (keyframe_timestamp != kNoDecodeTimestamp())
      return keyframe_timestamp;
  }

  DVLOG(2) << __func__ << " " << GetStreamTypeName()
           << " no buffered data for dts=" << start_timestamp.InSecondsF();
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
  DVLOG(1) << __func__;

  DCHECK(*itr != ranges_.end());
  if (**itr == selected_range_) {
    DVLOG(1) << __func__ << " deleting selected range.";
    SetSelectedRange(NULL);
  }

  if (*itr == range_for_next_append_) {
    DVLOG(1) << __func__ << " deleting range_for_next_append_.";
    range_for_next_append_ = ranges_.end();
    ResetLastAppendedState();
  }

  delete **itr;
  *itr = ranges_.erase(*itr);
}

bool SourceBufferStream::SetPendingBuffer(
    scoped_refptr<StreamParserBuffer>* out_buffer) {
  DCHECK(out_buffer->get());
  DCHECK(!pending_buffer_.get());

  const bool have_preroll_buffer = !!(*out_buffer)->preroll_buffer().get();

  if (!have_preroll_buffer)
    return false;

  pending_buffer_.swap(*out_buffer);
  pending_buffers_complete_ = false;
  return true;
}

}  // namespace media
