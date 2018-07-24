// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/assistant_mojo_media_client.h"

#include "base/single_thread_task_runner.h"
#include "media/base/audio_decoder.h"
#include "media/filters/ffmpeg_audio_decoder.h"

namespace media {

AssistantMojoMediaClient::AssistantMojoMediaClient() = default;

AssistantMojoMediaClient::~AssistantMojoMediaClient() = default;

std::unique_ptr<AudioDecoder> AssistantMojoMediaClient::CreateAudioDecoder(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    MediaLog* media_log) {
  return std::make_unique<FFmpegAudioDecoder>(std::move(task_runner),
                                              media_log);
}

}  // namespace media
