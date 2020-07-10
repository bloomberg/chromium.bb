// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/audio_loopback_stream_creator.h"

#include "content/browser/media/in_process_audio_loopback_stream_creator.h"

namespace content {

// static
std::unique_ptr<AudioLoopbackStreamCreator>
AudioLoopbackStreamCreator::CreateInProcessAudioLoopbackStreamCreator() {
  return std::make_unique<InProcessAudioLoopbackStreamCreator>();
}

AudioLoopbackStreamCreator::~AudioLoopbackStreamCreator() = default;

}  // namespace content
