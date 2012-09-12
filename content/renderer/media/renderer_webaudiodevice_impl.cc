// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/renderer_webaudiodevice_impl.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "content/renderer/media/audio_device_factory.h"
#include "media/base/media_switches.h"

using content::AudioDeviceFactory;
using WebKit::WebAudioDevice;
using WebKit::WebVector;

RendererWebAudioDeviceImpl::RendererWebAudioDeviceImpl(
    const media::AudioParameters& params,
    WebAudioDevice::RenderCallback* callback)
    : is_running_(false),
      client_callback_(callback) {
  audio_device_ = AudioDeviceFactory::NewOutputDevice();

  // TODO(crogers): remove once we properly handle input device selection.
  // https://code.google.com/p/chromium/issues/detail?id=147327
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableWebAudioInput)) {
    // TODO(crogers): support more than hard-coded stereo:
    // https://code.google.com/p/chromium/issues/detail?id=147326
    audio_device_->InitializeIO(params, 2, this);
  } else {
    audio_device_->Initialize(params, this);
  }
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

int RendererWebAudioDeviceImpl::Render(media::AudioBus* dest,
                                       int audio_delay_milliseconds) {
  RenderIO(NULL, dest, audio_delay_milliseconds);
  return dest->frames();
}

void RendererWebAudioDeviceImpl::RenderIO(media::AudioBus* source,
                                          media::AudioBus* dest,
                                          int audio_delay_milliseconds) {
  // Make the client callback for an I/O cycle.
  DCHECK(client_callback_);
  if (client_callback_) {
    // Wrap the input pointers using WebVector.
    size_t input_channels =
        source ? static_cast<size_t>(source->channels()) : 0;
    WebVector<float*> web_audio_input_data(input_channels);
    for (size_t i = 0; i < input_channels; ++i)
      web_audio_input_data[i] = source->channel(i);

    // Wrap the output pointers using WebVector.
    WebVector<float*> web_audio_data(
        static_cast<size_t>(dest->channels()));
    for (int i = 0; i < dest->channels(); ++i)
      web_audio_data[i] = dest->channel(i);

    client_callback_->render(web_audio_input_data,
                             web_audio_data,
                             dest->frames());
  }
}

void RendererWebAudioDeviceImpl::OnRenderError() {
  // TODO(crogers): implement error handling.
}
