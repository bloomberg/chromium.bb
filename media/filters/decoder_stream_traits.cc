// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/decoder_stream_traits.h"

#include "base/logging.h"
#include "media/base/audio_decoder.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"

namespace media {

std::string DecoderStreamTraits<DemuxerStream::AUDIO>::ToString() {
  return "Audio";
}

bool DecoderStreamTraits<DemuxerStream::AUDIO>::FinishInitialization(
    const StreamInitCB& init_cb,
    DecoderType* decoder,
    DemuxerStream* stream) {
  DCHECK(stream);
  if (!decoder) {
    init_cb.Run(false);
    return false;
  }
  init_cb.Run(true);
  return true;
}

void DecoderStreamTraits<DemuxerStream::AUDIO>::ReportStatistics(
    const StatisticsCB& statistics_cb,
    int bytes_decoded) {
  PipelineStatistics statistics;
  statistics.audio_bytes_decoded = bytes_decoded;
  statistics_cb.Run(statistics);
}

DecoderStreamTraits<DemuxerStream::AUDIO>::DecoderConfigType
    DecoderStreamTraits<DemuxerStream::AUDIO>::GetDecoderConfig(
        DemuxerStream& stream) {
  return stream.audio_decoder_config();
}

std::string DecoderStreamTraits<DemuxerStream::VIDEO>::ToString() {
  return "Video";
}

bool DecoderStreamTraits<DemuxerStream::VIDEO>::FinishInitialization(
    const StreamInitCB& init_cb,
    DecoderType* decoder,
    DemuxerStream* stream) {
  DCHECK(stream);
  if (!decoder) {
    init_cb.Run(false);
    return false;
  }
  if (decoder->NeedsBitstreamConversion())
    stream->EnableBitstreamConverter();
  init_cb.Run(true);
  return true;
}

void DecoderStreamTraits<DemuxerStream::VIDEO>::ReportStatistics(
    const StatisticsCB& statistics_cb,
    int bytes_decoded) {
  PipelineStatistics statistics;
  statistics.video_bytes_decoded = bytes_decoded;
  statistics_cb.Run(statistics);
}

DecoderStreamTraits<DemuxerStream::VIDEO>::DecoderConfigType
    DecoderStreamTraits<DemuxerStream::VIDEO>::GetDecoderConfig(
        DemuxerStream& stream) {
  return stream.video_decoder_config();
}

}  // namespace media
