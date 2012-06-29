// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Audio rendering unit utilizing audio output stream provided by browser
// process through IPC.
//
// Relationship of classes.
//
//  AudioOutputController                AudioDevice
//           ^                                ^
//           |                                |
//           v               IPC              v
//    AudioRendererHost  <---------> AudioMessageFilter
//
// Transportation of audio samples from the render to the browser process
// is done by using shared memory in combination with a sync socket pair
// to generate a low latency transport. The AudioDevice user registers an
// AudioDevice::RenderCallback at construction and will be polled by the
// AudioDevice for audio to be played out by the underlying audio layers.
//
// State sequences.
//
//            Task [IO thread]                  IPC [IO thread]
//
// Start -> CreateStreamOnIOThread -----> AudioHostMsg_CreateStream ------>
//       <- OnStreamCreated <- AudioMsg_NotifyStreamCreated <-
//       ---> PlayOnIOThread -----------> AudioHostMsg_PlayStream -------->
//
// Optionally Play() / Pause() sequences may occur:
// Play -> PlayOnIOThread --------------> AudioHostMsg_PlayStream --------->
// Pause -> PauseOnIOThread ------------> AudioHostMsg_PauseStream -------->
// (note that Play() / Pause() sequences before OnStreamCreated are
//  deferred until OnStreamCreated, with the last valid state being used)
//
// AudioDevice::Render => audio transport on audio thread =>
//                               |
// Stop --> ShutDownOnIOThread -------->  AudioHostMsg_CloseStream -> Close
//
// This class utilizes several threads during its lifetime, namely:
// 1. Creating thread.
//    Must be the main render thread.
// 2. Control thread (may be the main render thread or another thread).
//    The methods: Start(), Stop(), Play(), Pause(), SetVolume()
//    must be called on the same thread.
// 3. IO thread (internal implementation detail - not exposed to public API)
//    The thread within which this class receives all the IPC messages and
//    IPC communications can only happen in this thread.
// 4. Audio transport thread (See AudioDeviceThread).
//    Responsible for calling the AudioThreadCallback implementation that in
//    turn calls AudioRendererSink::RenderCallback which feeds audio samples to
//    the audio layer in the browser process using sync sockets and shared
//    memory.
//
// Implementation notes:
//
// - Start() is asynchronous/non-blocking.
// - Stop() is asynchronous/non-blocking.
// - Play() is asynchronous/non-blocking.
// - Pause() is asynchronous/non-blocking.
// - The user must call Stop() before deleting the class instance.

#ifndef CONTENT_RENDERER_MEDIA_AUDIO_DEVICE_H_
#define CONTENT_RENDERER_MEDIA_AUDIO_DEVICE_H_
#pragma once

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/shared_memory.h"
#include "content/common/content_export.h"
#include "content/renderer/media/audio_device_thread.h"
#include "content/renderer/media/audio_message_filter.h"
#include "content/renderer/media/scoped_loop_observer.h"
#include "media/audio/audio_parameters.h"
#include "media/base/audio_renderer_sink.h"

namespace media {
class AudioParameters;
}

class CONTENT_EXPORT AudioDevice
    : NON_EXPORTED_BASE(public media::AudioRendererSink),
      public AudioMessageFilter::Delegate,
      NON_EXPORTED_BASE(public ScopedLoopObserver) {
 public:
  // Methods called on main render thread -------------------------------------

  // Creates an uninitialized AudioDevice. Clients must call Initialize()
  // before using.
  AudioDevice();

  // AudioRendererSink implementation.
  virtual void Initialize(const media::AudioParameters& params,
                          RenderCallback* callback) OVERRIDE;
  virtual void Start() OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void Play() OVERRIDE;
  virtual void Pause(bool flush) OVERRIDE;
  virtual bool SetVolume(double volume) OVERRIDE;
  virtual void GetVolume(double* volume) OVERRIDE;

  // Methods called on IO thread ----------------------------------------------
  // AudioMessageFilter::Delegate methods, called by AudioMessageFilter.
  virtual void OnStateChanged(AudioStreamState state) OVERRIDE;
  virtual void OnStreamCreated(base::SharedMemoryHandle handle,
                               base::SyncSocket::Handle socket_handle,
                               uint32 length) OVERRIDE;

 private:
  // Magic required by ref_counted.h to avoid any code deleting the object
  // accidentally while there are references to it.
  friend class base::RefCountedThreadSafe<AudioDevice>;
  virtual ~AudioDevice();

  // Methods called on IO thread ----------------------------------------------
  // The following methods are tasks posted on the IO thread that needs to
  // be executed on that thread. They interact with AudioMessageFilter and
  // sends IPC messages on that thread.
  void CreateStreamOnIOThread(const media::AudioParameters& params);
  void PlayOnIOThread();
  void PauseOnIOThread(bool flush);
  void ShutDownOnIOThread();
  void SetVolumeOnIOThread(double volume);

  void Send(IPC::Message* message);

  // MessageLoop::DestructionObserver implementation for the IO loop.
  // If the IO loop dies before we do, we shut down the audio thread from here.
  virtual void WillDestroyCurrentMessageLoop() OVERRIDE;

  media::AudioParameters audio_parameters_;

  RenderCallback* callback_;

  // The current volume scaling [0.0, 1.0] of the audio stream.
  double volume_;

  // Cached audio message filter (lives on the main render thread).
  scoped_refptr<AudioMessageFilter> filter_;

  // Our stream ID on the message filter. Only accessed on the IO thread.
  // Must only be modified on the IO thread.
  int32 stream_id_;

  // State of Play() / Pause() calls before OnStreamCreated() is called.
  bool play_on_start_;

  // Set to |true| when OnStreamCreated() is called.
  // Set to |false| when ShutDownOnIOThread() is called.
  // This is for use with play_on_start_ to track Play() / Pause() state.
  // Must only be touched from the IO thread.
  bool is_started_;

  // Our audio thread callback class.  See source file for details.
  class AudioThreadCallback;

  // In order to avoid a race between OnStreamCreated and Stop(), we use this
  // guard to control stopping and starting the audio thread.
  base::Lock audio_thread_lock_;
  AudioDeviceThread audio_thread_;
  scoped_ptr<AudioDevice::AudioThreadCallback> audio_callback_;


  DISALLOW_COPY_AND_ASSIGN(AudioDevice);
};

#endif  // CONTENT_RENDERER_MEDIA_AUDIO_DEVICE_H_
