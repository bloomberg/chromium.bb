// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_AUDIO_DEVICE_H_
#define CONTENT_RENDERER_MEDIA_AUDIO_DEVICE_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/shared_memory.h"
#include "base/threading/simple_thread.h"
#include "content/renderer/media/audio_message_filter.h"

struct AudioParameters;

// Each instance of AudioDevice corresponds to one host stream.
// This class is not thread-safe, so its methods must be called from
// the same thread.
class AudioDevice : public AudioMessageFilter::Delegate,
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

  // |buffer_size| is the number of sample-frames.
  AudioDevice(size_t buffer_size,
              int channels,
              double sample_rate,
              RenderCallback* callback);
  virtual ~AudioDevice();

  // Starts audio playback. Returns |true| on success.
  bool Start();

  // Stops audio playback. Returns |true| on success.
  bool Stop();

  // Sets the playback volume, with range [0.0, 1.0] inclusive.
  // Returns |true| on success.
  bool SetVolume(double volume);

  // Gets the playback volume, with range [0.0, 1.0] inclusive.
  // Returns |true| on success.
  bool GetVolume(double* volume);

  double sample_rate() const { return sample_rate_; }
  size_t buffer_size() const { return buffer_size_; }

 private:
  // I/O thread backends to above functions.
  void InitializeOnIOThread(const AudioParameters& params);
  void StartOnIOThread();
  void ShutDownOnIOThread();
  void SetVolumeOnIOThread(double volume);

  // AudioMessageFilter::Delegate implementation.
  virtual void OnRequestPacket(AudioBuffersState buffers_state);
  virtual void OnStateChanged(AudioStreamState state);
  virtual void OnCreated(base::SharedMemoryHandle handle, uint32 length);
  virtual void OnLowLatencyCreated(base::SharedMemoryHandle handle,
                                   base::SyncSocket::Handle socket_handle,
                                   uint32 length);
  virtual void OnVolume(double volume);

  // DelegateSimpleThread::Delegate implementation.
  virtual void Run();

  // Format
  size_t buffer_size_;  // in sample-frames
  int channels_;
  int bits_per_sample_;
  double sample_rate_;

  // Calls the client's callback for rendering audio.
  void FireRenderCallback();
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

  // MessageFilter used to send/receive IPC. THIS MUST ONLY BE ACCESSED ON THE
  // I/O thread except to send messages and get the message loop.
  static scoped_refptr<AudioMessageFilter> filter_;

  // Our ID on the message filter. THIS MUST ONLY BE ACCESSED ON THE I/O THREAD
  // or else you could race with the initialize function which sets it.
  int32 stream_id_;

  scoped_ptr<base::SharedMemory> shared_memory_;
  scoped_ptr<base::SyncSocket> socket_;

  DISALLOW_COPY_AND_ASSIGN(AudioDevice);
};

#endif  // CONTENT_RENDERER_MEDIA_AUDIO_DEVICE_H_
