// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/decoder_factory.h"

#include "base/single_thread_task_runner.h"

namespace media {

DecoderFactory::DecoderFactory() {}

DecoderFactory::~DecoderFactory() {}

void DecoderFactory::CreateAudioDecoders(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    std::vector<std::unique_ptr<AudioDecoder>>* audio_decoders) {}

void DecoderFactory::CreateVideoDecoders(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    GpuVideoAcceleratorFactories* gpu_factories,
    std::vector<std::unique_ptr<VideoDecoder>>* video_decoders) {}

}  // namespace media
