// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// MessageFilter that handles audio input messages and delegates them to
// audio capturers. Created on render thread, AudioMessageFilter is operated on
// IO thread (main thread of render process), it intercepts audio messages
// and process them on IO thread since these messages are time critical.
// This implementation only supports low-latency (based on SyncSocket)
// messaging.

#ifndef CONTENT_RENDERER_MEDIA_AUDIO_INPUT_MESSAGE_FILTER_H_
#define CONTENT_RENDERER_MEDIA_AUDIO_INPUT_MESSAGE_FILTER_H_
#pragma once

#include "base/id_map.h"
#include "base/shared_memory.h"
#include "base/sync_socket.h"
#include "ipc/ipc_channel_proxy.h"
#include "media/audio/audio_buffers_state.h"

class MessageLoop;

class AudioInputMessageFilter : public IPC::ChannelProxy::MessageFilter {
 public:
  class Delegate {
   public:
    // Called when a low-latency audio input stream has been created in the
    // browser process.
    virtual void OnLowLatencyCreated(base::SharedMemoryHandle handle,
                                     base::SyncSocket::Handle socket_handle,
                                     uint32 length) = 0;

    // Called when notification of input stream volume is received from the
    // browser process.
    virtual void OnVolume(double volume) = 0;

   protected:
    virtual ~Delegate() {}
  };

  explicit AudioInputMessageFilter(int32 route_id);
  virtual ~AudioInputMessageFilter();

  // Add a delegate to the map and return id of the entry.
  int32 AddDelegate(Delegate* delegate);

  // Remove a delegate referenced by |id| from the map.
  void RemoveDelegate(int32 id);

  // Sends an IPC message using |channel_|.
  bool Send(IPC::Message* message);

  MessageLoop* message_loop() { return message_loop_; }

 private:
  // TODO(henrika): add unit tests (compare with AudioMessageFilter)

  // IPC::ChannelProxy::MessageFilter override. Called on IO thread.
  virtual bool OnMessageReceived(const IPC::Message& message);
  virtual void OnFilterAdded(IPC::Channel* channel);
  virtual void OnFilterRemoved();
  virtual void OnChannelClosing();

  // Received when browser process has created an audio input stream of low
  // latency.
  void OnLowLatencyStreamCreated(int stream_id, base::SharedMemoryHandle handle,
#if defined(OS_WIN)
                                 base::SyncSocket::Handle socket_handle,
#else
                                 base::FileDescriptor socket_descriptor,
#endif
                                 uint32 length);

  // Notification of volume property of an audio input stream.
  void OnStreamVolume(int stream_id, double volume);

  // A map of stream ids to delegates.
  IDMap<Delegate> delegates_;

  IPC::Channel* channel_;

  int32 route_id_;

  MessageLoop* message_loop_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(AudioInputMessageFilter);
};

#endif  // CONTENT_RENDERER_MEDIA_AUDIO_INPUT_MESSAGE_FILTER_H_
