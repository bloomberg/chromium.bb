// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/virtual_audio_input_stream.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "media/audio/virtual_audio_output_stream.h"

namespace media {

// LoopbackAudioConverter works similar to AudioConverter and converts input
// streams to different audio parameters. Then, the LoopbackAudioConverter can
// be used as an input to another AudioConverter. This allows us to
// use converted audio from AudioOutputStreams as input to an AudioConverter.
// For example, this allows converting multiple streams into a common format and
// using the converted audio as input to another AudioConverter (i.e. a mixer).
class LoopbackAudioConverter : public AudioConverter::InputCallback {
 public:
  LoopbackAudioConverter(const AudioParameters& input_params,
                         const AudioParameters& output_params)
      : audio_converter_(input_params, output_params, false) {}

  virtual ~LoopbackAudioConverter() {}

  void AddInput(AudioConverter::InputCallback* input) {
    audio_converter_.AddInput(input);
  }

  void RemoveInput(AudioConverter::InputCallback* input) {
    audio_converter_.RemoveInput(input);
  }

 private:
  virtual double ProvideInput(AudioBus* audio_bus,
                              base::TimeDelta buffer_delay) OVERRIDE {
    audio_converter_.Convert(audio_bus);
    return 1.0;
  }

  AudioConverter audio_converter_;

  DISALLOW_COPY_AND_ASSIGN(LoopbackAudioConverter);
};

VirtualAudioInputStream::VirtualAudioInputStream(
    const AudioParameters& params, base::MessageLoopProxy* message_loop,
    const AfterCloseCallback& after_close_cb)
    : message_loop_(message_loop),
      after_close_cb_(after_close_cb),
      callback_(NULL),
      buffer_duration_(base::TimeDelta::FromMicroseconds(
          params.frames_per_buffer() * base::Time::kMicrosecondsPerSecond /
          static_cast<float>(params.sample_rate()))),
      buffer_(new uint8[params.GetBytesPerBuffer()]),
      params_(params),
      audio_bus_(AudioBus::Create(params_)),
      mixer_(params_, params_, false),
      num_attached_output_streams_(0) {
  DCHECK(params_.IsValid());
  DCHECK(message_loop_);
}

VirtualAudioInputStream::~VirtualAudioInputStream() {
  for (AudioConvertersMap::iterator it = converters_.begin();
       it != converters_.end(); ++it)
    delete it->second;

  DCHECK_EQ(0, num_attached_output_streams_);
}

bool VirtualAudioInputStream::Open() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  memset(buffer_.get(), 0, params_.GetBytesPerBuffer());
  return true;
}

void VirtualAudioInputStream::Start(AudioInputCallback* callback) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  callback_ = callback;
  next_read_time_ = base::Time::Now();
  on_more_data_cb_.Reset(base::Bind(&VirtualAudioInputStream::ReadAudio,
                                    base::Unretained(this)));
  message_loop_->PostTask(FROM_HERE, on_more_data_cb_.callback());
}

void VirtualAudioInputStream::Stop() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  on_more_data_cb_.Cancel();
}

void VirtualAudioInputStream::AddOutputStream(
    VirtualAudioOutputStream* stream, const AudioParameters& output_params) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  AudioConvertersMap::iterator converter = converters_.find(output_params);
  if (converter == converters_.end()) {
    std::pair<AudioConvertersMap::iterator, bool> result = converters_.insert(
        std::make_pair(output_params,
                       new LoopbackAudioConverter(output_params, params_)));
    converter = result.first;

    // Add to main mixer if we just added a new AudioTransform.
    mixer_.AddInput(converter->second);
  }
  converter->second->AddInput(stream);
  ++num_attached_output_streams_;
}

void VirtualAudioInputStream::RemoveOutputStream(
    VirtualAudioOutputStream* stream, const AudioParameters& output_params) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  DCHECK(converters_.find(output_params) != converters_.end());
  converters_[output_params]->RemoveInput(stream);

  --num_attached_output_streams_;
  DCHECK_LE(0, num_attached_output_streams_);
}

void VirtualAudioInputStream::ReadAudio() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(callback_);

  mixer_.Convert(audio_bus_.get());
  audio_bus_->ToInterleaved(params_.frames_per_buffer(),
                            params_.bits_per_sample() / 8,
                            buffer_.get());

  callback_->OnData(this,
                    buffer_.get(),
                    params_.GetBytesPerBuffer(),
                    params_.GetBytesPerBuffer(),
                    1.0);

  // Need to account for time spent here due to renderer side mixing as well as
  // the imprecision of PostDelayedTask.
  next_read_time_ += buffer_duration_;
  base::Time now = base::Time::Now();
  base::TimeDelta delay = next_read_time_ - now;
  if (delay < base::TimeDelta()) {
    // Reset the next read time if we end up getting too far behind. We'll just
    // slow down playback to avoid using up all the CPU.
    delay = buffer_duration_;
    next_read_time_ = now;
  }

  message_loop_->PostDelayedTask(FROM_HERE,
                                 on_more_data_cb_.callback(),
                                 delay);
}

void VirtualAudioInputStream::Close() {
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (callback_) {
    DCHECK(on_more_data_cb_.IsCancelled());
    callback_->OnClose(this);
    callback_ = NULL;
  }

  // If a non-null AfterCloseCallback was provided to the constructor, invoke it
  // here.  The callback is moved to a stack-local first since |this| could be
  // destroyed during Run().
  if (!after_close_cb_.is_null()) {
    const AfterCloseCallback cb = after_close_cb_;
    after_close_cb_.Reset();
    cb.Run(this);
  }
}

double VirtualAudioInputStream::GetMaxVolume() {
  return 1.0;
}

void VirtualAudioInputStream::SetVolume(double volume) {}

double VirtualAudioInputStream::GetVolume() {
  return 1.0;
}

void VirtualAudioInputStream::SetAutomaticGainControl(bool enabled) {}

bool VirtualAudioInputStream::GetAutomaticGainControl() {
  return false;
}

}  // namespace media
