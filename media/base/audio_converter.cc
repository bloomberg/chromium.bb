// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_converter.h"

#include <algorithm>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "media/base/audio_pull_fifo.h"
#include "media/base/channel_mixer.h"
#include "media/base/multi_channel_resampler.h"
#include "media/base/vector_math.h"

namespace media {

AudioConverter::AudioConverter(const AudioParameters& input_params,
                               const AudioParameters& output_params,
                               bool disable_fifo)
    : downmix_early_(false),
      resampler_frame_delay_(0),
      input_channel_count_(input_params.channels()) {
  CHECK(input_params.IsValid());
  CHECK(output_params.IsValid());

  // Handle different input and output channel layouts.
  if (input_params.channel_layout() != output_params.channel_layout()) {
    DVLOG(1) << "Remixing channel layout from " << input_params.channel_layout()
             << " to " << output_params.channel_layout() << "; from "
             << input_params.channels() << " channels to "
             << output_params.channels() << " channels.";
    channel_mixer_.reset(new ChannelMixer(input_params, output_params));

    // Pare off data as early as we can for efficiency.
    downmix_early_ = input_params.channels() > output_params.channels();
    if (downmix_early_) {
      DVLOG(1) << "Remixing channel layout prior to resampling.";
      // |unmixed_audio_| will be allocated on the fly.
    } else {
      // Instead, if we're not downmixing early we need a temporary AudioBus
      // which matches the input channel count but uses the output frame size
      // since we'll mix into the AudioBus from the output stream.
      unmixed_audio_ = AudioBus::Create(
          input_params.channels(), output_params.frames_per_buffer());
    }
  }

  // Only resample if necessary since it's expensive.
  if (input_params.sample_rate() != output_params.sample_rate()) {
    DVLOG(1) << "Resampling from " << input_params.sample_rate() << " to "
             << output_params.sample_rate();
    double io_sample_rate_ratio = input_params.sample_rate() /
        static_cast<double>(output_params.sample_rate());
    resampler_.reset(new MultiChannelResampler(
        downmix_early_ ? output_params.channels() :
            input_params.channels(),
        io_sample_rate_ratio, base::Bind(
            &AudioConverter::ProvideInput, base::Unretained(this))));
  }

  input_frame_duration_ = base::TimeDelta::FromMicroseconds(
      base::Time::kMicrosecondsPerSecond /
      static_cast<double>(input_params.sample_rate()));
  output_frame_duration_ = base::TimeDelta::FromMicroseconds(
      base::Time::kMicrosecondsPerSecond /
      static_cast<double>(output_params.sample_rate()));

  if (disable_fifo)
    return;

  // Since the resampler / output device may want a different buffer size than
  // the caller asked for, we need to use a FIFO to ensure that both sides
  // read in chunk sizes they're configured for.
  if (resampler_.get() ||
      input_params.frames_per_buffer() != output_params.frames_per_buffer()) {
    DVLOG(1) << "Rebuffering from " << input_params.frames_per_buffer()
             << " to " << output_params.frames_per_buffer();
    audio_fifo_.reset(new AudioPullFifo(
        downmix_early_ ? output_params.channels() :
            input_params.channels(),
        input_params.frames_per_buffer(), base::Bind(
            &AudioConverter::SourceCallback,
            base::Unretained(this))));
  }
}

AudioConverter::~AudioConverter() {}

void AudioConverter::AddInput(InputCallback* input) {
  transform_inputs_.push_back(input);
}

void AudioConverter::RemoveInput(InputCallback* input) {
  DCHECK(std::find(transform_inputs_.begin(), transform_inputs_.end(), input) !=
         transform_inputs_.end());
  transform_inputs_.remove(input);

  if (transform_inputs_.empty())
    Reset();
}

void AudioConverter::Reset() {
  if (audio_fifo_)
    audio_fifo_->Clear();
  if (resampler_)
    resampler_->Flush();
}

void AudioConverter::Convert(AudioBus* dest) {
  if (transform_inputs_.empty()) {
    dest->Zero();
    return;
  }

  bool needs_mixing = channel_mixer_ && !downmix_early_;
  AudioBus* temp_dest = needs_mixing ? unmixed_audio_.get() : dest;
  DCHECK(temp_dest);

  if (!resampler_ && !audio_fifo_) {
    SourceCallback(0, temp_dest);
  } else {
    if (resampler_)
      resampler_->Resample(temp_dest, temp_dest->frames());
    else
      ProvideInput(0, temp_dest);
  }

  if (needs_mixing) {
    DCHECK_EQ(temp_dest->frames(), dest->frames());
    channel_mixer_->Transform(temp_dest, dest);
  }
}

void AudioConverter::SourceCallback(int fifo_frame_delay, AudioBus* dest) {
  bool needs_downmix = channel_mixer_ && downmix_early_;

  if (!mixer_input_audio_bus_ ||
      mixer_input_audio_bus_->frames() != dest->frames()) {
    mixer_input_audio_bus_ =
        AudioBus::Create(input_channel_count_, dest->frames());
  }

  if (needs_downmix &&
      (!unmixed_audio_ || unmixed_audio_->frames() != dest->frames())) {
    // If we're downmixing early we need a temporary AudioBus which matches
    // the the input channel count and input frame size since we're passing
    // |unmixed_audio_| directly to the |source_callback_|.
    unmixed_audio_ = AudioBus::Create(input_channel_count_, dest->frames());
  }

  AudioBus* temp_dest = needs_downmix ? unmixed_audio_.get() : dest;

  // Sanity check our inputs.
  DCHECK_EQ(temp_dest->frames(), mixer_input_audio_bus_->frames());
  DCHECK_EQ(temp_dest->channels(), mixer_input_audio_bus_->channels());

  // Calculate the buffer delay for this callback.
  base::TimeDelta buffer_delay;
  if (resampler_) {
    buffer_delay += base::TimeDelta::FromMicroseconds(
        resampler_frame_delay_ * output_frame_duration_.InMicroseconds());
  }
  if (audio_fifo_) {
    buffer_delay += base::TimeDelta::FromMicroseconds(
        fifo_frame_delay * input_frame_duration_.InMicroseconds());
  }

  // Have each mixer render its data into an output buffer then mix the result.
  for (InputCallbackSet::iterator it = transform_inputs_.begin();
       it != transform_inputs_.end(); ++it) {
    InputCallback* input = *it;

    float volume = input->ProvideInput(
        mixer_input_audio_bus_.get(), buffer_delay);

    // Optimize the most common single input, full volume case.
    if (it == transform_inputs_.begin()) {
      if (volume == 1.0f) {
        mixer_input_audio_bus_->CopyTo(temp_dest);
        continue;
      }

      // Zero |temp_dest| otherwise, so we're mixing into a clean buffer.
      temp_dest->Zero();
    }

    // Volume adjust and mix each mixer input into |temp_dest| after rendering.
    if (volume > 0) {
      for (int i = 0; i < mixer_input_audio_bus_->channels(); ++i) {
        vector_math::FMAC(
            mixer_input_audio_bus_->channel(i), volume,
            mixer_input_audio_bus_->frames(), temp_dest->channel(i));
      }
    }
  }

  if (needs_downmix) {
    DCHECK_EQ(temp_dest->frames(), dest->frames());
    channel_mixer_->Transform(temp_dest, dest);
  }
}

void AudioConverter::ProvideInput(int resampler_frame_delay, AudioBus* dest) {
  resampler_frame_delay_ = resampler_frame_delay;
  if (audio_fifo_)
    audio_fifo_->Consume(dest, dest->frames());
  else
    SourceCallback(0, dest);
}

}  // namespace media
