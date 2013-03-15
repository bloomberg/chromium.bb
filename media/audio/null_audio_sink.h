// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_NULL_AUDIO_SINK_H_
#define MEDIA_AUDIO_NULL_AUDIO_SINK_H_

#include "base/md5.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/audio_renderer_sink.h"

namespace base {
class MessageLoopProxy;
}

namespace media {
class AudioBus;
class FakeAudioConsumer;

class MEDIA_EXPORT NullAudioSink
    : NON_EXPORTED_BASE(public AudioRendererSink) {
 public:
  NullAudioSink(const scoped_refptr<base::MessageLoopProxy>& message_loop);

  // AudioRendererSink implementation.
  virtual void Initialize(const AudioParameters& params,
                          RenderCallback* callback) OVERRIDE;
  virtual void Start() OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void Pause(bool flush) OVERRIDE;
  virtual void Play() OVERRIDE;
  virtual bool SetVolume(double volume) OVERRIDE;

  // Enables audio frame hashing and reinitializes the MD5 context.  Must be
  // called prior to Initialize().
  void StartAudioHashForTesting();

  // Returns the MD5 hash of all audio frames seen since the last reset.
  std::string GetAudioHashForTesting();

 protected:
  virtual ~NullAudioSink();

 private:
  // Task that periodically calls Render() to consume audio data.
  void CallRender(AudioBus* audio_bus);

  bool initialized_;
  bool playing_;
  RenderCallback* callback_;

  // Controls whether or not a running MD5 hash is computed for audio frames.
  bool hash_audio_for_testing_;
  int channels_;
  scoped_array<base::MD5Context> md5_channel_contexts_;

  scoped_refptr<base::MessageLoopProxy> message_loop_;
  scoped_ptr<FakeAudioConsumer> fake_consumer_;

  DISALLOW_COPY_AND_ASSIGN(NullAudioSink);
};

}  // namespace media

#endif  // MEDIA_AUDIO_NULL_AUDIO_SINK_H_
