// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/fake_audio_output_stream.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "media/audio/audio_manager_base.h"

namespace media {

// static
AudioOutputStream* FakeAudioOutputStream::MakeFakeStream(
    AudioManagerBase* manager, const AudioParameters& params) {
  return new FakeAudioOutputStream(manager, params);
}

FakeAudioOutputStream::FakeAudioOutputStream(AudioManagerBase* manager,
                                             const AudioParameters& params)
    : audio_manager_(manager),
      callback_(NULL),
      fake_consumer_(manager->GetMessageLoop(), params) {
}

FakeAudioOutputStream::~FakeAudioOutputStream() {
  DCHECK(!callback_);
}

bool FakeAudioOutputStream::Open() {
  DCHECK(audio_manager_->GetMessageLoop()->BelongsToCurrentThread());
  return true;
}

void FakeAudioOutputStream::Start(AudioSourceCallback* callback)  {
  DCHECK(audio_manager_->GetMessageLoop()->BelongsToCurrentThread());
  callback_ = callback;
  fake_consumer_.Start(base::Bind(
      &FakeAudioOutputStream::CallOnMoreData, base::Unretained(this)));
}

void FakeAudioOutputStream::Stop() {
  DCHECK(audio_manager_->GetMessageLoop()->BelongsToCurrentThread());
  fake_consumer_.Stop();
  callback_ = NULL;
}

void FakeAudioOutputStream::Close() {
  DCHECK(!callback_);
  DCHECK(audio_manager_->GetMessageLoop()->BelongsToCurrentThread());
  audio_manager_->ReleaseOutputStream(this);
}

void FakeAudioOutputStream::SetVolume(double volume) {};

void FakeAudioOutputStream::GetVolume(double* volume) {
  *volume = 0;
};

void FakeAudioOutputStream::CallOnMoreData(AudioBus* audio_bus) {
  DCHECK(audio_manager_->GetMessageLoop()->BelongsToCurrentThread());
  callback_->OnMoreData(audio_bus, AudioBuffersState());
}

}  // namespace media
