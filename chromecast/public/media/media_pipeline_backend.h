// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_PUBLIC_MEDIA_MEDIA_PIPELINE_BACKEND_H_
#define CHROMECAST_PUBLIC_MEDIA_MEDIA_PIPELINE_BACKEND_H_

namespace chromecast {
namespace media {

class AudioPipelineDevice;
class MediaClockDevice;
struct MediaPipelineDeviceParams;
class VideoPipelineDevice;

// Interface for creating and managing ownership of platform-specific clock,
// audio and video devices.  cast_shell owns the MediaPipelineBackend for
// as long as it is needed; the implementation is responsible for
// tearing down the individual components correctly when it is destroyed.
// A new MediaPipelineBackend will be instantiated for each media player
// instance.
class MediaPipelineBackend {
 public:
  virtual ~MediaPipelineBackend() {}

  // Returns the platform-specific pipeline clock.
  virtual MediaClockDevice* GetClock() = 0;

  // Returns the platform-specific audio backend.
  virtual AudioPipelineDevice* GetAudio() = 0;

  // Returns the platform-specific video backend.
  virtual VideoPipelineDevice* GetVideo() = 0;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_PIPELINE_DEVICE_FACTORY_H_
