// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_PLATFORM_AUDIO_INPUT_IMPL_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_PLATFORM_AUDIO_INPUT_IMPL_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/renderer/media/audio_input_message_filter.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"

struct AudioParameters;

class PepperPlatformAudioInputImpl
    : public webkit::ppapi::PluginDelegate::PlatformAudioInput,
      public AudioInputMessageFilter::Delegate,
      public base::RefCountedThreadSafe<PepperPlatformAudioInputImpl> {
 public:
  virtual ~PepperPlatformAudioInputImpl();

  // Factory function, returns NULL on failure. StreamCreated() will be called
  // when the stream is created.
  static PepperPlatformAudioInputImpl* Create(
      uint32_t sample_rate,
      uint32_t sample_count,
      webkit::ppapi::PluginDelegate::PlatformAudioCommonClient* client);

  // PlatformAudioInput implementation (called on main thread).
  virtual bool StartCapture() OVERRIDE;
  virtual bool StopCapture() OVERRIDE;
  virtual void ShutDown() OVERRIDE;

 private:
  PepperPlatformAudioInputImpl();

  bool Initialize(
      uint32_t sample_rate,
      uint32_t sample_count,
      webkit::ppapi::PluginDelegate::PlatformAudioCommonClient* client);

  // I/O thread backends to above functions.
  void InitializeOnIOThread(const AudioParameters& params);
  void StartCaptureOnIOThread();
  void StopCaptureOnIOThread();
  void ShutDownOnIOThread();

  // AudioInputMessageFilter::Delegate.
  virtual void OnStreamCreated(base::SharedMemoryHandle handle,
                               base::SyncSocket::Handle socket_handle,
                               uint32 length) OVERRIDE;
  virtual void OnVolume(double volume) OVERRIDE;
  virtual void OnStateChanged(AudioStreamState state) OVERRIDE;
  virtual void OnDeviceReady(const std::string&) OVERRIDE;

  // The client to notify when the stream is created. THIS MUST ONLY BE
  // ACCESSED ON THE MAIN THREAD.
  webkit::ppapi::PluginDelegate::PlatformAudioCommonClient* client_;

  // MessageFilter used to send/receive IPC. THIS MUST ONLY BE ACCESSED ON THE
  // I/O thread except to send messages and get the message loop.
  scoped_refptr<AudioInputMessageFilter> filter_;

  // Our ID on the MessageFilter. THIS MUST ONLY BE ACCESSED ON THE I/O THREAD
  // or else you could race with the initialize function which sets it.
  int32 stream_id_;

  base::MessageLoopProxy* main_message_loop_proxy_;

  DISALLOW_COPY_AND_ASSIGN(PepperPlatformAudioInputImpl);
};

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_PLATFORM_AUDIO_INPUT_IMPL_H_
