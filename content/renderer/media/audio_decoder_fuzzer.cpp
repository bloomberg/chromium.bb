// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/audio_decoder.h"
#include "media/base/media.h"
#include "third_party/WebKit/public/platform/WebAudioBus.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // This is needed to suppress noisy log messages from ffmpeg.
  media::InitializeMediaLibrary();

  if (size > 8 * 1024) {
    // Larger inputs are likely to trigger timeouts and OOMswhich would not be
    // considered as valid bugs.
    return 0;
  }

  blink::WebAudioBus web_audio_bus;
  bool success = content::DecodeAudioFileData(
      &web_audio_bus, reinterpret_cast<const char*>(data), size);

  if (!success)
    return 0;

  return 0;
}
