// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_renderer_mixer.h"

#if defined(ARCH_CPU_X86_FAMILY) && defined(__SSE__)
#include <xmmintrin.h>
#endif

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/cpu.h"
#include "base/logging.h"
#include "base/memory/aligned_memory.h"
#include "media/audio/audio_util.h"
#include "media/base/limits.h"

namespace media {

AudioRendererMixer::AudioRendererMixer(
    const AudioParameters& input_params, const AudioParameters& output_params,
    const scoped_refptr<AudioRendererSink>& sink)
    : audio_sink_(sink),
      current_audio_delay_milliseconds_(0) {
  // Sanity check sample rates.
  DCHECK_LE(input_params.sample_rate(), limits::kMaxSampleRate);
  DCHECK_GE(input_params.sample_rate(), limits::kMinSampleRate);
  DCHECK_LE(output_params.sample_rate(), limits::kMaxSampleRate);
  DCHECK_GE(output_params.sample_rate(), limits::kMinSampleRate);

  // Only resample if necessary since it's expensive.
  if (input_params.sample_rate() != output_params.sample_rate()) {
    resampler_.reset(new MultiChannelResampler(
        output_params.channels(),
        input_params.sample_rate() / static_cast<double>(
            output_params.sample_rate()),
        base::Bind(&AudioRendererMixer::ProvideInput, base::Unretained(this))));
  }

  audio_sink_->Initialize(output_params, this);
  audio_sink_->Start();
}

AudioRendererMixer::~AudioRendererMixer() {
  // AudioRendererSinks must be stopped before being destructed.
  audio_sink_->Stop();

  // Ensures that all mixer inputs have stopped themselves prior to destruction
  // and have called RemoveMixerInput().
  DCHECK_EQ(mixer_inputs_.size(), 0U);
}

void AudioRendererMixer::AddMixerInput(
    const scoped_refptr<AudioRendererMixerInput>& input) {
  base::AutoLock auto_lock(mixer_inputs_lock_);
  mixer_inputs_.insert(input);
}

void AudioRendererMixer::RemoveMixerInput(
    const scoped_refptr<AudioRendererMixerInput>& input) {
  base::AutoLock auto_lock(mixer_inputs_lock_);
  mixer_inputs_.erase(input);
}

int AudioRendererMixer::Render(AudioBus* audio_bus,
                               int audio_delay_milliseconds) {
  current_audio_delay_milliseconds_ = audio_delay_milliseconds;

  if (resampler_.get())
    resampler_->Resample(audio_bus, audio_bus->frames());
  else
    ProvideInput(audio_bus);

  // Always return the full number of frames requested, ProvideInput() will pad
  // with silence if it wasn't able to acquire enough data.
  return audio_bus->frames();
}

void AudioRendererMixer::ProvideInput(AudioBus* audio_bus) {
  base::AutoLock auto_lock(mixer_inputs_lock_);

  // Allocate staging area for each mixer input's audio data on first call.  We
  // won't know how much to allocate until here because of resampling.  Ensure
  // our intermediate AudioBus is sized exactly as the original.  Resize should
  // only happen once due to the way the resampler works.
  if (!mixer_input_audio_bus_.get() ||
      mixer_input_audio_bus_->frames() != audio_bus->frames()) {
    mixer_input_audio_bus_ =
        AudioBus::Create(audio_bus->channels(), audio_bus->frames());
  }

  // Sanity check our inputs.
  DCHECK_EQ(audio_bus->frames(), mixer_input_audio_bus_->frames());
  DCHECK_EQ(audio_bus->channels(), mixer_input_audio_bus_->channels());

  // Zero |audio_bus| so we're mixing into a clean buffer and return silence if
  // we couldn't get enough data from our inputs.
  audio_bus->Zero();

  // Have each mixer render its data into an output buffer then mix the result.
  for (AudioRendererMixerInputSet::iterator it = mixer_inputs_.begin();
       it != mixer_inputs_.end(); ++it) {
    const scoped_refptr<AudioRendererMixerInput>& input = *it;

    double volume;
    input->GetVolume(&volume);

    // Nothing to do if the input isn't playing.
    if (!input->playing())
      continue;

    int frames_filled = input->callback()->Render(
        mixer_input_audio_bus_.get(), current_audio_delay_milliseconds_);
    if (frames_filled == 0)
      continue;

    // Volume adjust and mix each mixer input into |audio_bus| after rendering.
    for (int i = 0; i < audio_bus->channels(); ++i) {
       VectorFMAC(mixer_input_audio_bus_->channel(i), volume, frames_filled,
                  audio_bus->channel(i));
    }

    // No need to clamp values as InterleaveFloatToInt() will take care of this
    // for us later when data is transferred to the browser process.
  }
}

void AudioRendererMixer::OnRenderError() {
  base::AutoLock auto_lock(mixer_inputs_lock_);

  // Call each mixer input and signal an error.
  for (AudioRendererMixerInputSet::iterator it = mixer_inputs_.begin();
       it != mixer_inputs_.end(); ++it) {
    (*it)->callback()->OnRenderError();
  }
}

void AudioRendererMixer::VectorFMAC(const float src[], float scale, int len,
                                    float dest[]) {
  // Rely on function level static initialization to keep VectorFMACProc
  // selection thread safe.
  typedef void (*VectorFMACProc)(const float src[], float scale, int len,
                                 float dest[]);
#if defined(ARCH_CPU_X86_FAMILY) && defined(__SSE__)
  static const VectorFMACProc kVectorFMACProc =
      base::CPU().has_sse() ? VectorFMAC_SSE : VectorFMAC_C;
#else
  static const VectorFMACProc kVectorFMACProc = VectorFMAC_C;
#endif

  return kVectorFMACProc(src, scale, len, dest);
}

void AudioRendererMixer::VectorFMAC_C(const float src[], float scale, int len,
                                      float dest[]) {
  for (int i = 0; i < len; ++i)
    dest[i] += src[i] * scale;
}

#if defined(ARCH_CPU_X86_FAMILY) && defined(__SSE__)
void AudioRendererMixer::VectorFMAC_SSE(const float src[], float scale, int len,
                                        float dest[]) {
  // Ensure |src| and |dest| are 16-byte aligned.
  DCHECK_EQ(0u, reinterpret_cast<uintptr_t>(src) & 0x0F);
  DCHECK_EQ(0u, reinterpret_cast<uintptr_t>(dest) & 0x0F);

  __m128 m_scale = _mm_set_ps1(scale);
  int rem = len % 4;
  for (int i = 0; i < len - rem; i += 4) {
    _mm_store_ps(dest + i, _mm_add_ps(_mm_load_ps(dest + i),
                 _mm_mul_ps(_mm_load_ps(src + i), m_scale)));
  }

  // Handle any remaining values that wouldn't fit in an SSE pass.
  if (rem)
    VectorFMAC_C(src + len - rem, scale, rem, dest + len - rem);
}
#endif

}  // namespace media
