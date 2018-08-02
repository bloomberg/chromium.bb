// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/platform/audio_input_provider_impl.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "libassistant/shared/public/platform_audio_buffer.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_sample_types.h"
#include "media/base/channel_layout.h"
#include "services/audio/public/cpp/device_factory.h"
#include "services/service_manager/public/cpp/connector.h"

namespace chromeos {
namespace assistant {

namespace {

// This format should match //c/b/c/assistant/platform_audio_input_host.cc.
constexpr assistant_client::BufferFormat kFormat{
    16000 /* sample_rate */, assistant_client::INTERLEAVED_S32, 1 /* channels */
};

class DefaultHotwordStateManager : public AudioInputImpl::HotwordStateManager {
 public:
  DefaultHotwordStateManager() = default;
  ~DefaultHotwordStateManager() override = default;

  void OnConversationTurnStarted() override {}
  void OnConversationTurnFinished() override {}
  void OnCaptureDataArrived() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultHotwordStateManager);
};

class DspHotwordStateManager : public AudioInputImpl::HotwordStateManager {
 public:
  DspHotwordStateManager(scoped_refptr<base::SequencedTaskRunner> task_runner,
                         AudioInputImpl* input)
      : task_runner_(task_runner), input_(input) {
    second_phase_timer_.SetTaskRunner(task_runner_);
  }

  // HotwordStateManager overrides:
  void OnConversationTurnStarted() override {
    if (second_phase_timer_.IsRunning()) {
      DCHECK(stream_state_ == StreamState::HOTWORD);
      second_phase_timer_.Stop();
      stream_state_ = StreamState::NORMAL;
    } else {
      // Handles user click on mic button.
      input_->RecreateAudioInputStream(false /* hotword */);
    }
  }

  void OnConversationTurnFinished() override {
    input_->RecreateAudioInputStream(true /* hotword */);
    stream_state_ = StreamState::HOTWORD;
  }

  void OnCaptureDataArrived() override {
    if (stream_state_ == StreamState::HOTWORD &&
        !second_phase_timer_.IsRunning()) {
      // 1s from now, if OnConversationTurnStarted is not called, we assume that
      // libassistant has rejected the hotword supplied by DSP. Thus, we reset
      // and reopen the device on hotword state.
      second_phase_timer_.Start(
          FROM_HERE, base::TimeDelta::FromSeconds(1),
          base::BindRepeating(
              &DspHotwordStateManager::OnConversationTurnFinished,
              base::Unretained(this)));
    }
  }

 private:
  enum class StreamState {
    HOTWORD,
    NORMAL,
  };

  StreamState stream_state_ = StreamState::HOTWORD;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::OneShotTimer second_phase_timer_;
  AudioInputImpl* input_;

  DISALLOW_COPY_AND_ASSIGN(DspHotwordStateManager);
};

}  // namespace

AudioInputBufferImpl::AudioInputBufferImpl(const void* data,
                                           uint32_t frame_count)
    : data_(data), frame_count_(frame_count) {}

AudioInputBufferImpl::~AudioInputBufferImpl() = default;

assistant_client::BufferFormat AudioInputBufferImpl::GetFormat() const {
  return kFormat;
}

const void* AudioInputBufferImpl::GetData() const {
  return data_;
}

void* AudioInputBufferImpl::GetWritableData() {
  NOTREACHED();
  return nullptr;
}

int AudioInputBufferImpl::GetFrameCount() const {
  return frame_count_;
}

AudioInputImpl::AudioInputImpl(service_manager::Connector* connector,
                               bool default_on)
    : default_on_(default_on),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      weak_factory_(this) {
  DETACH_FROM_SEQUENCE(observer_sequence_checker_);

  if (IsDspHotwordEnabled()) {
    state_manager_ =
        std::make_unique<DspHotwordStateManager>(task_runner_, this);
  } else {
    state_manager_ = std::make_unique<DefaultHotwordStateManager>();
  }
}

AudioInputImpl::~AudioInputImpl() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  source_.reset();
}

