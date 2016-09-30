// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/cast_audio_mixer.h"

#include "base/logging.h"
#include "chromecast/media/audio/cast_audio_manager.h"
#include "chromecast/media/audio/cast_audio_output_stream.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/channel_layout.h"

namespace {
const int kBitsPerSample = 16;
const int kFramesPerBuffer = 1024;
const int kSampleRate = 48000;
}  // namespace

namespace chromecast {
namespace media {

class CastAudioMixer::MixerProxyStream
    : public ::media::AudioOutputStream,
      private ::media::AudioConverter::InputCallback {
 public:
  MixerProxyStream(const ::media::AudioParameters& input_params,
                   const ::media::AudioParameters& output_params,
                   CastAudioManager* audio_manager,
                   CastAudioMixer* audio_mixer)
      : audio_manager_(audio_manager),
        audio_mixer_(audio_mixer),
        source_callback_(nullptr),
        input_params_(input_params),
        output_params_(output_params),
        opened_(false),
        volume_(1.0) {}

  ~MixerProxyStream() override {}

  void OnError() {
    DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());

    if (source_callback_)
      source_callback_->OnError(this);
  }

 private:
  class ResamplerProxy : public ::media::AudioConverter::InputCallback {
   public:
    ResamplerProxy(::media::AudioConverter::InputCallback* input_callback,
                   const ::media::AudioParameters& input_params,
                   const ::media::AudioParameters& output_params) {
      resampler_.reset(
          new ::media::AudioConverter(input_params, output_params, false));
      resampler_->AddInput(input_callback);
    }

    ~ResamplerProxy() override {}

   private:
    // ::media::AudioConverter::InputCallback implementation
    double ProvideInput(::media::AudioBus* audio_bus,
                        uint32_t frames_delayed) override {
      resampler_->ConvertWithDelay(frames_delayed, audio_bus);

      // Volume multiplier has already been applied by |resampler_|.
      return 1.0;
    }

    std::unique_ptr<::media::AudioConverter> resampler_;

    DISALLOW_COPY_AND_ASSIGN(ResamplerProxy);
  };

  // ::media::AudioOutputStream implementation
  bool Open() override {
    DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());

    ::media::AudioParameters::Format format = input_params_.format();
    DCHECK((format == ::media::AudioParameters::AUDIO_PCM_LINEAR) ||
           (format == ::media::AudioParameters::AUDIO_PCM_LOW_LATENCY));

    ::media::ChannelLayout channel_layout = input_params_.channel_layout();
    if ((channel_layout != ::media::CHANNEL_LAYOUT_MONO) &&
        (channel_layout != ::media::CHANNEL_LAYOUT_STEREO)) {
      LOG(WARNING) << "Unsupported channel layout: " << channel_layout;
      return false;
    }
    DCHECK_GE(input_params_.channels(), 1);
    DCHECK_LE(input_params_.channels(), 2);

    return opened_ = audio_mixer_->Register(this);
  }

  void Close() override {
    DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());

    if (proxy_)
      Stop();
    if (opened_)
      audio_mixer_->Unregister(this);
    // Signal to the manager that we're closed and can be removed.
    // This should be the last call in the function as it deletes "this".
    audio_manager_->ReleaseOutputStream(this);
  }

  void Start(AudioSourceCallback* source_callback) override {
    DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
    DCHECK(source_callback);

    if (!opened_ || proxy_)
      return;
    source_callback_ = source_callback;
    proxy_.reset(new ResamplerProxy(this, input_params_, output_params_));
    audio_mixer_->AddInput(proxy_.get());
  }

  void Stop() override {
    DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());

    if (!proxy_)
      return;
    audio_mixer_->RemoveInput(proxy_.get());
    proxy_.reset();
    source_callback_ = nullptr;
  }

  void SetVolume(double volume) override {
    DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());

    volume_ = volume;
  }

  void GetVolume(double* volume) override {
    DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());

    *volume = volume_;
  }

  // ::media::AudioConverter::InputCallback implementation
  double ProvideInput(::media::AudioBus* audio_bus,
                      uint32_t frames_delayed) override {
    DCHECK(source_callback_);

    const base::TimeDelta delay = ::media::AudioTimestampHelper::FramesToTime(
        frames_delayed, input_params_.sample_rate());
    source_callback_->OnMoreData(delay, base::TimeTicks::Now(), 0, audio_bus);
    return volume_;
  }

  CastAudioManager* const audio_manager_;
  CastAudioMixer* const audio_mixer_;

  std::unique_ptr<ResamplerProxy> proxy_;
  AudioSourceCallback* source_callback_;
  const ::media::AudioParameters input_params_;
  const ::media::AudioParameters output_params_;
  bool opened_;
  double volume_;

  DISALLOW_COPY_AND_ASSIGN(MixerProxyStream);
};

