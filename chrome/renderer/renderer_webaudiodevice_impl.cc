// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/renderer_webaudiodevice_impl.h"

using WebKit::WebAudioDevice;
using WebKit::WebVector;

RendererWebAudioDeviceImpl::RendererWebAudioDeviceImpl(size_t buffer_size,
    int channels, double sample_rate, WebAudioDevice::RenderCallback* callback)
    : client_callback_(callback) {
  audio_device_.reset(
      new AudioDevice(buffer_size, channels, sample_rate, this));
}

RendererWebAudioDeviceImpl::~RendererWebAudioDeviceImpl() {
  stop();
}

void RendererWebAudioDeviceImpl::start() {
  audio_device_->Start();
}

void RendererWebAudioDeviceImpl::stop() {
  audio_device_->Stop();
}

void RendererWebAudioDeviceImpl::Render(const std::vector<float*>& audio_data,
                                size_t number_of_frames) {
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
