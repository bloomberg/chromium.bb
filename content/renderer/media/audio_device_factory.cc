// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/audio_device_factory.h"

#include "base/logging.h"
#include "content/renderer/media/audio_input_message_filter.h"
#include "content/renderer/media/audio_message_filter.h"
#include "content/renderer/media/renderer_audio_output_device.h"
#include "media/audio/audio_input_device.h"

namespace content {

// static
AudioDeviceFactory* AudioDeviceFactory::factory_ = NULL;

// static
scoped_refptr<RendererAudioOutputDevice> AudioDeviceFactory::NewOutputDevice() {
  RendererAudioOutputDevice* device = NULL;
  if (factory_)
    device = factory_->CreateOutputDevice();

  return device ? device : new RendererAudioOutputDevice(
      AudioMessageFilter::Get(), AudioMessageFilter::Get()->io_message_loop());
}

// static
scoped_refptr<media::AudioInputDevice> AudioDeviceFactory::NewInputDevice() {
  media::AudioInputDevice* device = NULL;
  if (factory_)
    device = factory_->CreateInputDevice();

  return device ? device : new media::AudioInputDevice(
      AudioInputMessageFilter::Get(),
      AudioInputMessageFilter::Get()->io_message_loop());
}

AudioDeviceFactory::AudioDeviceFactory() {
  DCHECK(!factory_) << "Can't register two factories at once.";
  factory_ = this;
}

AudioDeviceFactory::~AudioDeviceFactory() {
  factory_ = NULL;
}

}  // namespace content