CastAudioMixer::CastAudioMixer(const RealStreamFactory& real_stream_factory)
    : error_(false),
      real_stream_factory_(real_stream_factory),
      output_stream_(nullptr) {
  output_params_ = ::media::AudioParameters(
      ::media::AudioParameters::Format::AUDIO_PCM_LOW_LATENCY,
      ::media::CHANNEL_LAYOUT_STEREO, kSampleRate, kBitsPerSample,
      kFramesPerBuffer);
  mixer_.reset(
      new ::media::AudioConverter(output_params_, output_params_, false));
  thread_checker_.DetachFromThread();
}

CastAudioMixer::~CastAudioMixer() {}

::media::AudioOutputStream* CastAudioMixer::MakeStream(
    const ::media::AudioParameters& params,
    CastAudioManager* audio_manager) {
  return new MixerProxyStream(params, output_params_, audio_manager, this);
}

bool CastAudioMixer::Register(MixerProxyStream* proxy_stream) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(std::find(proxy_streams_.begin(), proxy_streams_.end(),
                   proxy_stream) == proxy_streams_.end());

  // If the mixer is in an error state, do not allow any new
  // registrations until every active stream has been
  // unregistered.
  if (error_)
    return false;

  // Initialize a backend instance if there are no output streams.
  // The stream will fail to register if the CastAudioOutputStream
  // is not opened properly.
  if (proxy_streams_.empty()) {
    DCHECK(!output_stream_);
    output_stream_ = real_stream_factory_.Run(output_params_);
    if (!output_stream_->Open()) {
      output_stream_->Close();
      output_stream_ = nullptr;
      return false;
    }
  }

  proxy_streams_.push_back(proxy_stream);
  return true;
}

void CastAudioMixer::Unregister(MixerProxyStream* proxy_stream) {
  DCHECK(thread_checker_.CalledOnValidThread());
  auto it =
      std::find(proxy_streams_.begin(), proxy_streams_.end(), proxy_stream);
  DCHECK(it != proxy_streams_.end());

  proxy_streams_.erase(it);

  // Reset the state once all streams have been unregistered.
  if (proxy_streams_.empty()) {
    DCHECK(mixer_->empty());
    if (output_stream_)
      output_stream_->Close();
    output_stream_ = nullptr;
    error_ = false;
  }
}

void CastAudioMixer::AddInput(
    ::media::AudioConverter::InputCallback* input_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Start the backend if there are no current inputs.
  if (mixer_->empty() && output_stream_)
    output_stream_->Start(this);
  mixer_->AddInput(input_callback);
}

void CastAudioMixer::RemoveInput(
    ::media::AudioConverter::InputCallback* input_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  mixer_->RemoveInput(input_callback);
  // Stop |output_stream_| if there are no inputs and the stream is running.
  if (mixer_->empty() && output_stream_)
    output_stream_->Stop();
}

int CastAudioMixer::OnMoreData(base::TimeDelta delay,
                               base::TimeTicks /* delay_timestamp */,
                               int /* prior_frames_skipped */,
                               ::media::AudioBus* dest) {
  DCHECK(thread_checker_.CalledOnValidThread());

  uint32_t frames_delayed = ::media::AudioTimestampHelper::TimeToFrames(
      delay, output_params_.sample_rate());
  mixer_->ConvertWithDelay(frames_delayed, dest);
  return dest->frames();
}

void CastAudioMixer::OnError(::media::AudioOutputStream* stream) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(output_stream_ == stream);

  // TODO(ameyak): Add rate limiting. If errors are seen to occur
  //               above some arbitrary value in a specified amount
  //               of time, signal errors to all currently active
  //               streams and close.

  output_stream_->Stop();
  output_stream_->Close();
  output_stream_ = real_stream_factory_.Run(output_params_);

  if (output_stream_->Open()) {
    output_stream_->Start(this);
  } else {
    // Assume error state
    output_stream_->Close();
    output_stream_ = nullptr;
    error_ = true;
    for (auto it = proxy_streams_.begin(); it != proxy_streams_.end(); it++)
      (*it)->OnError();
  }
}

}  // namespace media
}  // namespace chromecast
