// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_AUDIO_DEVICE_FACTORY_H_
#define CONTENT_RENDERER_MEDIA_AUDIO_DEVICE_FACTORY_H_
#pragma once

#include "base/basictypes.h"
#include "content/common/content_export.h"

namespace media {
class AudioRendererSink;
}

namespace content {

// A factory for creating AudioRendererSinks. There is a global factory
// function that can be installed for the purposes of testing to provide
// a specialized AudioRendererSink class.
// This class uses the same pattern as content::RenderViewHostFactory.
class CONTENT_EXPORT AudioDeviceFactory {
 public:
  // Creates an AudioRendererSink using the currently registered factory,
  // or the default one if no factory is registered. Ownership of the returned
  // pointer will be passed to the caller.
 static media::AudioRendererSink* Create();

 protected:
  AudioDeviceFactory();
  virtual ~AudioDeviceFactory();

  // You can derive from this class and specify an implementation for this
  // function to create a different kind of AudioRendererSink for testing.
  virtual media::AudioRendererSink* CreateAudioDevice() = 0;

 private:
  // The current globally registered factory. This is NULL when we should
  // create the default AudioRendererSinks.
  static AudioDeviceFactory* factory_;

  DISALLOW_COPY_AND_ASSIGN(AudioDeviceFactory);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_AUDIO_DEVICE_FACTORY_H_
