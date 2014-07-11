// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mp2t/es_adapter_video.h"

#include "media/base/buffers.h"
#include "media/base/stream_parser_buffer.h"
#include "media/base/video_decoder_config.h"
#include "media/formats/mp2t/mp2t_common.h"

namespace media {
namespace mp2t {

// Arbitrary decision about the frame duration when there is no previous
// hint about what could be the frame duration.
static const int kDefaultFrameDurationMs = 40;

// To calculate the frame duration, we make an assumption
// that the timestamp of the next frame in presentation order
// is no further than 5 frames away in decode order.
// TODO(damienv): the previous assumption should cover most of the practical
// cases. However, the right way to calculate the frame duration would be
// to emulate the H264 dpb bumping process.
static const size_t kHistorySize = 5;

EsAdapterVideo::EsAdapterVideo(
    const NewVideoConfigCB& new_video_config_cb,
    const EmitBufferCB& emit_buffer_cb)
    : new_video_config_cb_(new_video_config_cb),
      emit_buffer_cb_(emit_buffer_cb),
      has_valid_config_(false),
      has_valid_frame_(false),
      last_frame_duration_(
          base::TimeDelta::FromMilliseconds(kDefaultFrameDurationMs)),
      buffer_index_(0) {
}

EsAdapterVideo::~EsAdapterVideo() {
}

void EsAdapterVideo::Flush() {
  ProcessPendingBuffers(true);
}

void EsAdapterVideo::Reset() {
  has_valid_config_ = false;
  has_valid_frame_ = false;

  last_frame_duration_ =
      base::TimeDelta::FromMilliseconds(kDefaultFrameDurationMs);

  config_list_.clear();
  buffer_index_ = 0;
  buffer_list_.clear();
  emitted_pts_.clear();

  discarded_frames_min_pts_ = base::TimeDelta();
  discarded_frames_dts_.clear();
}

void EsAdapterVideo::OnConfigChanged(
    const VideoDecoderConfig& video_decoder_config) {
  config_list_.push_back(
      ConfigEntry(buffer_index_ + buffer_list_.size(), video_decoder_config));
  has_valid_config_ = true;
  ProcessPendingBuffers(false);
}

void EsAdapterVideo::OnNewBuffer(
    const scoped_refptr<StreamParserBuffer>& stream_parser_buffer) {
  // Discard the incoming frame:
  // - if it is not associated with any config,
  // - or if only non-key frames have been added to a new segment.
  if (!has_valid_config_ ||
      (!has_valid_frame_ && !stream_parser_buffer->IsKeyframe())) {
    if (discarded_frames_dts_.empty() ||
        discarded_frames_min_pts_ > stream_parser_buffer->timestamp()) {
      discarded_frames_min_pts_ = stream_parser_buffer->timestamp();
    }
    discarded_frames_dts_.push_back(
        stream_parser_buffer->GetDecodeTimestamp());
    return;
  }

  has_valid_frame_ = true;

  if (!discarded_frames_dts_.empty())
    ReplaceDiscardedFrames(stream_parser_buffer);

  buffer_list_.push_back(stream_parser_buffer);
  ProcessPendingBuffers(false);
}

void EsAdapterVideo::ProcessPendingBuffers(bool flush) {
  DCHECK(has_valid_config_);

  while (!buffer_list_.empty() &&
         (flush || buffer_list_.size() > kHistorySize)) {
    // Signal a config change, just before emitting the corresponding frame.
    if (!config_list_.empty() && config_list_.front().first == buffer_index_) {
      new_video_config_cb_.Run(config_list_.front().second);
      config_list_.pop_front();
    }

    scoped_refptr<StreamParserBuffer> buffer = buffer_list_.front();
    buffer_list_.pop_front();
    buffer_index_++;

    if (buffer->duration() == kNoTimestamp()) {
      base::TimeDelta next_frame_pts = GetNextFramePts(buffer->timestamp());
      if (next_frame_pts == kNoTimestamp()) {
        // This can happen when emitting the very last buffer
        // or if the stream do not meet the assumption behind |kHistorySize|.
        DVLOG(LOG_LEVEL_ES) << "Using last frame duration: "
                            << last_frame_duration_.InMilliseconds();
        buffer->set_duration(last_frame_duration_);
      } else {
        base::TimeDelta duration = next_frame_pts - buffer->timestamp();
        DVLOG(LOG_LEVEL_ES) << "Frame duration: " << duration.InMilliseconds();
        buffer->set_duration(duration);
      }
    }

    emitted_pts_.push_back(buffer->timestamp());
    if (emitted_pts_.size() > kHistorySize)
      emitted_pts_.pop_front();

    last_frame_duration_ = buffer->duration();
    emit_buffer_cb_.Run(buffer);
  }
}

base::TimeDelta EsAdapterVideo::GetNextFramePts(base::TimeDelta current_pts) {
  base::TimeDelta next_pts = kNoTimestamp();

  // Consider the timestamps of future frames (in decode order).
  // Note: the next frame is not enough when the GOP includes some B frames.
  for (BufferQueue::const_iterator it = buffer_list_.begin();
       it != buffer_list_.end(); ++it) {
    if ((*it)->timestamp() < current_pts)
      continue;
    if (next_pts == kNoTimestamp() || next_pts > (*it)->timestamp())
      next_pts = (*it)->timestamp();
  }

  // Consider the timestamps of previous frames (in decode order).
  // In a simple GOP structure with B frames, the frame next to the last B
  // frame (in presentation order) is located before in decode order.
  for (std::list<base::TimeDelta>::const_iterator it = emitted_pts_.begin();
       it != emitted_pts_.end(); ++it) {
    if (*it < current_pts)
      continue;
    if (next_pts == kNoTimestamp() || next_pts > *it)
      next_pts = *it;
  }

  return next_pts;
}

void EsAdapterVideo::ReplaceDiscardedFrames(
    const scoped_refptr<StreamParserBuffer>& stream_parser_buffer) {
  DCHECK(!discarded_frames_dts_.empty());
  DCHECK(stream_parser_buffer->IsKeyframe());

  // PTS is interpolated between the min PTS of discarded frames
  // and the PTS of the first valid buffer.
  base::TimeDelta pts = discarded_frames_min_pts_;
  base::TimeDelta pts_delta =
      (stream_parser_buffer->timestamp() - pts) / discarded_frames_dts_.size();

  while (!discarded_frames_dts_.empty()) {
    scoped_refptr<StreamParserBuffer> frame =
        StreamParserBuffer::CopyFrom(
            stream_parser_buffer->data(),
            stream_parser_buffer->data_size(),
            stream_parser_buffer->IsKeyframe(),
            stream_parser_buffer->type(),
            stream_parser_buffer->track_id());
    frame->SetDecodeTimestamp(discarded_frames_dts_.front());
    frame->set_timestamp(pts);
    frame->set_duration(pts_delta);
    buffer_list_.push_back(frame);
    pts += pts_delta;
    discarded_frames_dts_.pop_front();
  }
}

}  // namespace mp2t
}  // namespace media
