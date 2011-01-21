// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Audio rendering unit utilizing audio output stream provided by browser
// process through IPC.
//
// Relationship of classes.
//
//    AudioRendererHost                AudioRendererImpl
//           ^                                ^
//           |                                |
//           v                 IPC            v
//   RenderMessageFilter   <---------> AudioMessageFilter
//
// Implementation of interface with audio device is in AudioRendererHost and
// it provides services and entry points in RenderMessageFilter, allowing
// usage of IPC calls to interact with audio device. AudioMessageFilter acts
// as a portal for IPC calls and does no more than delegation.
//
// Transportation of audio buffer is done by using shared memory, after
// OnCreateStream is executed, OnCreated would be called along with a
// SharedMemoryHandle upon successful creation of audio output stream in the
// browser process. The same piece of shared memory would be used during the
// lifetime of this unit.
//
// This class lives inside three threads during it's lifetime, namely:
// 1. IO thread.
//    The thread within which this class receives all the IPC messages and
//    IPC communications can only happen in this thread.
// 2. Pipeline thread
//    Initialization of filter and proper stopping of filters happens here.
//    Properties of this filter is also set in this thread.
// 3. Audio decoder thread (If there's one.)
//    Responsible for decoding audio data and gives raw PCM data to this object.

#ifndef CHROME_RENDERER_MEDIA_AUDIO_RENDERER_IMPL_H_
#define CHROME_RENDERER_MEDIA_AUDIO_RENDERER_IMPL_H_
#pragma once

#include "base/gtest_prod_util.h"
#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "base/shared_memory.h"
#include "base/synchronization/lock.h"
#include "chrome/renderer/audio_message_filter.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_manager.h"
#include "media/base/filters.h"
#include "media/filters/audio_renderer_base.h"

class AudioMessageFilter;

class AudioRendererImpl : public media::AudioRendererBase,
                          public AudioMessageFilter::Delegate,
                          public MessageLoop::DestructionObserver {
 public:
  // Methods called on Render thread ------------------------------------------
  explicit AudioRendererImpl(AudioMessageFilter* filter);
  virtual ~AudioRendererImpl();

  // Methods called on IO thread ----------------------------------------------
  // AudioMessageFilter::Delegate methods, called by AudioMessageFilter.
  virtual void OnRequestPacket(AudioBuffersState buffers_state);
  virtual void OnStateChanged(const ViewMsg_AudioStreamState_Params& state);
  virtual void OnCreated(base::SharedMemoryHandle handle, uint32 length);
  virtual void OnLowLatencyCreated(base::SharedMemoryHandle handle,
                                   base::SyncSocket::Handle socket_handle,
                                   uint32 length);
  virtual void OnVolume(double volume);

  // Methods called on pipeline thread ----------------------------------------
  // media::Filter implementation.
  virtual void SetPlaybackRate(float rate);
  virtual void Pause(media::FilterCallback* callback);
  virtual void Seek(base::TimeDelta time, media::FilterCallback* callback);
  virtual void Play(media::FilterCallback* callback);

  // media::AudioRenderer implementation.
  virtual void SetVolume(float volume);

 protected:
  // Methods called on audio renderer thread ----------------------------------
  // These methods are called from AudioRendererBase.
  virtual bool OnInitialize(const media::MediaFormat& media_format);
  virtual void OnStop();

  // Called when the decoder completes a Read().
  virtual void ConsumeAudioSamples(scoped_refptr<media::Buffer> buffer_in);

 private:
  // For access to constructor and IO thread methods.
  friend class AudioRendererImplTest;
  FRIEND_TEST_ALL_PREFIXES(AudioRendererImplTest, Stop);
  FRIEND_TEST_ALL_PREFIXES(AudioRendererImplTest,
                           DestroyedMessageLoop_ConsumeAudioSamples);
  // Helper methods.
  // Convert number of bytes to duration of time using information about the
  // number of channels, sample rate and sample bits.
  base::TimeDelta ConvertToDuration(int bytes);

  // Methods call on IO thread ------------------------------------------------
  // The following methods are tasks posted on the IO thread that needs to
  // be executed on that thread. They interact with AudioMessageFilter and
  // sends IPC messages on that thread.
  void CreateStreamTask(AudioParameters params);
  void PlayTask();
  void PauseTask();
  void SeekTask();
  void SetVolumeTask(double volume);
  void NotifyPacketReadyTask();
  void DestroyTask();

  // Called on IO thread when message loop is dying.
  virtual void WillDestroyCurrentMessageLoop();

  // Information about the audio stream.
  AudioParameters params_;
  uint32 bytes_per_second_;

  scoped_refptr<AudioMessageFilter> filter_;

  // ID of the stream created in the browser process.
  int32 stream_id_;

  // Memory shared by the browser process for audio buffer.
  scoped_ptr<base::SharedMemory> shared_memory_;
  uint32 shared_memory_size_;

  // Message loop for the IO thread.
  MessageLoop* io_loop_;

  // Protects:
  // - |stopped_|
  // - |pending_request_|
  // - |request_buffers_state_|
  base::Lock lock_;

  // A flag that indicates this filter is called to stop.
  bool stopped_;

  // A flag that indicates an outstanding packet request.
  bool pending_request_;

  // State of the audio buffers at time of the last request.
  AudioBuffersState request_buffers_state_;

  // State variables for prerolling.
  bool prerolling_;

  // Remaining bytes for prerolling to complete.
  uint32 preroll_bytes_;

  DISALLOW_COPY_AND_ASSIGN(AudioRendererImpl);
};

#endif  // CHROME_RENDERER_MEDIA_AUDIO_RENDERER_IMPL_H_
