// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/sounds/audio_stream_handler.h"

#include <string>

#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_manager_base.h"
#include "media/base/channel_layout.h"

namespace media {

namespace {

// Volume percent.
const double kOutputVolumePercent = 0.8;

// The number of frames each OnMoreData() call will request.
const int kDefaultFrameCount = 1024;

AudioStreamHandler::TestObserver* g_observer_for_testing = NULL;
AudioOutputStream::AudioSourceCallback* g_audio_source_for_testing = NULL;

}  // namespace

class AudioStreamHandler::AudioStreamContainer
    : public AudioOutputStream::AudioSourceCallback {
 public:
  AudioStreamContainer(const WavAudioHandler& wav_audio,
                       const AudioParameters& params)
      : stream_(NULL),
        wav_audio_(wav_audio),
        params_(params),
        cursor_(0) {
  }

  virtual ~AudioStreamContainer() {
    DCHECK(AudioManager::Get()->GetMessageLoop()->BelongsToCurrentThread());
  }

  void Play() {
    DCHECK(AudioManager::Get()->GetMessageLoop()->BelongsToCurrentThread());

    if (!stream_) {
      stream_ = AudioManager::Get()->MakeAudioOutputStreamProxy(params_,
                                                                std::string(),
                                                                std::string());
      if (!stream_ || !stream_->Open()) {
        LOG(ERROR) << "Failed to open an output stream.";
        return;
      }
      stream_->SetVolume(kOutputVolumePercent);
    } else {
      // TODO (ygorshenin@): implement smart stream rewind.
      stream_->Stop();
    }

    cursor_ = 0;
    if (g_audio_source_for_testing)
      stream_->Start(g_audio_source_for_testing);
    else
      stream_->Start(this);

    if (g_observer_for_testing)
      g_observer_for_testing->OnPlay();
  }

  void Stop() {
    DCHECK(AudioManager::Get()->GetMessageLoop()->BelongsToCurrentThread());
    if (!stream_)
      return;
    stream_->Stop();
    stream_->Close();
    stream_ = NULL;

    if (g_observer_for_testing)
      g_observer_for_testing->OnStop(cursor_);
  }

 private:
  // AudioOutputStream::AudioSourceCallback overrides:
  // Following methods could be called from *ANY* thread.
  virtual int OnMoreData(AudioBus* dest,
                         AudioBuffersState /* state */) OVERRIDE {
    size_t bytes_written = 0;
    if (wav_audio_.AtEnd(cursor_) ||
        !wav_audio_.CopyTo(dest, cursor_, &bytes_written)) {
      AudioManager::Get()->GetMessageLoop()->PostTask(
          FROM_HERE,
          base::Bind(&AudioStreamContainer::Stop, base::Unretained(this)));
      return 0;
    }
    cursor_ += bytes_written;

    return dest->frames();
  }

  virtual int OnMoreIOData(AudioBus* /* source */,
                           AudioBus* dest,
                           AudioBuffersState state) OVERRIDE {
    return OnMoreData(dest, state);
  }

  virtual void OnError(AudioOutputStream* /* stream */) OVERRIDE {
    LOG(ERROR) << "Error during system sound reproduction.";
  }

  AudioOutputStream* stream_;

  const WavAudioHandler wav_audio_;
  const AudioParameters params_;

  size_t cursor_;

  DISALLOW_COPY_AND_ASSIGN(AudioStreamContainer);
};

AudioStreamHandler::AudioStreamHandler(const base::StringPiece& wav_data)
    : wav_audio_(wav_data),
      initialized_(false) {
  AudioManager* manager = AudioManager::Get();
  if (!manager) {
    LOG(ERROR) << "Can't get access to audio manager.";
    return;
  }
  AudioParameters params(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                         GuessChannelLayout(wav_audio_.num_channels()),
                         wav_audio_.sample_rate(),
                         wav_audio_.bits_per_sample(),
                         kDefaultFrameCount);
  if (!params.IsValid()) {
    LOG(ERROR) << "Audio params are invalid.";
    return;
  }
  stream_.reset(new AudioStreamContainer(wav_audio_, params));
  initialized_ = true;
}

AudioStreamHandler::~AudioStreamHandler() {
  DCHECK(CalledOnValidThread());
  AudioManager::Get()->GetMessageLoop()->PostTask(
      FROM_HERE,
      base::Bind(&AudioStreamContainer::Stop, base::Unretained(stream_.get())));
  AudioManager::Get()->GetMessageLoop()->DeleteSoon(FROM_HERE,
                                                    stream_.release());
}

bool AudioStreamHandler::IsInitialized() const {
  DCHECK(CalledOnValidThread());
  return initialized_;
}

bool AudioStreamHandler::Play() {
  DCHECK(CalledOnValidThread());

  if (!IsInitialized())
    return false;

  AudioManager::Get()->GetMessageLoop()->PostTask(
      FROM_HERE,
      base::Bind(base::IgnoreResult(&AudioStreamContainer::Play),
                 base::Unretained(stream_.get())));
  return true;
}

void AudioStreamHandler::Stop() {
  DCHECK(CalledOnValidThread());
  AudioManager::Get()->GetMessageLoop()->PostTask(
      FROM_HERE,
      base::Bind(&AudioStreamContainer::Stop, base::Unretained(stream_.get())));
}

// static
void AudioStreamHandler::SetObserverForTesting(TestObserver* observer) {
  g_observer_for_testing = observer;
}

// static
void AudioStreamHandler::SetAudioSourceForTesting(
    AudioOutputStream::AudioSourceCallback* source) {
  g_audio_source_for_testing = source;
}

}  // namespace media
