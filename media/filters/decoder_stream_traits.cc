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

// static
DecoderStreamTraits<DemuxerStream::AUDIO>::DecoderConfigType
DecoderStreamTraits<DemuxerStream::AUDIO>::GetDecoderConfig(
    DemuxerStream* stream) {
  return stream->audio_decoder_config();
}

DecoderStreamTraits<DemuxerStream::AUDIO>::DecoderStreamTraits(
    const scoped_refptr<MediaLog>& media_log)
    : media_log_(media_log) {}

void DecoderStreamTraits<DemuxerStream::AUDIO>::ReportStatistics(
    const StatisticsCB& statistics_cb,
    int bytes_decoded) {
  PipelineStatistics statistics;
  statistics.audio_bytes_decoded = bytes_decoded;
  statistics_cb.Run(statistics);
}

void DecoderStreamTraits<DemuxerStream::AUDIO>::InitializeDecoder(
    DecoderType* decoder,
    const DecoderConfigType& config,
    bool /* low_delay */,
    CdmContext* cdm_context,
    const InitCB& init_cb,
    const OutputCB& output_cb) {
  DCHECK(config.IsValidConfig());
  decoder->Initialize(config, cdm_context, init_cb, output_cb);
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
    const scoped_refptr<DecoderBuffer>& buffer) {
  audio_ts_validator_->CheckForTimestampGap(buffer);
}

void DecoderStreamTraits<DemuxerStream::AUDIO>::OnDecodeDone(
    const scoped_refptr<OutputType>& buffer) {
  audio_ts_validator_->RecordOutputDuration(buffer);
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

// static
DecoderStreamTraits<DemuxerStream::VIDEO>::DecoderConfigType
DecoderStreamTraits<DemuxerStream::VIDEO>::GetDecoderConfig(
    DemuxerStream* stream) {
  return stream->video_decoder_config();
}

DecoderStreamTraits<DemuxerStream::VIDEO>::DecoderStreamTraits(
    const scoped_refptr<MediaLog>& media_log)
    // Randomly selected number of samples to keep.
    : keyframe_distance_average_(16) {}

void DecoderStreamTraits<DemuxerStream::VIDEO>::ReportStatistics(
    const StatisticsCB& statistics_cb,
    int bytes_decoded) {
  PipelineStatistics statistics;
  statistics.video_bytes_decoded = bytes_decoded;

  if (keyframe_distance_average_.count()) {
    statistics.video_keyframe_distance_average =
        keyframe_distance_average_.Average();
  } else {
    // Before we have enough keyframes to calculate the average distance, we
    // will assume the average keyframe distance is infinitely large.
    statistics.video_keyframe_distance_average = base::TimeDelta::Max();
  }

  statistics_cb.Run(statistics);
}

void DecoderStreamTraits<DemuxerStream::VIDEO>::InitializeDecoder(
    DecoderType* decoder,
    const DecoderConfigType& config,
    bool low_delay,
    CdmContext* cdm_context,
    const InitCB& init_cb,
    const OutputCB& output_cb) {
  DCHECK(config.IsValidConfig());
  decoder->Initialize(config, low_delay, cdm_context, init_cb, output_cb);
}

void DecoderStreamTraits<DemuxerStream::VIDEO>::OnStreamReset(
    DemuxerStream* stream) {
  DCHECK(stream);
  last_keyframe_timestamp_ = base::TimeDelta();
}

void DecoderStreamTraits<DemuxerStream::VIDEO>::OnDecode(
    const scoped_refptr<DecoderBuffer>& buffer) {
  if (!buffer)
    return;

  if (buffer->end_of_stream()) {
    last_keyframe_timestamp_ = base::TimeDelta();
    return;
  }

  if (!buffer->is_key_frame())
    return;

  base::TimeDelta current_frame_timestamp = buffer->timestamp();
  if (last_keyframe_timestamp_.is_zero()) {
    last_keyframe_timestamp_ = current_frame_timestamp;
    return;
  }

  base::TimeDelta frame_distance =
      current_frame_timestamp - last_keyframe_timestamp_;
  UMA_HISTOGRAM_MEDIUM_TIMES("Media.Video.KeyFrameDistance", frame_distance);
  last_keyframe_timestamp_ = current_frame_timestamp;
  keyframe_distance_average_.AddSample(frame_distance);
}

}  // namespace media
