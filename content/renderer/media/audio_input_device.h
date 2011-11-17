// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Low-latency audio capturing unit utilizing audio input stream provided
// by browser process through IPC.
//
// Relationship of classes:
//
//  AudioInputController                 AudioInputDevice
//           ^                                  ^
//           |                                  |
//           v                  IPC             v
// AudioInputRendererHost  <---------> AudioInputMessageFilter
//           ^
//           |
//           v
// AudioInputDeviceManager
//
// Transportation of audio samples from the browser to the render process
// is done by using shared memory in combination with a sync socket pair
// to generate a low latency transport. The AudioInputDevice user registers
// an AudioInputDevice::CaptureCallback at construction and will be called
// by the AudioInputDevice with recorded audio from the underlying audio layers.
// The session ID is used by the AudioInputRendererHost to start the device
// referenced by this ID.
//
// State sequences:
//
//            Task [IO thread]                  IPC [IO thread]
//
// Sequence where session_id has not been set using SetDevice():
// Start -> InitializeOnIOThread -----> AudioInputHostMsg_CreateStream ------->
//     <- OnLowLatencyCreated <- AudioInputMsg_NotifyLowLatencyStreamCreated <-
//       ---> StartOnIOThread ---------> AudioInputHostMsg_PlayStream -------->
//
// Sequence where session_id has been set using SetDevice():
// Start ->  InitializeOnIOThread --> AudioInputHostMsg_StartDevice --->
//      <---- OnStarted <-------------- AudioInputMsg_NotifyDeviceStarted <----
//       -> OnDeviceReady ------------> AudioInputHostMsg_CreateStream ------->
//     <- OnLowLatencyCreated <- AudioInputMsg_NotifyLowLatencyStreamCreated <-
//       ---> StartOnIOThread ---------> AudioInputHostMsg_PlayStream -------->
//
// AudioInputDevice::Capture => low latency audio transport on audio thread =>
//                               |
// Stop --> ShutDownOnIOThread ------>  AudioInputHostMsg_CloseStream -> Close
//
// This class utilizes three threads during its lifetime, namely:
//
// 1. Creating thread.
//    Must be the main render thread. Start and Stop should be called on
//    this thread.
// 2. IO thread.
//    The thread within which this class receives all the IPC messages and
//    IPC communications can only happen in this thread.
// 3. Audio transport thread.
//    Responsible for calling the CaptrureCallback and feed audio samples from
//    the audio layer in the browser process using sync sockets and shared
//    memory.
//
// Implementation notes:
//
// - Start() is asynchronous/non-blocking.
// - Stop() is synchronous/blocking.
// - SetDevice() is asynchronous/non-blocking.
// - The user must call Stop() before deleting the class instance.

#ifndef CONTENT_RENDERER_MEDIA_AUDIO_INPUT_DEVICE_H_
#define CONTENT_RENDERER_MEDIA_AUDIO_INPUT_DEVICE_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/shared_memory.h"
#include "base/threading/simple_thread.h"
#include "content/common/content_export.h"
#include "content/renderer/media/audio_input_message_filter.h"
#include "media/audio/audio_parameters.h"

// TODO(henrika): This class is based on the AudioDevice class and it has
// many components in common. Investigate potential for re-factoring.
// TODO(henrika): Add support for event handling (e.g. OnStateChanged,
// OnCaptureStopped etc.) and ensure that we can deliver these notifications
// to any clients using this class.
class CONTENT_EXPORT AudioInputDevice
    : public AudioInputMessageFilter::Delegate,
      public base::DelegateSimpleThread::Delegate,
      public base::RefCountedThreadSafe<AudioInputDevice> {
 public:
  class CONTENT_EXPORT CaptureCallback {
   public:
    virtual void Capture(const std::vector<float*>& audio_data,
                         size_t number_of_frames,
                         size_t audio_delay_milliseconds) = 0;
   protected:
    virtual ~CaptureCallback() {}
  };

  class CONTENT_EXPORT CaptureEventHandler {
   public:
    // Notification to the client that the device with the specific index has
    // been started. This callback is triggered as a result of StartDevice().
    virtual void OnDeviceStarted(int device_index) = 0;

    // Notification to the client that the device has been stopped.
    virtual void OnDeviceStopped() = 0;

   protected:
    virtual ~CaptureEventHandler() {}
  };

  // Methods called on main render thread -------------------------------------
  AudioInputDevice(size_t buffer_size,
                   int channels,
                   double sample_rate,
                   CaptureCallback* callback,
                   CaptureEventHandler* event_handler);
  virtual ~AudioInputDevice();

  // Specify the |session_id| to query which device to use. This method is
  // asynchronous/non-blocking.
  // Start() will use the second sequence if this method is called before.
  void SetDevice(int session_id);

  // Starts audio capturing. This method is asynchronous/non-blocking.
  // TODO(henrika): add support for notification when recording has started.
  void Start();

  // Stops audio capturing. This method is synchronous/blocking.
  // Returns |true| on success.
  // TODO(henrika): add support for notification when recording has stopped.
  bool Stop();

  // Sets the capture volume scaling, with range [0.0, 1.0] inclusive.
  // Returns |true| on success.
  bool SetVolume(double volume);

  // Gets the capture volume scaling, with range [0.0, 1.0] inclusive.
  // Returns |true| on success.
  bool GetVolume(double* volume);

  double sample_rate() const { return audio_parameters_.sample_rate; }
  size_t buffer_size() const { return audio_parameters_.samples_per_packet; }

  // Methods called on IO thread ----------------------------------------------
  // AudioInputMessageFilter::Delegate impl., called by AudioInputMessageFilter
  virtual void OnLowLatencyCreated(base::SharedMemoryHandle handle,
                                   base::SyncSocket::Handle socket_handle,
                                   uint32 length) OVERRIDE;
  virtual void OnVolume(double volume) OVERRIDE;
  virtual void OnStateChanged(AudioStreamState state) OVERRIDE;
  virtual void OnDeviceReady(int index) OVERRIDE;

 private:
  // Methods called on IO thread ----------------------------------------------
  // The following methods are tasks posted on the IO thread that needs to
  // be executed on that thread. They interact with AudioInputMessageFilter and
  // sends IPC messages on that thread.
  void InitializeOnIOThread();
  void SetSessionIdOnIOThread(int session_id);
  void StartOnIOThread();
  void ShutDownOnIOThread(base::WaitableEvent* completion);
  void SetVolumeOnIOThread(double volume);

  void Send(IPC::Message* message);

  // Method called on the audio thread ----------------------------------------
  // Calls the client's callback for capturing audio.
  void FireCaptureCallback();

  // DelegateSimpleThread::Delegate implementation.
  virtual void Run() OVERRIDE;

  // Format
  AudioParameters audio_parameters_;

  CaptureCallback* callback_;
  CaptureEventHandler* event_handler_;

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

  // Cached audio input message filter (lives on the main render thread).
  scoped_refptr<AudioInputMessageFilter> filter_;

  // Our stream ID on the message filter. Only modified on the IO thread.
  int32 stream_id_;

  // The media session ID used to identify which input device to be started.
  // Only modified on the IO thread.
  int session_id_;

  // State variable used to indicate it is waiting for a OnDeviceReady()
  // callback. Only modified on the IO thread.
  bool pending_device_ready_;

  scoped_ptr<base::SharedMemory> shared_memory_;
  scoped_ptr<base::SyncSocket> socket_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(AudioInputDevice);
};

#endif  // CONTENT_RENDERER_MEDIA_AUDIO_INPUT_DEVICE_H_
