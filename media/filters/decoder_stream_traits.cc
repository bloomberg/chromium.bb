// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/decoder_stream_traits.h"

#include "base/logging.h"
#include "media/base/video_decoder.h"

namespace media {

bool DecoderStreamTraits<DemuxerStream::VIDEO>::FinishInitialization(
    const StreamInitCB& init_cb,
    DecoderType* decoder,
    DemuxerStream* stream) {
  DCHECK(stream);
  if (!decoder) {
    init_cb.Run(false, false);
    return false;
  }
  if (decoder->NeedsBitstreamConversion())
    stream->EnableBitstreamConverter();
  // TODO(xhwang): We assume |decoder_->HasAlpha()| does not change after
  // reinitialization. Check this condition.
  init_cb.Run(true, decoder->HasAlpha());
  return true;
}

void DecoderStreamTraits<DemuxerStream::VIDEO>::ReportStatistics(
    const StatisticsCB& statistics_cb,
    int bytes_decoded) {
  PipelineStatistics statistics;
  statistics.video_bytes_decoded = bytes_decoded;
  statistics_cb.Run(statistics);
}

}  // namespace media