void AudioInputImpl::Capture(const media::AudioBus* audio_source,
                             int audio_delay_milliseconds,
                             double volume,
                             bool key_pressed) {
  DCHECK_EQ(kFormat.num_channels, audio_source->channels());

  state_manager_->OnCaptureDataArrived();

  std::vector<int32_t> buffer(kFormat.num_channels * audio_source->frames());
  audio_source->ToInterleaved<media::SignedInt32SampleTypeTraits>(
      audio_source->frames(), buffer.data());
  int64_t time = base::TimeTicks::Now().since_origin().InMilliseconds() -
                 audio_delay_milliseconds;
  AudioInputBufferImpl input_buffer(buffer.data(), audio_source->frames());
  {
    base::AutoLock lock(lock_);
    for (auto* observer : observers_)
      observer->OnBufferAvailable(input_buffer, time);
  }
}

void AudioInputImpl::OnCaptureError(const std::string& message) {
  DLOG(ERROR) << "Capture error " << message;
  base::AutoLock lock(lock_);
  for (auto* observer : observers_)
    observer->OnError(AudioInput::Error::FATAL_ERROR);
}

void AudioInputImpl::OnCaptureMuted(bool is_muted) {}

assistant_client::BufferFormat AudioInputImpl::GetFormat() const {
  return kFormat;
}

void AudioInputImpl::AddObserver(
    assistant_client::AudioInput::Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(observer_sequence_checker_);
  bool should_start = false;
  {
    base::AutoLock lock(lock_);
    observers_.push_back(observer);
    should_start = observers_.size() == 1;
  }

  if (default_on_ && should_start) {
    // Post to main thread runner to start audio recording. Assistant thread
    // does not have thread context defined in //base and will fail sequence
    // check in AudioCapturerSource::Start().
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&AudioInputImpl::StartRecording,
                                          weak_factory_.GetWeakPtr()));
  }
}

void AudioInputImpl::RemoveObserver(
    assistant_client::AudioInput::Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(observer_sequence_checker_);
  bool should_stop = false;
  {
    base::AutoLock lock(lock_);
    base::Erase(observers_, observer);
    should_stop = observers_.empty();
  }
  if (should_stop) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&AudioInputImpl::StopRecording,
                                          weak_factory_.GetWeakPtr()));
  }
}

void AudioInputImpl::SetMicState(bool mic_open) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!default_on_) {
    if (mic_open)
      source_->Start();
    else
      source_->Stop();
  }
}

void AudioInputImpl::OnConversationTurnStarted() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  state_manager_->OnConversationTurnStarted();
}

void AudioInputImpl::OnConversationTurnFinished() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  state_manager_->OnConversationTurnFinished();
}

void AudioInputImpl::OnHotwordEnabled(bool enable) {
  default_on_ = enable;
  if (default_on_)
    source_->Start();
  else
    source_->Stop();
}

void AudioInputImpl::StartRecording() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!source_);
  RecreateAudioInputStream(IsDspHotwordEnabled());
}

void AudioInputImpl::StopRecording() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (source_) {
    source_->Stop();
    source_.reset();
  }
}

void AudioInputImpl::RecreateAudioInputStream(bool hotword) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  StopRecording();

  source_ = audio::CreateInputDevice(
      connector_->Clone(), media::AudioDeviceDescription::kDefaultDeviceId);
  // AUDIO_PCM_LINEAR and AUDIO_PCM_LOW_LATENCY are the same on CRAS.
  auto param = media::AudioParameters(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY, media::CHANNEL_LAYOUT_MONO,
      kFormat.sample_rate,
      kFormat.sample_rate / 10 /* buffer size for 100 ms */);
  if (hotword) {
    param.set_effects(media::AudioParameters::PlatformEffectsMask::HOTWORD);
  }
  source_->Initialize(param, this);
  source_->Start();
}

AudioInputProviderImpl::AudioInputProviderImpl(
    service_manager::Connector* connector,
    bool default_on)
    : audio_input_(connector, default_on) {}

AudioInputProviderImpl::~AudioInputProviderImpl() = default;

AudioInputImpl& AudioInputProviderImpl::GetAudioInput() {
  return audio_input_;
}

int64_t AudioInputProviderImpl::GetCurrentAudioTime() {
  // TODO(xiaohuic): see if we can support real timestamp.
  return 0;
}

void AudioInputProviderImpl::SetMicState(bool mic_open) {
  audio_input_.SetMicState(mic_open);
}

void AudioInputProviderImpl::OnHotwordEnabled(bool enable) {
  audio_input_.OnHotwordEnabled(enable);
}

}  // namespace assistant
}  // namespace chromeos
