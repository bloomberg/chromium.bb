// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// MessageFilter that handles audio messages and delegates them to audio
// renderers. Created on render thread, AudioMessageFilter is operated on
// IO thread (secondary thread of render process) it intercepts audio messages
// and process them on IO thread since these messages are time critical.

#ifndef CONTENT_RENDERER_MEDIA_AUDIO_MESSAGE_FILTER_H_
#define CONTENT_RENDERER_MEDIA_AUDIO_MESSAGE_FILTER_H_
#pragma once

#include "base/gtest_prod_util.h"
#include "base/id_map.h"
#include "base/shared_memory.h"
#include "base/sync_socket.h"
#include "content/common/media/audio_stream_state.h"
#include "content/common/content_export.h"
#include "ipc/ipc_channel_proxy.h"
#include "media/audio/audio_buffers_state.h"

namespace base {
class Time;
}

class CONTENT_EXPORT AudioMessageFilter
    : public IPC::ChannelProxy::MessageFilter {
 public:
  class CONTENT_EXPORT Delegate {
   public:
    // Called when an audio packet is requested from the browser process.
    virtual void OnRequestPacket(AudioBuffersState buffers_state) = 0;

    // Called when state of an audio stream has changed in the browser process.
    virtual void OnStateChanged(AudioStreamState state) = 0;

    // Called when an audio stream has been created in the browser process.
    virtual void OnCreated(base::SharedMemoryHandle handle, uint32 length) = 0;

    // Called when a low-latency audio stream has been created in the browser
    // process.
    virtual void OnLowLatencyCreated(base::SharedMemoryHandle handle,
                                     base::SyncSocket::Handle socket_handle,
                                     uint32 length) = 0;

    // Called when notification of stream volume is received from the browser
    // process.
    virtual void OnVolume(double volume) = 0;

   protected:
    virtual ~Delegate() {}
  };

  AudioMessageFilter();
  virtual ~AudioMessageFilter();

  // Add a delegate to the map and return id of the entry.
  int32 AddDelegate(Delegate* delegate);

  // Remove a delegate referenced by |id| from the map.
  void RemoveDelegate(int32 id);

  // Sends an IPC message using |channel_|.
  bool Send(IPC::Message* message);

 private:
  FRIEND_TEST_ALL_PREFIXES(AudioMessageFilterTest, Basic);
  FRIEND_TEST_ALL_PREFIXES(AudioMessageFilterTest, Delegates);

  // IPC::ChannelProxy::MessageFilter override. Called on IO thread.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnFilterAdded(IPC::Channel* channel) OVERRIDE;
  virtual void OnFilterRemoved() OVERRIDE;
  virtual void OnChannelClosing() OVERRIDE;

  // Received when browser process wants more audio packet.
  void OnRequestPacket(int stream_id, AudioBuffersState buffers_state);

  // Received when browser process has created an audio output stream.
  void OnStreamCreated(int stream_id, base::SharedMemoryHandle handle,
                       uint32 length);

  // Received when browser process has created an audio output stream of low
  // latency.
  void OnLowLatencyStreamCreated(int stream_id, base::SharedMemoryHandle handle,
#if defined(OS_WIN)
                                 base::SyncSocket::Handle socket_handle,
#else
                                 base::FileDescriptor socket_descriptor,
#endif
                                 uint32 length);


  // Received when internal state of browser process' audio output device has
  // changed.
  void OnStreamStateChanged(int stream_id, AudioStreamState state);

  // Notification of volume property of an audio output stream.
  void OnStreamVolume(int stream_id, double volume);

  // A map of stream ids to delegates.
  IDMap<Delegate> delegates_;

  IPC::Channel* channel_;

  DISALLOW_COPY_AND_ASSIGN(AudioMessageFilter);
};

#endif  // CONTENT_RENDERER_MEDIA_AUDIO_MESSAGE_FILTER_H_
