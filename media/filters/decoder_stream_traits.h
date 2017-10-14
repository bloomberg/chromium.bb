// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_DECODER_STREAM_TRAITS_H_
#define MEDIA_FILTERS_DECODER_STREAM_TRAITS_H_

#include "base/containers/flat_set.h"
#include "base/time/time.h"
#include "media/base/cdm_context.h"
#include "media/base/demuxer_stream.h"
#include "media/base/moving_average.h"
#include "media/base/pipeline_status.h"
#include "media/base/video_decoder_config.h"
#include "media/filters/audio_timestamp_validator.h"

namespace media {

class AudioBuffer;
class AudioDecoder;
class AudioDecoderConfig;
class CdmContext;
class DecryptingAudioDecoder;
class DecryptingVideoDecoder;
class DemuxerStream;
class VideoDecoder;
class VideoDecoderConfig;
class VideoFrame;

template <DemuxerStream::Type StreamType>
class DecoderStreamTraits {};

enum class PostDecodeAction { DELIVER, DROP };

template <>
class MEDIA_EXPORT DecoderStreamTraits<DemuxerStream::AUDIO> {
 public:
  typedef AudioBuffer OutputType;
  typedef AudioDecoder DecoderType;
  typedef AudioDecoderConfig DecoderConfigType;
  typedef DecryptingAudioDecoder DecryptingDecoderType;
  typedef base::Callback<void(bool success)> InitCB;
  typedef base::Callback<void(const scoped_refptr<OutputType>&)> OutputCB;

  static std::string ToString();
  static bool NeedsBitstreamConversion(DecoderType* decoder);
  static scoped_refptr<OutputType> CreateEOSOutput();
  static DecoderConfigType GetDecoderConfig(DemuxerStream* stream);

  explicit DecoderStreamTraits(MediaLog* media_log);

  void ReportStatistics(const StatisticsCB& statistics_cb, int bytes_decoded);
  void InitializeDecoder(DecoderType* decoder,
                         const DecoderConfigType& config,
                         bool low_delay,
                         CdmContext* cdm_context,
                         const InitCB& init_cb,
                         const OutputCB& output_cb);
  void OnDecode(const scoped_refptr<DecoderBuffer>& buffer);
  PostDecodeAction OnDecodeDone(const scoped_refptr<OutputType>& buffer);
  void OnStreamReset(DemuxerStream* stream);
  void OnConfigChanged(const DecoderConfigType& config);

 private:
  // Validates encoded timestamps match decoded output duration. MEDIA_LOG warns
  // if timestamp gaps are detected. Sufficiently large gaps can lead to AV sync
  // drift.
  std::unique_ptr<AudioTimestampValidator> audio_ts_validator_;

  MediaLog* media_log_;
};

template <>
class MEDIA_EXPORT DecoderStreamTraits<DemuxerStream::VIDEO> {
 public:
  typedef VideoFrame OutputType;
  typedef VideoDecoder DecoderType;
  typedef VideoDecoderConfig DecoderConfigType;
  typedef DecryptingVideoDecoder DecryptingDecoderType;
  typedef base::Callback<void(bool success)> InitCB;
  typedef base::Callback<void(const scoped_refptr<OutputType>&)> OutputCB;

  static std::string ToString();
  static bool NeedsBitstreamConversion(DecoderType* decoder);
  static scoped_refptr<OutputType> CreateEOSOutput();
  static DecoderConfigType GetDecoderConfig(DemuxerStream* stream);

  explicit DecoderStreamTraits(MediaLog* media_log);

  void ReportStatistics(const StatisticsCB& statistics_cb, int bytes_decoded);
  void InitializeDecoder(DecoderType* decoder,
                         const DecoderConfigType& config,
                         bool low_delay,
                         CdmContext* cdm_context,
                         const InitCB& init_cb,
                         const OutputCB& output_cb);
  void OnDecode(const scoped_refptr<DecoderBuffer>& buffer);

  PostDecodeAction OnDecodeDone(const scoped_refptr<OutputType>& buffer);
  void OnStreamReset(DemuxerStream* stream);
  void OnConfigChanged(const DecoderConfigType& config) {}

 private:
  base::TimeDelta last_keyframe_timestamp_;
  MovingAverage keyframe_distance_average_;
  base::flat_set<base::TimeDelta> frames_to_drop_;
};

}  // namespace media

#endif  // MEDIA_FILTERS_DECODER_STREAM_TRAITS_H_
