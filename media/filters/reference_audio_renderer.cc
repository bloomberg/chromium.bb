// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/reference_audio_renderer.h"

#include <math.h>

#include "base/bind.h"
#include "base/synchronization/waitable_event.h"

namespace media {

ReferenceAudioRenderer::ReferenceAudioRenderer(AudioManager* audio_manager)
    : AudioRendererBase(),
      audio_manager_(audio_manager),
      bytes_per_second_(0),
      has_buffered_data_(true),
      buffer_capacity_(0) {
}

ReferenceAudioRenderer::~ReferenceAudioRenderer() {
  // Close down the audio device.
  if (controller_) {
      base::WaitableEvent closed_event(true, false);
      controller_->Close(base::Bind(&base::WaitableEvent::Signal,
                                    base::Unretained(&closed_event)));
      closed_event.Wait();
  }
}

void ReferenceAudioRenderer::SetPlaybackRate(float rate) {
  // TODO(fbarchard): limit rate to reasonable values
  AudioRendererBase::SetPlaybackRate(rate);

  if (controller_ && rate > 0.0f)
    controller_->Play();
}

void ReferenceAudioRenderer::SetVolume(float volume) {
  if (controller_)
    controller_->SetVolume(volume);
}

void ReferenceAudioRenderer::OnCreated(AudioOutputController* controller) {
  NOTIMPLEMENTED();
}

void ReferenceAudioRenderer::OnPlaying(AudioOutputController* controller) {
  NOTIMPLEMENTED();
}

void ReferenceAudioRenderer::OnPaused(AudioOutputController* controller) {
  NOTIMPLEMENTED();
}

void ReferenceAudioRenderer::OnError(AudioOutputController* controller,
                                     int error_code) {
  NOTIMPLEMENTED();
}

void ReferenceAudioRenderer::OnMoreData(AudioOutputController* controller,
                                        AudioBuffersState buffers_state) {
  // TODO(fbarchard): Waveout_output_win.h should handle zero length buffers
  //                  without clicking.
  uint32 pending_bytes = static_cast<uint32>(ceil(buffers_state.total_bytes() *
                                                  GetPlaybackRate()));
  base::TimeDelta delay = base::TimeDelta::FromMicroseconds(
      base::Time::kMicrosecondsPerSecond * pending_bytes /
      bytes_per_second_);
  has_buffered_data_ = buffers_state.pending_bytes != 0;
  uint32 read = FillBuffer(buffer_.get(), buffer_capacity_, delay);
  controller->EnqueueData(buffer_.get(), read);
}

void ReferenceAudioRenderer::OnRenderEndOfStream() {
  // We cannot signal end of stream as long as we have buffered data.
  // In such case eventually host would playback all the data, and OnMoreData()
  // would be called with buffers_state.pending_bytes == 0. At that moment
  // we'll call SignalEndOfStream();
  if (!has_buffered_data_)
    SignalEndOfStream();
}

bool ReferenceAudioRenderer::OnInitialize(int bits_per_channel,
                                          ChannelLayout channel_layout,
                                          int sample_rate) {
  int samples_per_packet = sample_rate / 10;
  int hardware_buffer_size = samples_per_packet *
      ChannelLayoutToChannelCount(channel_layout) * bits_per_channel / 8;

  // Allocate audio buffer based on hardware buffer size.
  buffer_capacity_ = 3 * hardware_buffer_size;
  buffer_.reset(new uint8[buffer_capacity_]);

  AudioParameters params(AudioParameters::AUDIO_PCM_LINEAR, channel_layout,
                         sample_rate, bits_per_channel, samples_per_packet);
  bytes_per_second_ = params.GetBytesPerSecond();

  controller_ = AudioOutputController::Create(audio_manager_, this, params,
                                              buffer_capacity_);
  return controller_ != NULL;
}

void ReferenceAudioRenderer::OnStop() {
  if (controller_)
    controller_->Pause();
}

}  // namespace media
