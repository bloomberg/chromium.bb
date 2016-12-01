// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_MIXER_H_
#define CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_MIXER_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "media/audio/audio_io.h"
#include "media/base/audio_converter.h"
#include "media/base/audio_parameters.h"

namespace chromecast {
namespace media {

class CastAudioManager;

// CastAudioMixer mixes multiple AudioOutputStreams and passes the mixed
// stream down to a single AudioOutputStream to be rendered by the CMA backend.
class CastAudioMixer : public ::media::AudioOutputStream::AudioSourceCallback {
 public:
  using RealStreamFactory = base::Callback<::media::AudioOutputStream*(
      const ::media::AudioParameters&)>;

  explicit CastAudioMixer(const RealStreamFactory& real_stream_factory);
  ~CastAudioMixer() override;

  virtual ::media::AudioOutputStream* MakeStream(
      const ::media::AudioParameters& params,
      CastAudioManager* audio_manager);

 private:
  class MixerProxyStream;

  // ::media::AudioOutputStream::AudioSourceCallback implementation
  int OnMoreData(base::TimeDelta delay,
                 base::TimeTicks delay_timestamp,
                 int prior_frames_skipped,
                 ::media::AudioBus* dest) override;

  void OnError(::media::AudioOutputStream* stream) override;

  // MixedAudioOutputStreams call Register on opening and AddInput on starting.
  bool Register(MixerProxyStream* proxy_stream);
  void Unregister(MixerProxyStream* proxy_stream);
  void AddInput(::media::AudioConverter::InputCallback* input_callback);
  void RemoveInput(::media::AudioConverter::InputCallback* input_callback);

  base::ThreadChecker thread_checker_;
  std::unique_ptr<::media::AudioConverter> mixer_;
  bool error_;

  const RealStreamFactory real_stream_factory_;
  ::media::AudioOutputStream* output_stream_;
  ::media::AudioParameters output_params_;

  std::vector<MixerProxyStream*> proxy_streams_;

  DISALLOW_COPY_AND_ASSIGN(CastAudioMixer);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_MIXER_H_
