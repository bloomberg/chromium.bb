// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/null_audio_sink.h"

#include "base/bind.h"
#include "base/message_loop_proxy.h"
#include "base/stringprintf.h"
#include "base/sys_byteorder.h"
#include "media/audio/fake_audio_consumer.h"

namespace media {

NullAudioSink::NullAudioSink(
    const scoped_refptr<base::MessageLoopProxy>& message_loop)
    : initialized_(false),
      playing_(false),
      callback_(NULL),
      hash_audio_for_testing_(false),
      channels_(0),
      message_loop_(message_loop) {
}

NullAudioSink::~NullAudioSink() {}

void NullAudioSink::Initialize(const AudioParameters& params,
                               RenderCallback* callback) {
  DCHECK(!initialized_);

  fake_consumer_.reset(new FakeAudioConsumer(message_loop_, params));

  if (hash_audio_for_testing_) {
    channels_ = params.channels();
    md5_channel_contexts_.reset(new base::MD5Context[params.channels()]);
    for (int i = 0; i < params.channels(); i++)
      base::MD5Init(&md5_channel_contexts_[i]);
  }

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
  if (!hash_audio_for_testing_ || frames_received <= 0)
    return;

  DCHECK_EQ(sizeof(float), sizeof(uint32));
  int channels = audio_bus->channels();
  for (int channel_idx = 0; channel_idx < channels; ++channel_idx) {
    float* channel = audio_bus->channel(channel_idx);
    for (int frame_idx = 0; frame_idx < frames_received; frame_idx++) {
      // Convert float to uint32 w/o conversion loss.
      uint32 frame = base::ByteSwapToLE32(bit_cast<uint32>(channel[frame_idx]));
      base::MD5Update(&md5_channel_contexts_[channel_idx], base::StringPiece(
          reinterpret_cast<char*>(&frame), sizeof(frame)));
    }
  }
}

void NullAudioSink::StartAudioHashForTesting() {
  DCHECK(!initialized_);
  hash_audio_for_testing_ = true;
}

std::string NullAudioSink::GetAudioHashForTesting() {
  DCHECK(hash_audio_for_testing_);

  base::MD5Digest digest;
  if (channels_ == 0) {
    // If initialize failed or was never called, ensure we return an empty hash.
    base::MD5Context context;
    base::MD5Init(&context);
    base::MD5Final(&digest, &context);
  } else {
    // Hash all channels into the first channel.
    for (int i = 1; i < channels_; i++) {
      base::MD5Final(&digest, &md5_channel_contexts_[i]);
      base::MD5Update(&md5_channel_contexts_[0], base::StringPiece(
          reinterpret_cast<char*>(&digest), sizeof(base::MD5Digest)));
    }
    base::MD5Final(&digest, &md5_channel_contexts_[0]);
  }

  return base::MD5DigestToBase16(digest);
}

}  // namespace media
