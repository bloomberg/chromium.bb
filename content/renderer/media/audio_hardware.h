// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef CONTENT_RENDERER_MEDIA_AUDIO_HARDWARE_H_
#define CONTENT_RENDERER_MEDIA_AUDIO_HARDWARE_H_
#pragma once

#include "base/basictypes.h"
#include "content/common/content_export.h"

// Contains static methods to query hardware properties from the browser
// process. Values are cached to avoid unnecessary round trips, but the cache
// can be cleared if needed (currently only used by tests).
namespace audio_hardware {

// Fetch the sample rate of the default audio output end point device.
// Must be called from RenderThreadImpl::current().
CONTENT_EXPORT double GetOutputSampleRate();

// Fetch the sample rate of the default audio input end point device.
// Must be called from RenderThreadImpl::current().
CONTENT_EXPORT double GetInputSampleRate();

// Fetch the buffer size we use for the default output device.
// Must be called from RenderThreadImpl::current().
CONTENT_EXPORT size_t GetOutputBufferSize();

// Forces the next call to any of the Get functions to query the hardware
// and repopulate the cache.
CONTENT_EXPORT void ResetCache();
}  // namespace audio_hardware

#endif  // CONTENT_RENDERER_MEDIA_AUDIO_HARDWARE_H_
