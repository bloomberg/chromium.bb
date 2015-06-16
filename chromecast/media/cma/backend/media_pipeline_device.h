// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_PIPELINE_DEVICE_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_PIPELINE_DEVICE_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"

namespace chromecast {
namespace media {
class AudioPipelineDevice;
class MediaClockDevice;
class MediaPipelineDeviceFactory;
class MediaPipelineDeviceParams;
class VideoPipelineDevice;

// MediaPipelineDevice is the owner of the underlying audio/video/clock
// devices.  It ensures that the clock outlives the audio and video devices, so
// it is safe for them to reference the clock with a raw pointer.
class MediaPipelineDevice {
 public:
  // Constructs object using clock, audio, video created by given factory.
  explicit MediaPipelineDevice(scoped_ptr<MediaPipelineDeviceFactory> factory);

  // Constructs explicitly from separate clock, audio, video elements
  MediaPipelineDevice(scoped_ptr<MediaClockDevice> media_clock_device,
                      scoped_ptr<AudioPipelineDevice> audio_pipeline_device,
                      scoped_ptr<VideoPipelineDevice> video_pipeline_device);
  ~MediaPipelineDevice();

  AudioPipelineDevice* GetAudioPipelineDevice() const {
    return audio_pipeline_device_.get();
  }
  VideoPipelineDevice* GetVideoPipelineDevice() const {
    return video_pipeline_device_.get();
  }
  MediaClockDevice* GetMediaClockDevice() const {
    return media_clock_device_.get();
  }

 private:
  scoped_ptr<MediaClockDevice> media_clock_device_;
  scoped_ptr<AudioPipelineDevice> audio_pipeline_device_;
  scoped_ptr<VideoPipelineDevice> video_pipeline_device_;

  DISALLOW_COPY_AND_ASSIGN(MediaPipelineDevice);
};

// Factory method to create a MediaPipelineDevice.
typedef base::Callback<scoped_ptr<MediaPipelineDevice>(
    const MediaPipelineDeviceParams&)> CreatePipelineDeviceCB;

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_PIPELINE_DEVICE_H_
