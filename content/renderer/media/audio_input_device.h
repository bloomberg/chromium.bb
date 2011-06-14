// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_AUDIO_INPUT_DEVICE_H_
#define CONTENT_RENDERER_MEDIA_AUDIO_INPUT_DEVICE_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/shared_memory.h"
#include "base/threading/simple_thread.h"
#include "content/renderer/media/audio_input_message_filter.h"

struct AudioParameters;

// Each instance of AudioInputDevice corresponds to one host stream.
// This class is not thread-safe, so its methods must be called from
// the same thread.

// TODO(henrika): This class is based on the AudioDevice class and it has
// many components in common. Investigate potential for re-factoring.
class AudioInputDevice : public AudioInputMessageFilter::Delegate,
                         public base::DelegateSimpleThread::Delegate,
                         public base::RefCountedThreadSafe<AudioInputDevice> {
 public:
  class CaptureCallback {
   public:
    virtual void Capture(const std::vector<float*>& audio_data,
                         size_t number_of_frames,
                         size_t audio_delay_milliseconds) = 0;
   protected:
    virtual ~CaptureCallback() {}
  };

  // |buffer_size| is the number of sample-frames.
  AudioInputDevice(size_t buffer_size,
                   int channels,
                   double sample_rate,
                   CaptureCallback* callback);
  virtual ~AudioInputDevice();

  // Starts audio capturing. Returns |true| on success.
  bool Start();

  // Stops audio capturing. Returns |true| on success.
  bool Stop();

  // Sets the capture volume scaling, with range [0.0, 1.0] inclusive.
  // Returns |true| on success.
  bool SetVolume(double volume);

  // Gets the capture volume scaling, with range [0.0, 1.0] inclusive.
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

  // AudioInputMessageFilter::Delegate implementation
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

  // Calls the client's callback for capturing audio.
  void FireCaptureCallback();
  CaptureCallback* callback_;

  // The client callback receives captured audio here.
  std::vector<float*> audio_data_;

  // The client stores the last reported audio delay in this member.
  // The delay shall reflect the amount of audio which still resides in
  // the input buffer, i.e., the expected audio input delay.
  int audio_delay_milliseconds_;

  // The current volume scaling [0.0, 1.0] of the audio stream.
  double volume_;

  // Callbacks for capturing audio occur on this thread.
  scoped_ptr<base::DelegateSimpleThread> audio_thread_;

  // IPC message stuff.
  base::SharedMemory* shared_memory() { return shared_memory_.get(); }
  base::SyncSocket* socket() { return socket_.get(); }
  void* shared_memory_data() { return shared_memory()->memory(); }

  // MessageFilter used to send/receive IPC. THIS MUST ONLY BE ACCESSED ON THE
  // I/O thread except to send messages and get the message loop.
  static scoped_refptr<AudioInputMessageFilter> filter_;

  // Our ID on the message filter. THIS MUST ONLY BE ACCESSED ON THE I/O THREAD
  // or else you could race with the initialize function which sets it.
  int32 stream_id_;

  scoped_ptr<base::SharedMemory> shared_memory_;
  scoped_ptr<base::SyncSocket> socket_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(AudioInputDevice);
};

#endif  // CONTENT_RENDERER_MEDIA_AUDIO_INPUT_DEVICE_H_
