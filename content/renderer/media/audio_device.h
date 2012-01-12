// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Low-latency audio rendering unit utilizing audio output stream provided
// by browser process through IPC.
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
// Start -> InitializeOnIOThread ------> AudioHostMsg_CreateStream -------->
//       <- OnLowLatencyCreated <- AudioMsg_NotifyLowLatencyStreamCreated <-
//       ---> PlayOnIOThread -----------> AudioHostMsg_PlayStream -------->
//
// Optionally Play() / Pause() sequences may occur:
// Play -> PlayOnIOThread --------------> AudioHostMsg_PlayStream --------->
// Pause -> PauseOnIOThread ------------> AudioHostMsg_PauseStream -------->
// (note that Play() / Pause() sequences before OnLowLatencyCreated are
//  deferred until OnLowLatencyCreated, with the last valid state being used)
//
// AudioDevice::Render => audio transport on audio thread with low latency =>
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
// 4. Audio transport thread.
//    Responsible for calling the RenderCallback and feeding audio samples to
//    the audio layer in the browser process using sync sockets and shared
//    memory.
//
// Implementation notes:
//
// - Start() is asynchronous/non-blocking.
// - Stop() is synchronous/blocking.
// - Play() is asynchronous/non-blocking.
// - Pause() is asynchronous/non-blocking.
// - The user must call Stop() before deleting the class instance.

#ifndef CONTENT_RENDERER_MEDIA_AUDIO_DEVICE_H_
#define CONTENT_RENDERER_MEDIA_AUDIO_DEVICE_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/shared_memory.h"
#include "base/synchronization/lock.h"
#include "base/threading/simple_thread.h"
#include "content/common/content_export.h"
#include "content/renderer/media/audio_message_filter.h"
#include "media/audio/audio_parameters.h"
#include "media/base/audio_renderer_sink.h"

