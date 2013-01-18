// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/virtual_audio_output_stream.h"

#include "base/message_loop_proxy.h"
#include "media/audio/virtual_audio_input_stream.h"

namespace media {

VirtualAudioOutputStream::VirtualAudioOutputStream(
    const AudioParameters& params, base::MessageLoopProxy* message_loop,
    VirtualAudioInputStream* target, const AfterCloseCallback& after_close_cb)
    : params_(params), message_loop_(message_loop),
      target_input_stream_(target), after_close_cb_(after_close_cb),
      callback_(NULL), volume_(1.0f) {
  DCHECK(params_.IsValid());
  DCHECK(message_loop_);
  DCHECK(target);
}

VirtualAudioOutputStream::~VirtualAudioOutputStream() {
  DCHECK(!callback_);
}

bool VirtualAudioOutputStream::Open() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  return true;
}

void VirtualAudioOutputStream::Start(AudioSourceCallback* callback)  {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(!callback_);
  callback_ = callback;
  target_input_stream_->AddOutputStream(this, params_);
}

void VirtualAudioOutputStream::Stop() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  if (callback_) {
    callback_ = NULL;
    target_input_stream_->RemoveOutputStream(this, params_);
  }
}

void VirtualAudioOutputStream::Close() {
  DCHECK(message_loop_->BelongsToCurrentThread());

  Stop();

  // If a non-null AfterCloseCallback was provided to the constructor, invoke it
  // here.  The callback is moved to a stack-local first since |this| could be
  // destroyed during Run().
  if (!after_close_cb_.is_null()) {
    const AfterCloseCallback cb = after_close_cb_;
    after_close_cb_.Reset();
    cb.Run(this);
  }
}

void VirtualAudioOutputStream::SetVolume(double volume) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  volume_ = volume;
}

void VirtualAudioOutputStream::GetVolume(double* volume) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  *volume = volume_;
}

double VirtualAudioOutputStream::ProvideInput(AudioBus* audio_bus,
                                              base::TimeDelta buffer_delay) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(callback_);

  int frames = callback_->OnMoreData(audio_bus, AudioBuffersState());
  if (frames < audio_bus->frames())
    audio_bus->ZeroFramesPartial(frames, audio_bus->frames() - frames);

  return frames > 0 ? volume_ : 0;
}

}  // namespace media
