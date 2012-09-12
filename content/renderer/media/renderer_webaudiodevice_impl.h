// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_RENDERER_WEBAUDIODEVICE_IMPL_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_RENDERER_WEBAUDIODEVICE_IMPL_H_

#include "base/memory/ref_counted.h"
#include "media/base/audio_renderer_sink.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebAudioDevice.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebVector.h"

class RendererWebAudioDeviceImpl
    : public WebKit::WebAudioDevice,
      public media::AudioRendererSink::RenderCallback {
 public:
  RendererWebAudioDeviceImpl(const media::AudioParameters& params,
                             WebKit::WebAudioDevice::RenderCallback* callback);
  virtual ~RendererWebAudioDeviceImpl();

  // WebKit::WebAudioDevice implementation.
  virtual void start();
  virtual void stop();
  virtual double sampleRate();

  // AudioRendererSink::RenderCallback implementation.
  virtual int Render(media::AudioBus* dest,
                     int audio_delay_milliseconds) OVERRIDE;

  virtual void RenderIO(media::AudioBus* source,
                        media::AudioBus* dest,
                        int audio_delay_milliseconds) OVERRIDE;

  virtual void OnRenderError() OVERRIDE;

 private:
  scoped_refptr<media::AudioRendererSink> audio_device_;
  bool is_running_;

  // Weak reference to the callback into WebKit code.
  WebKit::WebAudioDevice::RenderCallback* client_callback_;

  DISALLOW_COPY_AND_ASSIGN(RendererWebAudioDeviceImpl);
};

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_RENDERER_WEBAUDIODEVICE_IMPL_H_
