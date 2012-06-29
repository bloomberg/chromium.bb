// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// RenderAudioSourceProvider provides a bridge between classes:
//     WebKit::WebAudioSourceProvider <---> media::AudioRendererSink
//
// RenderAudioSourceProvider is a "sink" of audio, and uses a default
// AudioDevice if a client has not explicitly been set.
//
// WebKit optionally sets a client, and then periodically calls provideInput()
// to render a certain number of audio sample-frames.  provideInput()
// uses the renderer to get this data, and then massages it into the form
// required by provideInput().  In this case, the default AudioDevice
// is no longer used.
//
// THREAD SAFETY:
// It is assumed that the callers to setClient() and provideInput()
// implement appropriate locking for thread safety when making
// these calls.  This happens in WebKit.

#ifndef CONTENT_RENDERER_MEDIA_RENDER_AUDIOSOURCEPROVIDER_H_
#define CONTENT_RENDERER_MEDIA_RENDER_AUDIOSOURCEPROVIDER_H_

#include <vector>

#include "base/synchronization/lock.h"
#include "media/base/audio_renderer_sink.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebVector.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebAudioSourceProvider.h"

namespace WebKit {
class WebAudioSourceProviderClient;
}

class RenderAudioSourceProvider
    : public WebKit::WebAudioSourceProvider,
      public media::AudioRendererSink {
 public:
  RenderAudioSourceProvider();

  // WebKit::WebAudioSourceProvider implementation.

  // WebKit calls setClient() if it desires to take control of the rendered
  // audio stream.  We call client's setFormat() when the audio stream format
  // is known.
  virtual void setClient(WebKit::WebAudioSourceProviderClient* client);

  // If setClient() has been called, then WebKit calls provideInput()
  // periodically to get the rendered audio stream.
  virtual void provideInput(const WebKit::WebVector<float*>& audio_data,
                            size_t number_of_frames);

  // AudioRendererSink implementation.
  virtual void Start() OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void Play() OVERRIDE;
  virtual void Pause(bool flush) OVERRIDE;
  virtual void SetPlaybackRate(float rate) OVERRIDE;
  virtual bool SetVolume(double volume) OVERRIDE;
  virtual void GetVolume(double* volume) OVERRIDE;
  virtual void Initialize(const media::AudioParameters& params,
                          RenderCallback* renderer) OVERRIDE;

 protected:
  virtual ~RenderAudioSourceProvider();

 private:
  // Set to true when Initialize() is called.
  bool is_initialized_;
  int channels_;
  int sample_rate_;

  bool is_running_;
  double volume_;
  media::AudioRendererSink::RenderCallback* renderer_;
  WebKit::WebAudioSourceProviderClient* client_;

  // Protects access to sink_
  base::Lock sink_lock_;

  // default_sink_ is the default sink.
  scoped_refptr<media::AudioRendererSink> default_sink_;

  DISALLOW_COPY_AND_ASSIGN(RenderAudioSourceProvider);
};

#endif  // CONTENT_RENDERER_MEDIA_RENDER_AUDIOSOURCEPROVIDER_H_
