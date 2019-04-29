// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/decoder_stream_traits.h"

#include <limits>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_decoder.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/video_decoder.h"
#include "media/base/video_frame.h"

namespace media {

// Audio decoder stream traits implementation.

// static
std::string DecoderStreamTraits<DemuxerStream::AUDIO>::ToString() {
  return "audio";
}

// static
bool DecoderStreamTraits<DemuxerStream::AUDIO>::NeedsBitstreamConversion(
    DecoderType* decoder) {
  return decoder->NeedsBitstreamConversion();
}

// static
scoped_refptr<DecoderStreamTraits<DemuxerStream::AUDIO>::OutputType>
DecoderStreamTraits<DemuxerStream::AUDIO>::CreateEOSOutput() {
  return OutputType::CreateEOSBuffer();
}

DecoderStreamTraits<DemuxerStream::AUDIO>::DecoderStreamTraits(
    MediaLog* media_log,
    ChannelLayout initial_hw_layout)
    : media_log_(media_log), initial_hw_layout_(initial_hw_layout) {}

DecoderStreamTraits<DemuxerStream::AUDIO>::DecoderConfigType
DecoderStreamTraits<DemuxerStream::AUDIO>::GetDecoderConfig(
    DemuxerStream* stream) {
  auto config = stream->audio_decoder_config();
  // Demuxer is not aware of hw layout, so we set it here.
  config.set_target_output_channel_layout(initial_hw_layout_);
  return config;
}

void DecoderStreamTraits<DemuxerStream::AUDIO>::ReportStatistics(
    const StatisticsCB& statistics_cb,
    int bytes_decoded) {
  stats_.audio_bytes_decoded = bytes_decoded;
  statistics_cb.Run(stats_);
}

void DecoderStreamTraits<DemuxerStream::AUDIO>::InitializeDecoder(
    DecoderType* decoder,
    const DecoderConfigType& config,
    bool /* low_delay */,
    CdmContext* cdm_context,
    const InitCB& init_cb,
    const OutputCB& output_cb,
    const WaitingCB& waiting_cb) {
  DCHECK(config.IsValidConfig());

  if (config_.IsValidConfig() && !config_.Matches(config))
    OnConfigChanged(config);
  config_ = config;

  stats_.audio_decoder_name = decoder->GetDisplayName();
  decoder->Initialize(config, cdm_context, init_cb, output_cb, waiting_cb);
}

void DecoderStreamTraits<DemuxerStream::AUDIO>::OnStreamReset(
    DemuxerStream* stream) {
  DCHECK(stream);
  // Stream is likely being seeked to a new timestamp, so make new validator to
  // build new timestamp expectations.
  audio_ts_validator_.reset(
      new AudioTimestampValidator(stream->audio_decoder_config(), media_log_));
}

void DecoderStreamTraits<DemuxerStream::AUDIO>::OnDecode(
    const DecoderBuffer& buffer) {
  audio_ts_validator_->CheckForTimestampGap(buffer);
}

PostDecodeAction DecoderStreamTraits<DemuxerStream::AUDIO>::OnDecodeDone(
    OutputType* buffer) {
  audio_ts_validator_->RecordOutputDuration(*buffer);
  return PostDecodeAction::DELIVER;
}

void DecoderStreamTraits<DemuxerStream::AUDIO>::OnConfigChanged(
    const DecoderConfigType& config) {
  // Reset validator with the latest config. Also ensures that we do not attempt
  // to match timestamps across config boundaries.
  audio_ts_validator_.reset(new AudioTimestampValidator(config, media_log_));
}

// Video decoder stream traits implementation.

// static
std::string DecoderStreamTraits<DemuxerStream::VIDEO>::ToString() {
  return "video";
}

// static
bool DecoderStreamTraits<DemuxerStream::VIDEO>::NeedsBitstreamConversion(
    DecoderType* decoder) {
  return decoder->NeedsBitstreamConversion();
}

// static
scoped_refptr<DecoderStreamTraits<DemuxerStream::VIDEO>::OutputType>
DecoderStreamTraits<DemuxerStream::VIDEO>::CreateEOSOutput() {
  return OutputType::CreateEOSFrame();
}

DecoderStreamTraits<DemuxerStream::VIDEO>::DecoderStreamTraits(
    MediaLog* media_log)
    // Randomly selected number of samples to keep.
    : keyframe_distance_average_(16) {}

DecoderStreamTraits<DemuxerStream::VIDEO>::DecoderConfigType
DecoderStreamTraits<DemuxerStream::VIDEO>::GetDecoderConfig(
    DemuxerStream* stream) {
  return stream->video_decoder_config();
}

void DecoderStreamTraits<DemuxerStream::VIDEO>::ReportStatistics(
    const StatisticsCB& statistics_cb,
    int bytes_decoded) {
  stats_.video_bytes_decoded = bytes_decoded;

  if (keyframe_distance_average_.count()) {
    stats_.video_keyframe_distance_average =
        keyframe_distance_average_.Average();
  } else {
    // Before we have enough keyframes to calculate the average distance, we
    // will assume the average keyframe distance is infinitely large.
    stats_.video_keyframe_distance_average = base::TimeDelta::Max();
  }

  statistics_cb.Run(stats_);
}

void DecoderStreamTraits<DemuxerStream::VIDEO>::InitializeDecoder(
    DecoderType* decoder,
    const DecoderConfigType& config,
    bool low_delay,
    CdmContext* cdm_context,
    const InitCB& init_cb,
    const OutputCB& output_cb,
    const WaitingCB& waiting_cb) {
  DCHECK(config.IsValidConfig());
  stats_.video_decoder_name = decoder->GetDisplayName();
  DVLOG(2) << stats_.video_decoder_name;
  decoder->Initialize(config, low_delay, cdm_context, init_cb, output_cb,
                      waiting_cb);
}

void DecoderStreamTraits<DemuxerStream::VIDEO>::OnStreamReset(
    DemuxerStream* stream) {
  DCHECK(stream);
  last_keyframe_timestamp_ = base::TimeDelta();
  frame_metadata_.clear();
}

void DecoderStreamTraits<DemuxerStream::VIDEO>::OnDecode(
    const DecoderBuffer& buffer) {
  if (buffer.end_of_stream()) {
    last_keyframe_timestamp_ = base::TimeDelta();
    return;
  }

  frame_metadata_[buffer.timestamp()] = {
      buffer.discard_padding().first == kInfiniteDuration,  // should_drop
      buffer.duration(),                                    // duration
  };

  if (!buffer.is_key_frame())
    return;

  base::TimeDelta current_frame_timestamp = buffer.timestamp();
  if (last_keyframe_timestamp_.is_zero()) {
    last_keyframe_timestamp_ = current_frame_timestamp;
    return;
  }

  const base::TimeDelta frame_distance =
      current_frame_timestamp - last_keyframe_timestamp_;
  last_keyframe_timestamp_ = current_frame_timestamp;
  keyframe_distance_average_.AddSample(frame_distance);
}

PostDecodeAction DecoderStreamTraits<DemuxerStream::VIDEO>::OnDecodeDone(
    OutputType* buffer) {
  // Add a timestamp here (after decoding completed) to enable buffering delay
  // measurements down the line.
  buffer->metadata()->SetTimeTicks(media::VideoFrameMetadata::DECODE_TIME,
                                   base::TimeTicks::Now());

  auto it = frame_metadata_.find(buffer->timestamp());

  // If the frame isn't in |frame_metadata_| it probably was erased below on a
  // previous cycle. We could drop these, but today our video algorithm will put
  // them back into sorted order or drop the frame if a later frame has already
  // been rendered.
  if (it == frame_metadata_.end())
    return PostDecodeAction::DELIVER;

  auto action = it->second.should_drop ? PostDecodeAction::DROP
                                       : PostDecodeAction::DELIVER;

  // Provide duration information to help the rendering algorithm on the very
  // first and very last frames.
  if (it->second.duration != kNoTimestamp) {
    buffer->metadata()->SetTimeDelta(VideoFrameMetadata::FRAME_DURATION,
                                     it->second.duration);
  }

  // We erase from the beginning onward to our target frame since frames should
  // be returned in presentation order. It's possible to accumulate entries in
  // this queue if playback begins at a non-keyframe; those frames may never be
  // returned from the decoder.
  frame_metadata_.erase(frame_metadata_.begin(), it + 1);
  return action;
}

}  // namespace media
