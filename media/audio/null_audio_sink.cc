// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/null_audio_sink.h"

#include "base/bind.h"
#include "base/message_loop_proxy.h"
#include "media/audio/fake_audio_consumer.h"
#include "media/base/audio_hash.h"

namespace media {

NullAudioSink::NullAudioSink(
    const scoped_refptr<base::MessageLoopProxy>& message_loop)
    : initialized_(false),
      playing_(false),
      callback_(NULL),
      message_loop_(message_loop) {
}

NullAudioSink::~NullAudioSink() {}

void NullAudioSink::Initialize(const AudioParameters& params,
                               RenderCallback* callback) {
  DCHECK(!initialized_);
  fake_consumer_.reset(new FakeAudioConsumer(message_loop_, params));
  callback_ = callback;
  initialized_ = true;
}

void NullAudioSink::Start() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(!playing_);
}

void NullAudioSink::Stop() {
  DCHECK(message_loop_->BelongsToCurrentThread());

  // Stop may be called at any time, so we have to check before stopping.
  if (fake_consumer_)
    fake_consumer_->Stop();
}

void NullAudioSink::Play() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(initialized_);

  if (playing_)
    return;

  fake_consumer_->Start(base::Bind(
      &NullAudioSink::CallRender, base::Unretained(this)));
  playing_ = true;
}

void NullAudioSink::Pause(bool /* flush */) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (!playing_)
    return;

  fake_consumer_->Stop();
  playing_ = false;
}

bool NullAudioSink::SetVolume(double volume) {
  // Audio is always muted.
  return volume == 0.0;
}

void NullAudioSink::CallRender(AudioBus* audio_bus) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  int frames_received = callback_->Render(audio_bus, 0);
  if (!audio_hash_ || frames_received <= 0)
    return;

  audio_hash_->Update(audio_bus, frames_received);
}

void NullAudioSink::StartAudioHashForTesting() {
  DCHECK(!initialized_);
  audio_hash_.reset(new AudioHash());
}

std::string NullAudioSink::GetAudioHashForTesting() {
  return audio_hash_ ? audio_hash_->ToString() : "";
}

}  // namespace media
