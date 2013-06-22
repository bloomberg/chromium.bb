// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_ANDROID_AUDIO_DECODER_ANDROID_H_
#define CONTENT_RENDERER_MEDIA_ANDROID_AUDIO_DECODER_ANDROID_H_

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/file_descriptor_posix.h"
#include "base/shared_memory.h"

namespace WebKit {
class WebAudioBus;
}

namespace content {

typedef base::Callback<
    void(base::SharedMemoryHandle, base::FileDescriptor, size_t)>
    WebAudioMediaCodecRunner;

bool DecodeAudioFileData(WebKit::WebAudioBus* destination_bus,
                         const char* data,
                         size_t data_size,
                         double sample_rate,
                         const WebAudioMediaCodecRunner& runner);

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_ANDROID_AUDIO_DECODER_ANDROID_H_
