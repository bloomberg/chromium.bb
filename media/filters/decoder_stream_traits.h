// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_DECODER_STREAM_TRAITS_H_
#define MEDIA_FILTERS_DECODER_STREAM_TRAITS_H_

#include "media/base/demuxer_stream.h"

namespace media {

class AudioDecoder;
class DecryptingAudioDecoder;
class DecryptingVideoDecoder;
class VideoDecoder;

template <DemuxerStream::Type StreamType>
struct DecoderStreamTraits {};

template <>
struct DecoderStreamTraits<DemuxerStream::AUDIO> {
  typedef AudioDecoder DecoderType;
  typedef DecryptingAudioDecoder DecryptingDecoderType;
};

template <>
struct DecoderStreamTraits<DemuxerStream::VIDEO> {
  typedef VideoDecoder DecoderType;
  typedef DecryptingVideoDecoder DecryptingDecoderType;
};

}  // namespace media

#endif  // MEDIA_FILTERS_DECODER_STREAM_TRAITS_H_
