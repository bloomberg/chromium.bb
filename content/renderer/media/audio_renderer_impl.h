// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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

#ifndef CONTENT_RENDERER_MEDIA_AUDIO_RENDERER_IMPL_H_
#define CONTENT_RENDERER_MEDIA_AUDIO_RENDERER_IMPL_H_
#pragma once

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/shared_memory.h"
#include "base/synchronization/lock.h"
#include "base/threading/simple_thread.h"
#include "content/common/content_export.h"
#include "content/renderer/media/audio_message_filter.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_manager.h"
#include "media/base/filters.h"
#include "media/filters/audio_renderer_base.h"

class AudioMessageFilter;

class CONTENT_EXPORT AudioRendererImpl
    : public media::AudioRendererBase,
      public AudioMessageFilter::Delegate,
      public base::DelegateSimpleThread::Delegate,
      public MessageLoop::DestructionObserver {
 public:
  // Methods called on Render thread ------------------------------------------
  AudioRendererImpl();
  virtual ~AudioRendererImpl();

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

  // Methods called on pipeline thread ----------------------------------------
  // media::Filter implementation.
  virtual void SetPlaybackRate(float rate) OVERRIDE;
  virtual void Pause(const base::Closure& callback) OVERRIDE;
  virtual void Seek(base::TimeDelta time,
                    const media::FilterStatusCB& cb) OVERRIDE;
  virtual void Play(const base::Closure& callback) OVERRIDE;

  // media::AudioRenderer implementation.
  virtual void SetVolume(float volume) OVERRIDE;

 protected:
  // Methods called on audio renderer thread ----------------------------------
  // These methods are called from AudioRendererBase.
  virtual bool OnInitialize(int bits_per_channel,
                            ChannelLayout channel_layout,
                            int sample_rate) OVERRIDE;
  virtual void OnStop() OVERRIDE;

  // Called when the decoder completes a Read().
  virtual void ConsumeAudioSamples(
      scoped_refptr<media::Buffer> buffer_in) OVERRIDE;

 private:
  // We are using either low- or high-latency code path.
  enum LatencyType {
    kUninitializedLatency = 0,
    kLowLatency,
    kHighLatency
  };
  static LatencyType latency_type_;

  // For access to constructor and IO thread methods.
  friend class AudioRendererImplTest;
  friend class DelegateCaller;
  FRIEND_TEST_ALL_PREFIXES(AudioRendererImplTest, Stop);
  FRIEND_TEST_ALL_PREFIXES(AudioRendererImplTest,
                           DestroyedMessageLoop_ConsumeAudioSamples);
  FRIEND_TEST_ALL_PREFIXES(AudioRendererImplTest, UpdateEarliestEndTime);
  // Helper methods.
  // Convert number of bytes to duration of time using information about the
  // number of channels, sample rate and sample bits.
  base::TimeDelta ConvertToDuration(int bytes);

  // Methods call on IO thread ------------------------------------------------
  // The following methods are tasks posted on the IO thread that needs to
  // be executed on that thread. They interact with AudioMessageFilter and
  // sends IPC messages on that thread.
  void CreateStreamTask(const AudioParameters& params);
  void PlayTask();
  void PauseTask();
  void SeekTask();
  void SetVolumeTask(double volume);
  void NotifyPacketReadyTask();
  void DestroyTask();

  // Called on IO thread when message loop is dying.
  virtual void WillDestroyCurrentMessageLoop() OVERRIDE;

  // DelegateSimpleThread::Delegate implementation.
  virtual void Run() OVERRIDE;

  // (Re-)starts playback.
  void NotifyDataAvailableIfNecessary();

  // Creates socket. Virtual so tests can override.
  virtual void CreateSocket(base::SyncSocket::Handle socket_handle);

  // Launching audio thread. Virtual so tests can override.
  virtual void CreateAudioThread();

  // Accessors used by tests.
  static LatencyType latency_type() {
    return latency_type_;
  }

  base::Time earliest_end_time() const {
    return earliest_end_time_;
  }

  void set_earliest_end_time(const base::Time& earliest_end_time) {
    earliest_end_time_ = earliest_end_time;
  }

  uint32 bytes_per_second() const {
    return bytes_per_second_;
  }

  // Should be called before any class instance is created.
  static void set_latency_type(LatencyType latency_type);

  // Helper method for IPC send calls.
  void Send(IPC::Message* message);

  // Estimate earliest time when current buffer can stop playing.
  void UpdateEarliestEndTime(int bytes_filled,
                             base::TimeDelta request_delay,
                             base::Time time_now);

  // Used to calculate audio delay given bytes.
  uint32 bytes_per_second_;

  // Whether the stream has been created yet.
  bool stream_created_;

  // ID of the stream created in the browser process.
  int32 stream_id_;

  // Memory shared by the browser process for audio buffer.
  scoped_ptr<base::SharedMemory> shared_memory_;
  uint32 shared_memory_size_;

  // Cached audio message filter (lives on the main render thread).
  scoped_refptr<AudioMessageFilter> filter_;

  // Low latency IPC stuff.
  scoped_ptr<base::SyncSocket> socket_;

  // That thread waits for audio input.
  scoped_ptr<base::DelegateSimpleThread> audio_thread_;

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

  // We're supposed to know amount of audio data OS or hardware buffered, but
  // that is not always so -- on my Linux box
  // AudioBuffersState::hardware_delay_bytes never reaches 0.
  //
  // As a result we cannot use it to find when stream ends. If we just ignore
  // buffered data we will notify host that stream ended before it is actually
  // did so, I've seen it done ~140ms too early when playing ~150ms file.
  //
  // Instead of trying to invent OS-specific solution for each and every OS we
  // are supporting, use simple workaround: every time we fill the buffer we
  // remember when it should stop playing, and do not assume that buffer is
  // empty till that time. Workaround is not bulletproof, as we don't exactly
  // know when that particular data would start playing, but it is much better
  // than nothing.
  base::Time earliest_end_time_;

  DISALLOW_COPY_AND_ASSIGN(AudioRendererImpl);
};

#endif  // CONTENT_RENDERER_MEDIA_AUDIO_RENDERER_IMPL_H_
