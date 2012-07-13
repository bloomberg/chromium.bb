// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/audio_device_factory.h"

#include "base/logging.h"
#include "content/common/child_process.h"
#include "content/renderer/media/audio_device.h"

namespace content {

// static
AudioDeviceFactory* AudioDeviceFactory::factory_ = NULL;

// static
media::AudioRendererSink* AudioDeviceFactory::Create() {
  if (factory_) {
    return factory_->CreateAudioDevice();
  }
  return new AudioDevice(
      ChildProcess::current()->io_message_loop()->message_loop_proxy());
}

AudioDeviceFactory::AudioDeviceFactory() {
  DCHECK(!factory_) << "Can't register two factories at once.";
  factory_ = this;
}

AudioDeviceFactory::~AudioDeviceFactory() {
  factory_ = NULL;
}

}  // namespace content
