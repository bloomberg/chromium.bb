// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_OUTPUT_STREAM_SINK_H_
#define MEDIA_AUDIO_AUDIO_OUTPUT_STREAM_SINK_H_

#include "base/compiler_specific.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "media/audio/audio_io.h"
#include "media/base/audio_renderer_sink.h"
#include "media/base/media_export.h"

namespace media {

// Wrapper which exposes the browser side audio interface (AudioOutputStream) as
// if it were a renderer side audio interface (AudioRendererSink). Note: This
// will not work for sandboxed renderers.
//
// TODO(dalecurtis): Delete this class once we have a proper mojo audio service;
// tracked by http://crbug.com/425368
class MEDIA_EXPORT AudioOutputStreamSink
    : NON_EXPORTED_BASE(public AudioRendererSink),
      public AudioOutputStream::AudioSourceCallback {
 public:
  AudioOutputStreamSink();

  // AudioRendererSink implementation.
  void Initialize(const AudioParameters& params,
                  RenderCallback* callback) override;
  void Start() override;
  void Stop() override;
  void Pause() override;
  void Play() override;
  bool SetVolume(double volume) override;

  // AudioSourceCallback implementation.
  int OnMoreData(AudioBus* dest, uint32 total_bytes_delay) override;
  void OnError(AudioOutputStream* stream) override;

 private:
  ~AudioOutputStreamSink() override;

  // Helper methods for running AudioManager methods on the audio thread.
  void DoStart();
  void DoStop();
  void DoPause();
  void DoPlay();
  void DoSetVolume(double volume);

  // Clears |active_render_callback_| under lock, synchronously stopping render
  // callbacks from any thread.  Must be called before Pause() and Stop()
  // trampoline to their helper methods on the audio thread.
  void ClearCallback();

  // Parameters provided by Initialize(), cached for use on other threads.
  AudioParameters params_;

  // Since Initialize() is only called once for AudioRenderSinks, save the
  // callback both here and under |active_render_callback_| which will be
  // cleared during Pause() and Stop() to achieve "synchronous" stoppage.
  RenderCallback* render_callback_;

  // The task runner for the audio thread.
  const scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner_;

  // The actual AudioOutputStream, must only be accessed on the audio thread.
  AudioOutputStream* stream_;

  // Lock and callback for forwarding OnMoreData() calls into Render() calls.
  base::Lock callback_lock_;
  RenderCallback* active_render_callback_;

  DISALLOW_COPY_AND_ASSIGN(AudioOutputStreamSink);
};

}  // namepace media

#endif  // MEDIA_AUDIO_AUDIO_OUTPUT_STREAM_SINK_H_
