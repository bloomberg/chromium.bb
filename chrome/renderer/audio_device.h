// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_AUDIO_DEVICE_H_
#define CHROME_RENDERER_AUDIO_DEVICE_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/shared_memory.h"
#include "base/threading/simple_thread.h"
#include "chrome/renderer/audio_message_filter.h"

// Each instance of AudioDevice corresponds to one host stream.
// This class is not thread-safe, so its methods must be called from
// the same thread.
class AudioDevice : public AudioMessageFilter::Delegate,
                    public base::DelegateSimpleThread::Delegate {
 public:
  class RenderCallback {
   public:
    virtual void Render(const std::vector<float*>& audio_data,
                        size_t number_of_frames) = 0;
   protected:
    virtual ~RenderCallback() {}
  };

  // |buffer_size| is the number of sample-frames.
  AudioDevice(size_t buffer_size,
              int channels,
              double sample_rate,
              RenderCallback* callback);
  virtual ~AudioDevice();

  // Returns |true| on success.
  bool Start();
  bool Stop();

 private:
  // AudioMessageFilter::Delegate implementation.
  virtual void OnRequestPacket(AudioBuffersState buffers_state);
  virtual void OnStateChanged(const ViewMsg_AudioStreamState_Params& state);
  virtual void OnCreated(base::SharedMemoryHandle handle, uint32 length);
  virtual void OnLowLatencyCreated(base::SharedMemoryHandle handle,
                                   base::SyncSocket::Handle socket_handle,
                                   uint32 length);
  virtual void OnVolume(double volume);
  virtual void OnDestroy();

  // DelegateSimpleThread::Delegate implementation.
  virtual void Run();

  // Format
  size_t buffer_size_;  // in sample-frames
  int channels_;
  double sample_rate_;

  // Calls the client's callback for rendering audio.
  void FireRenderCallback();
  RenderCallback* callback_;

  // The client callback renders audio into here.
  std::vector<float*> audio_data_;

  // Callbacks for rendering audio occur on this thread.
  scoped_ptr<base::DelegateSimpleThread> audio_thread_;

  // IPC message stuff.
  base::SharedMemory* shared_memory() { return shared_memory_.get(); }
  base::SyncSocket* socket() { return socket_.get(); }
  void* shared_memory_data() { return shared_memory()->memory(); }

  static scoped_refptr<AudioMessageFilter> filter_;
  int32 stream_id_;
  scoped_ptr<base::SharedMemory> shared_memory_;
  scoped_ptr<base::SyncSocket> socket_;

  DISALLOW_COPY_AND_ASSIGN(AudioDevice);
};

#endif  // CHROME_RENDERER_AUDIO_DEVICE_H_
