// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_PLATFORM_AUDIO_OUTPUT_IMPL_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_PLATFORM_AUDIO_OUTPUT_IMPL_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "content/renderer/media/audio_message_filter.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"

namespace media{
class AudioParameters;
}

namespace base {
class MessageLoopProxy;
}

namespace content {

class PepperPlatformAudioOutputImpl
    : public webkit::ppapi::PluginDelegate::PlatformAudioOutput,
      public AudioMessageFilter::Delegate,
      public base::RefCountedThreadSafe<PepperPlatformAudioOutputImpl> {
 public:
  // Factory function, returns NULL on failure. StreamCreated() will be called
  // when the stream is created.
  static PepperPlatformAudioOutputImpl* Create(
      int sample_rate,
      int frames_per_buffer,
      webkit::ppapi::PluginDelegate::PlatformAudioOutputClient* client);

  // PlatformAudioOutput implementation (called on main thread).
  virtual bool StartPlayback() OVERRIDE;
  virtual bool StopPlayback() OVERRIDE;
  virtual void ShutDown() OVERRIDE;

  // AudioMessageFilter::Delegate.
  virtual void OnStateChanged(AudioStreamState state) OVERRIDE;
  virtual void OnStreamCreated(base::SharedMemoryHandle handle,
                               base::SyncSocket::Handle socket_handle,
                               uint32 length) OVERRIDE;

 protected:
  virtual ~PepperPlatformAudioOutputImpl();

 private:
  friend class base::RefCountedThreadSafe<PepperPlatformAudioOutputImpl>;

  PepperPlatformAudioOutputImpl();

  bool Initialize(
      int sample_rate,
      int frames_per_buffer,
      webkit::ppapi::PluginDelegate::PlatformAudioOutputClient* client);

  // I/O thread backends to above functions.
  void InitializeOnIOThread(const media::AudioParameters& params);
  void StartPlaybackOnIOThread();
  void StopPlaybackOnIOThread();
  void ShutDownOnIOThread();

  // The client to notify when the stream is created. THIS MUST ONLY BE
  // ACCESSED ON THE MAIN THREAD.
  webkit::ppapi::PluginDelegate::PlatformAudioOutputClient* client_;

  // MessageFilter used to send/receive IPC. THIS MUST ONLY BE ACCESSED ON THE
  // I/O thread except to send messages and get the message loop.
  scoped_refptr<AudioMessageFilter> filter_;

  // Our ID on the MessageFilter. THIS MUST ONLY BE ACCESSED ON THE I/O THREAD
  // or else you could race with the initialize function which sets it.
  int32 stream_id_;

  base::MessageLoopProxy* main_message_loop_proxy_;

  DISALLOW_COPY_AND_ASSIGN(PepperPlatformAudioOutputImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_PLATFORM_AUDIO_OUTPUT_IMPL_H_
