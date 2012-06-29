// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/renderer_webaudiodevice_impl.h"

#include "base/logging.h"
#include "content/renderer/media/audio_device_factory.h"

using content::AudioDeviceFactory;
using WebKit::WebAudioDevice;
using WebKit::WebVector;

RendererWebAudioDeviceImpl::RendererWebAudioDeviceImpl(
    const media::AudioParameters& params,
    WebAudioDevice::RenderCallback* callback)
    : is_running_(false),
      client_callback_(callback) {
  audio_device_ = AudioDeviceFactory::Create();
  audio_device_->Initialize(params, this);
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
    audio_device_->Stop();
    is_running_ = false;
  }
}

double RendererWebAudioDeviceImpl::sampleRate() {
  return 44100.0;
}

int RendererWebAudioDeviceImpl::Render(const std::vector<float*>& audio_data,
                                       int number_of_frames,
                                       int audio_delay_milliseconds) {
  // Make the client callback to get rendered audio.
  DCHECK(client_callback_);
  if (client_callback_) {
    // Wrap the pointers using WebVector.
    WebVector<float*> web_audio_data(audio_data.size());
    for (size_t i = 0; i < audio_data.size(); ++i)
      web_audio_data[i] = audio_data[i];

    client_callback_->render(web_audio_data, number_of_frames);
  }
  return number_of_frames;
}

void RendererWebAudioDeviceImpl::OnRenderError() {
  // TODO(crogers): implement error handling.
}
