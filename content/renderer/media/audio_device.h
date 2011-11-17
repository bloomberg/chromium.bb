// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
//       ---> StartOnIOThread -----------> AudioHostMsg_PlayStream -------->
//
// AudioDevice::Render => audio transport on audio thread with low latency =>
//                               |
// Stop --> ShutDownOnIOThread -------->  AudioHostMsg_CloseStream -> Close
//
// This class utilizes three threads during its lifetime, namely:
// 1. Creating thread.
//    Must be the main render thread. Start and Stop should be called on
//    this thread.
// 2. IO thread.
//    The thread within which this class receives all the IPC messages and
//    IPC communications can only happen in this thread.
// 3. Audio transport thread.
//    Responsible for calling the RenderCallback and feed audio samples to
//    the audio layer in the browser process using sync sockets and shared
//    memory.
//
// Implementation notes:
//
// - Start() is asynchronous/non-blocking.
// - Stop() is synchronous/blocking.
// - The user must call Stop() before deleting the class instance.

#ifndef CONTENT_RENDERER_MEDIA_AUDIO_DEVICE_H_
#define CONTENT_RENDERER_MEDIA_AUDIO_DEVICE_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/shared_memory.h"
#include "base/threading/simple_thread.h"
#include "content/common/content_export.h"
#include "content/renderer/media/audio_message_filter.h"

struct AudioParameters;

class CONTENT_EXPORT AudioDevice
    : public AudioMessageFilter::Delegate,
      public base::DelegateSimpleThread::Delegate,
      public base::RefCountedThreadSafe<AudioDevice> {
 public:
  class RenderCallback {
   public:
    virtual void Render(const std::vector<float*>& audio_data,
                        size_t number_of_frames,
                        size_t audio_delay_milliseconds) = 0;
   protected:
    virtual ~RenderCallback() {}
  };

  // Methods called on main render thread -------------------------------------
  AudioDevice(size_t buffer_size,
              int channels,
              double sample_rate,
              RenderCallback* callback);
  virtual ~AudioDevice();

  // Starts audio playback.
  void Start();

  // Stops audio playback. Returns |true| on success.
  bool Stop();

  // Sets the playback volume, with range [0.0, 1.0] inclusive.
  // Returns |true| on success.
  bool SetVolume(double volume);

  // Gets the playback volume, with range [0.0, 1.0] inclusive.
  void GetVolume(double* volume);

  double sample_rate() const { return sample_rate_; }
  size_t buffer_size() const { return buffer_size_; }

  static double GetAudioHardwareSampleRate();
  static size_t GetAudioHardwareBufferSize();

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
  // Methods called on IO thread ----------------------------------------------
  // The following methods are tasks posted on the IO thread that needs to
  // be executed on that thread. They interact with AudioMessageFilter and
  // sends IPC messages on that thread.
  void InitializeOnIOThread(const AudioParameters& params);
  void StartOnIOThread();
  void ShutDownOnIOThread(base::WaitableEvent* completion);
  void SetVolumeOnIOThread(double volume);

  void Send(IPC::Message* message);

  // Method called on the audio thread (+ one call on the IO thread) ----------
  // Calls the client's callback for rendering audio. There will also be one
  // initial call on the IO thread before the audio thread has been created.
  void FireRenderCallback();

  // DelegateSimpleThread::Delegate implementation.
  virtual void Run() OVERRIDE;

  // Format
  size_t buffer_size_;  // in sample-frames
  int channels_;
  int bits_per_sample_;
  double sample_rate_;

  RenderCallback* callback_;

  // The client callback renders audio into here.
  std::vector<float*> audio_data_;

  // The client stores the last reported audio delay in this member.
  // The delay shall reflect the amount of audio which still resides in
  // the output buffer, i.e., the expected audio output delay.
  int audio_delay_milliseconds_;

  // The current volume scaling [0.0, 1.0] of the audio stream.
  double volume_;

  // Callbacks for rendering audio occur on this thread.
  scoped_ptr<base::DelegateSimpleThread> audio_thread_;

  // IPC message stuff.
  base::SharedMemory* shared_memory() { return shared_memory_.get(); }
  base::SyncSocket* socket() { return socket_.get(); }
  void* shared_memory_data() { return shared_memory()->memory(); }

  // Cached audio message filter (lives on the main render thread).
  scoped_refptr<AudioMessageFilter> filter_;

  // Our stream ID on the message filter. Only accessed on the IO thread.
  int32 stream_id_;

  // Data transfer between browser and render process uses a combination
  // of sync sockets and shared memory to provide lowest possible latency.
  scoped_ptr<base::SharedMemory> shared_memory_;
  scoped_ptr<base::SyncSocket> socket_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(AudioDevice);
};

#endif  // CONTENT_RENDERER_MEDIA_AUDIO_DEVICE_H_