class CONTENT_EXPORT AudioDevice
    : NON_EXPORTED_BASE(public media::AudioRendererSink),
      public AudioMessageFilter::Delegate,
      public base::DelegateSimpleThread::Delegate {
 public:
  // Methods called on main render thread -------------------------------------

  // Minimal constructor where Initialize() must be called later.
  AudioDevice();

  AudioDevice(size_t buffer_size,
              int channels,
              double sample_rate,
              RenderCallback* callback);

  // AudioRendererSink implementation.

  virtual void Initialize(size_t buffer_size,
                          int channels,
                          double sample_rate,
                          AudioParameters::Format latency_format,
                          RenderCallback* callback) OVERRIDE;
  // Starts audio playback.
  virtual void Start() OVERRIDE;

  // Stops audio playback.
  virtual void Stop() OVERRIDE;

  // Resumes playback if currently paused.
  // TODO(crogers): it should be possible to remove the extra complexity
  // of Play() and Pause() with additional re-factoring work in
  // AudioRendererImpl.
  virtual void Play() OVERRIDE;

  // Pauses playback.
  // If |flush| is true then any pending audio that is in the pipeline
  // (has not yet reached the hardware) will be discarded.  In this case,
  // when Play() is later called, no previous pending audio will be
  // rendered.
  virtual void Pause(bool flush) OVERRIDE;

  // Sets the playback volume, with range [0.0, 1.0] inclusive.
  // Returns |true| on success.
  virtual bool SetVolume(double volume) OVERRIDE;

  // Gets the playback volume, with range [0.0, 1.0] inclusive.
  virtual void GetVolume(double* volume) OVERRIDE;

  double sample_rate() const { return sample_rate_; }
  size_t buffer_size() const { return buffer_size_; }

  // Methods called on IO thread ----------------------------------------------
  // AudioMessageFilter::Delegate methods, called by AudioMessageFilter.
  virtual void OnRequestPacket(AudioBuffersState buffers_state) OVERRIDE;
  virtual void OnStateChanged(AudioStreamState state) OVERRIDE;
  virtual void OnCreated(base::SharedMemoryHandle handle,
                         uint32 length) OVERRIDE;
  virtual void OnLowLatencyCreated(base::SharedMemoryHandle handle,
                                   base::SyncSocket::Handle socket_handle,
                                   uint32 length) OVERRIDE;
  virtual void OnVolume(double volume) OVERRIDE;

 private:
  // Encapsulate socket into separate class to avoid double-close.
  // Class is ref-counted to avoid potential race condition if audio device
  // is deleted simultaneously with audio thread stopping.
  class AudioSocket : public base::RefCountedThreadSafe<AudioSocket> {
   public:
    explicit AudioSocket(base::SyncSocket::Handle socket_handle)
        : socket_(socket_handle) {
    }
    base::SyncSocket* socket() {
      return &socket_;
    }
    void Close() {
      // Close() should be thread-safe, obtain the lock.
      base::AutoLock auto_lock(lock_);
      socket_.Close();
    }

   private:
    // Magic required by ref_counted.h to avoid any code deleting the object
    // accidently while there are references to it.
    friend class base::RefCountedThreadSafe<AudioSocket>;
    ~AudioSocket() { }

    base::Lock lock_;
    base::SyncSocket socket_;
  };

  // Magic required by ref_counted.h to avoid any code deleting the object
  // accidently while there are references to it.
  friend class base::RefCountedThreadSafe<AudioDevice>;
  virtual ~AudioDevice();

  // Methods called on IO thread ----------------------------------------------
  // The following methods are tasks posted on the IO thread that needs to
  // be executed on that thread. They interact with AudioMessageFilter and
  // sends IPC messages on that thread.
  void InitializeOnIOThread(const AudioParameters& params);
  void PlayOnIOThread();
  void PauseOnIOThread(bool flush);
  void ShutDownOnIOThread(base::WaitableEvent* completion);
  void SetVolumeOnIOThread(double volume);

  void Send(IPC::Message* message);

  // Method called on the audio thread ----------------------------------------
  // Calls the client's callback for rendering audio.
  // Returns actual number of filled frames that callback returned. This length
  // is passed to host at the end of the shared memory (i.e. buffer). In case of
  // continuous stream host just ignores it and assumes buffer is always filled
  // to its capacity.
  size_t FireRenderCallback(int16* data);

  // DelegateSimpleThread::Delegate implementation.
  virtual void Run() OVERRIDE;

  // Closes socket and joins with the audio thread.
  void ShutDownAudioThread();

  // Format
  size_t buffer_size_;  // in sample-frames
  int channels_;
  int bits_per_sample_;
  double sample_rate_;
  AudioParameters::Format latency_format_;

  RenderCallback* callback_;

  // The client callback renders audio into here.
  std::vector<float*> audio_data_;

  // Set to |true| once Initialize() has been called.
  bool is_initialized_;

  // The client stores the last reported audio delay in this member.
  // The delay shall reflect the amount of audio which still resides in
  // the output buffer, i.e., the expected audio output delay.
  int audio_delay_milliseconds_;

  // The current volume scaling [0.0, 1.0] of the audio stream.
  double volume_;

  // Callbacks for rendering audio occur on this thread.
  scoped_ptr<base::DelegateSimpleThread> audio_thread_;

  // Cached audio message filter (lives on the main render thread).
  scoped_refptr<AudioMessageFilter> filter_;

  // Our stream ID on the message filter. Only accessed on the IO thread.
  int32 stream_id_;

  // State of Play() / Pause() calls before OnLowLatencyCreated() is called.
  bool play_on_start_;

  // Set to |true| when OnLowLatencyCreated() is called.
  // Set to |false| when ShutDownOnIOThread() is called.
  // This is for use with play_on_start_ to track Play() / Pause() state.
  bool is_started_;

  // Data transfer between browser and render process uses a combination
  // of sync sockets and shared memory to provide lowest possible latency.
  base::SharedMemoryHandle shared_memory_handle_;
  scoped_refptr<AudioSocket> audio_socket_;
  int memory_length_;

  // Protects lifetime of:
  // audio_socket_
  // audio_thread_
  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(AudioDevice);
};

#endif  // CONTENT_RENDERER_MEDIA_AUDIO_DEVICE_H_
