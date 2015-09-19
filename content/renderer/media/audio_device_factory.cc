// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/audio_device_factory.h"

#include "base/logging.h"
#include "content/renderer/media/audio_input_message_filter.h"
#include "content/renderer/media/audio_message_filter.h"
#include "media/audio/audio_input_device.h"
#include "media/audio/audio_output_device.h"
#include "url/origin.h"

namespace content {

// static
AudioDeviceFactory* AudioDeviceFactory::factory_ = NULL;

// static
scoped_refptr<media::AudioOutputDevice> AudioDeviceFactory::NewOutputDevice(
    int render_frame_id,
    int session_id,
    const std::string& device_id,
    const url::Origin& security_origin) {
  if (factory_) {
    media::AudioOutputDevice* const device = factory_->CreateOutputDevice(
        render_frame_id, session_id, device_id, security_origin);
    if (device)
      return device;
  }

  AudioMessageFilter* const filter = AudioMessageFilter::Get();
  scoped_refptr<media::AudioOutputDevice> device = new media::AudioOutputDevice(
      filter->CreateAudioOutputIPC(render_frame_id), filter->io_task_runner(),
      session_id, device_id, security_origin);
  device->RequestDeviceAuthorization();
  return device;
}

// static
scoped_refptr<media::AudioInputDevice> AudioDeviceFactory::NewInputDevice(
    int render_frame_id) {
  if (factory_) {
    media::AudioInputDevice* const device =
        factory_->CreateInputDevice(render_frame_id);
    if (device)
      return device;
  }

  AudioInputMessageFilter* const filter = AudioInputMessageFilter::Get();
  return new media::AudioInputDevice(
      filter->CreateAudioInputIPC(render_frame_id), filter->io_task_runner());
}

AudioDeviceFactory::AudioDeviceFactory() {
  DCHECK(!factory_) << "Can't register two factories at once.";
  factory_ = this;
}

AudioDeviceFactory::~AudioDeviceFactory() {
  factory_ = NULL;
}

}  // namespace content
