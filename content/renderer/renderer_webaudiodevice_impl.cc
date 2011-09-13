// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/renderer_webaudiodevice_impl.h"

using WebKit::WebAudioDevice;
using WebKit::WebVector;

RendererWebAudioDeviceImpl::RendererWebAudioDeviceImpl(size_t buffer_size,
    int channels, double sample_rate, WebAudioDevice::RenderCallback* callback)
    : is_running_(false),
      client_callback_(callback) {
  audio_device_ = new AudioDevice(buffer_size, channels, sample_rate, this);
}

RendererWebAudioDeviceImpl::~RendererWebAudioDeviceImpl() {
  stop();
}

void RendererWebAudioDeviceImpl::start() {
  if (!is_running_) {
    audio_device_->Start();
    is_running_ = true;
  }
}

void RendererWebAudioDeviceImpl::stop() {
  if (is_running_) {
    if (audio_device_->Stop())
      is_running_ = false;
  }
}

double RendererWebAudioDeviceImpl::sampleRate() {
  return 44100.0;
}

void RendererWebAudioDeviceImpl::Render(const std::vector<float*>& audio_data,
                                size_t number_of_frames,
                                size_t audio_delay_milliseconds) {
  // Make the client callback to get rendered audio.
  DCHECK(client_callback_);
  if (client_callback_) {
    // Wrap the pointers using WebVector.
    WebVector<float*> web_audio_data(audio_data.size());
    for (size_t i = 0; i < audio_data.size(); ++i)
      web_audio_data[i] = audio_data[i];

    client_callback_->render(web_audio_data, number_of_frames);
  }
}
