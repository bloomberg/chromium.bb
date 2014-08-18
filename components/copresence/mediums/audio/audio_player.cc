// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/copresence/mediums/audio/audio_player.h"

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "components/copresence/public/copresence_constants.h"
#include "content/public/browser/browser_thread.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_parameters.h"
#include "media/base/audio_bus.h"

namespace {

const int kDefaultFrameCount = 1024;
const double kOutputVolumePercent = 1.0f;

}  // namespace

namespace copresence {

// Public methods.

AudioPlayer::AudioPlayer()
    : is_playing_(false), stream_(NULL), frame_index_(0) {
}

AudioPlayer::~AudioPlayer() {
}

void AudioPlayer::Initialize() {
  media::AudioManager::Get()->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&AudioPlayer::InitializeOnAudioThread,
                 base::Unretained(this)));
}

void AudioPlayer::Play(
    const scoped_refptr<media::AudioBusRefCounted>& samples) {
  media::AudioManager::Get()->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(
          &AudioPlayer::PlayOnAudioThread, base::Unretained(this), samples));
}

void AudioPlayer::Stop() {
  media::AudioManager::Get()->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&AudioPlayer::StopOnAudioThread, base::Unretained(this)));
}

bool AudioPlayer::IsPlaying() {
  return is_playing_;
}

void AudioPlayer::Finalize() {
  media::AudioManager::Get()->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&AudioPlayer::FinalizeOnAudioThread, base::Unretained(this)));
}

// Private methods.

void AudioPlayer::InitializeOnAudioThread() {
  DCHECK(media::AudioManager::Get()->GetTaskRunner()->BelongsToCurrentThread());
  stream_ = output_stream_for_testing_
                ? output_stream_for_testing_.get()
                : media::AudioManager::Get()->MakeAudioOutputStreamProxy(
                      media::AudioParameters(
                          media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                          media::CHANNEL_LAYOUT_MONO,
                          kDefaultSampleRate,
                          kDefaultBitsPerSample,
                          kDefaultFrameCount),
                      std::string());

  if (!stream_ || !stream_->Open()) {
    LOG(ERROR) << "Failed to open an output stream.";
    if (stream_) {
      stream_->Close();
      stream_ = NULL;
    }
    return;
  }
  stream_->SetVolume(kOutputVolumePercent);
}

void AudioPlayer::PlayOnAudioThread(
    const scoped_refptr<media::AudioBusRefCounted>& samples) {
  DCHECK(media::AudioManager::Get()->GetTaskRunner()->BelongsToCurrentThread());
  if (!stream_)
    return;

  {
    base::AutoLock al(state_lock_);

    samples_ = samples;
    frame_index_ = 0;

    if (is_playing_)
      return;
  }

  is_playing_ = true;
  stream_->Start(this);
}

void AudioPlayer::StopOnAudioThread() {
  DCHECK(media::AudioManager::Get()->GetTaskRunner()->BelongsToCurrentThread());
  if (!stream_)
    return;

  stream_->Stop();
  is_playing_ = false;
}

void AudioPlayer::StopAndCloseOnAudioThread() {
  DCHECK(media::AudioManager::Get()->GetTaskRunner()->BelongsToCurrentThread());
  if (!stream_)
    return;

  if (is_playing_)
    stream_->Stop();
  stream_->Close();
  stream_ = NULL;

  is_playing_ = false;
}

void AudioPlayer::FinalizeOnAudioThread() {
  DCHECK(media::AudioManager::Get()->GetTaskRunner()->BelongsToCurrentThread());
  StopAndCloseOnAudioThread();
  delete this;
}

int AudioPlayer::OnMoreData(media::AudioBus* dest,
                            media::AudioBuffersState /* state */) {
  base::AutoLock al(state_lock_);
  // Continuously play our samples till explicitly told to stop.
  const int leftover_frames = samples_->frames() - frame_index_;
  const int frames_to_copy = std::min(dest->frames(), leftover_frames);

  samples_->CopyPartialFramesTo(frame_index_, frames_to_copy, 0, dest);
  frame_index_ += frames_to_copy;

  // If we didn't fill the destination audio bus, wrap around and fill the rest.
  if (leftover_frames <= dest->frames()) {
    samples_->CopyPartialFramesTo(
        0, dest->frames() - frames_to_copy, frames_to_copy, dest);
    frame_index_ = dest->frames() - frames_to_copy;
  }

  return dest->frames();
}

void AudioPlayer::OnError(media::AudioOutputStream* /* stream */) {
  LOG(ERROR) << "Error during system sound reproduction.";
  media::AudioManager::Get()->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&AudioPlayer::StopAndCloseOnAudioThread,
                 base::Unretained(this)));
}

void AudioPlayer::FlushAudioLoopForTesting() {
  if (media::AudioManager::Get()->GetTaskRunner()->BelongsToCurrentThread())
    return;

  // Queue task on the audio thread, when it is executed, that means we've
  // successfully executed all the tasks before us.
  base::RunLoop rl;
  media::AudioManager::Get()->GetTaskRunner()->PostTaskAndReply(
      FROM_HERE,
      base::Bind(base::IgnoreResult(&AudioPlayer::FlushAudioLoopForTesting),
                 base::Unretained(this)),
      rl.QuitClosure());
  rl.Run();
}

}  // namespace copresence
