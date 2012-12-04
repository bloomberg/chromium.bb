// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_AUDIO_DEVICE_FACTORY_H_
#define CONTENT_RENDERER_MEDIA_AUDIO_DEVICE_FACTORY_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"

namespace media {
class AudioInputDevice;
}

namespace content {

class RendererAudioOutputDevice;

// A factory for creating RendererAudioOutputDevices.  There is a global factory
// function that can be installed for the purposes of testing to provide
// a specialized AudioRendererSink class.
class AudioDeviceFactory {
 public:
  // Creates a RendererAudioOutputDevice using the currently registered factory.
  static scoped_refptr<RendererAudioOutputDevice> NewOutputDevice();

  // Creates an AudioInputDevice using the currently registered factory,
  static scoped_refptr<media::AudioInputDevice> NewInputDevice();

 protected:
  AudioDeviceFactory();
  virtual ~AudioDeviceFactory();

  // You can derive from this class and specify an implementation for these
  // functions to provide alternate audio device implementations.
  // If the return value of either of these function is NULL, we fall back
  // on the default implementation.
  virtual RendererAudioOutputDevice* CreateOutputDevice() = 0;
  virtual media::AudioInputDevice* CreateInputDevice() = 0;

 private:
  // The current globally registered factory. This is NULL when we should
  // create the default AudioRendererSinks.
  static AudioDeviceFactory* factory_;

  DISALLOW_COPY_AND_ASSIGN(AudioDeviceFactory);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_AUDIO_DEVICE_FACTORY_H_
