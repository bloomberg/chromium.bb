// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef CONTENT_RENDERER_MEDIA_AUDIO_HARDWARE_H_
#define CONTENT_RENDERER_MEDIA_AUDIO_HARDWARE_H_

#include "base/basictypes.h"
#include "content/common/content_export.h"
#include "media/base/channel_layout.h"

// Contains static methods to query hardware properties from the browser
// process. Values are cached to avoid unnecessary round trips, but the cache
// can be cleared if needed (currently only used by tests).
namespace content {

class CONTENT_EXPORT AudioHardware {
public:
  // Fetch the sample rate of the default audio output end point device.
  // Must be called from RenderThreadImpl::current().
  static int GetOutputSampleRate();

  // Fetch the sample rate of the default audio input end point device.
  // Must be called from RenderThreadImpl::current().
  static int GetInputSampleRate();

  // Fetch the buffer size we use for the default output device.
  // Must be called from RenderThreadImpl::current().
  // Must be used in conjunction with AUDIO_PCM_LOW_LATENCY.
  static size_t GetOutputBufferSize();

  // Fetch the audio channel layout for the default input device.
  // Must be called from RenderThreadImpl::current().
  static media::ChannelLayout GetInputChannelLayout();

  // Forces the next call to any of the Get functions to query the hardware
  // and repopulate the cache.
  static void ResetCache();
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_AUDIO_HARDWARE_H_
