// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_OUTPUT_STREAM_H_
#define CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_OUTPUT_STREAM_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromecast/public/media/media_component_device.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_parameters.h"

namespace media {
class FakeAudioWorker;
}  // namespace media

namespace chromecast {
namespace media {

class CastAudioManager;
class MediaPipelineBackend;

class CastAudioOutputStream : public ::media::AudioOutputStream {
 public:
  CastAudioOutputStream(const ::media::AudioParameters& audio_params,
                        CastAudioManager* audio_manager);
  ~CastAudioOutputStream() override;

  // ::media::AudioOutputStream implementation.
  bool Open() override;
  void Close() override;
  void Start(AudioSourceCallback* source_callback) override;
  void Stop() override;
  void SetVolume(double volume) override;
  void GetVolume(double* volume) override;

 private:
  // Task to push frame into audio pipeline device.
  void PushFrame(AudioSourceCallback* source_callback);
  void OnPushFrameStatus(AudioSourceCallback* source_callback,
                         MediaComponentDevice::FrameStatus status);
  void OnPushFrameError(AudioSourceCallback* source_callback);

  ::media::AudioParameters audio_params_;
  CastAudioManager* audio_manager_;

  double volume_;
  bool audio_device_busy_;
  scoped_ptr<::media::FakeAudioWorker> audio_worker_;
  scoped_ptr<::media::AudioBus> audio_bus_;
  scoped_ptr<MediaPipelineBackend> media_pipeline_backend_;

  base::WeakPtrFactory<CastAudioOutputStream> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(CastAudioOutputStream);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_OUTPUT_STREAM_H_
