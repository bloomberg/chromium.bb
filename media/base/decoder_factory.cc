// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/decoder_factory.h"

namespace media {

DecoderFactory::DecoderFactory() {}

DecoderFactory::~DecoderFactory() {}

void DecoderFactory::CreateAudioDecoders(
    ScopedVector<AudioDecoder>* audio_decoders) {}

void DecoderFactory::CreateVideoDecoders(
    ScopedVector<VideoDecoder>* video_decoders) {}

}  // namespace media
