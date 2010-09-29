// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/audio_message_filter.h"

#include "base/message_loop.h"
#include "base/time.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "ipc/ipc_logging.h"

AudioMessageFilter::AudioMessageFilter(int32 route_id)
    : channel_(NULL),
      route_id_(route_id),
      message_loop_(NULL) {
}

AudioMessageFilter::~AudioMessageFilter() {
}

// Called on the IPC thread.
bool AudioMessageFilter::Send(IPC::Message* message) {
  if (!channel_) {
    delete message;
    return false;
  }

  if (MessageLoop::current() != message_loop_) {
    // Can only access the IPC::Channel on the IPC thread since it's not thread
    // safe.
    message_loop_->PostTask(
        FROM_HERE, NewRunnableMethod(this, &AudioMessageFilter::Send, message));
    return true;
  }

  message->set_routing_id(route_id_);
  return channel_->Send(message);
}

bool AudioMessageFilter::OnMessageReceived(const IPC::Message& message) {
  if (message.routing_id() != route_id_)
    return false;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AudioMessageFilter, message)
    IPC_MESSAGE_HANDLER(ViewMsg_RequestAudioPacket, OnRequestPacket)
    IPC_MESSAGE_HANDLER(ViewMsg_NotifyAudioStreamCreated, OnStreamCreated)
    IPC_MESSAGE_HANDLER(ViewMsg_NotifyLowLatencyAudioStreamCreated,
                        OnLowLatencyStreamCreated)
    IPC_MESSAGE_HANDLER(ViewMsg_NotifyAudioStreamStateChanged,
                        OnStreamStateChanged)
    IPC_MESSAGE_HANDLER(ViewMsg_NotifyAudioStreamVolume, OnStreamVolume)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void AudioMessageFilter::OnFilterAdded(IPC::Channel* channel) {
  // Captures the message loop for IPC.
  message_loop_ = MessageLoop::current();
  channel_ = channel;
}

void AudioMessageFilter::OnFilterRemoved() {
  channel_ = NULL;
}

void AudioMessageFilter::OnChannelClosing() {
  channel_ = NULL;
}

void AudioMessageFilter::OnRequestPacket(const IPC::Message& msg,
                                         int stream_id,
                                         AudioBuffersState buffers_state) {
  Delegate* delegate = delegates_.Lookup(stream_id);
  if (!delegate) {
    DLOG(WARNING) << "Got audio packet request for a non-existent or removed"
        " audio renderer.";
    return;
  }

  delegate->OnRequestPacket(buffers_state);
}

void AudioMessageFilter::OnStreamCreated(int stream_id,
                                         base::SharedMemoryHandle handle,
                                         uint32 length) {
  Delegate* delegate = delegates_.Lookup(stream_id);
  if (!delegate) {
    DLOG(WARNING) << "Got audio stream event for a non-existent or removed"
        " audio renderer.";
    return;
  }
  delegate->OnCreated(handle, length);
}

void AudioMessageFilter::OnLowLatencyStreamCreated(
    int stream_id,
    base::SharedMemoryHandle handle,
#if defined(OS_WIN)
    base::SyncSocket::Handle socket_handle,
#else
    base::FileDescriptor socket_descriptor,
#endif
    uint32 length) {
  Delegate* delegate = delegates_.Lookup(stream_id);
  if (!delegate) {
    DLOG(WARNING) << "Got audio stream event for a non-existent or removed"
        " audio renderer.";
    return;
  }
#if !defined(OS_WIN)
  base::SyncSocket::Handle socket_handle = socket_descriptor.fd;
#endif
  delegate->OnLowLatencyCreated(handle, socket_handle, length);
}

void AudioMessageFilter::OnStreamStateChanged(
    int stream_id,
    const ViewMsg_AudioStreamState_Params& state) {
  Delegate* delegate = delegates_.Lookup(stream_id);
  if (!delegate) {
    DLOG(WARNING) << "Got audio stream event for a non-existent or removed"
        " audio renderer.";
    return;
  }
  delegate->OnStateChanged(state);
}

void AudioMessageFilter::OnStreamVolume(int stream_id, double volume) {
  Delegate* delegate = delegates_.Lookup(stream_id);
  if (!delegate) {
    DLOG(WARNING) << "Got audio stream event for a non-existent or removed"
        " audio renderer.";
    return;
  }
  delegate->OnVolume(volume);
}

int32 AudioMessageFilter::AddDelegate(Delegate* delegate) {
  return delegates_.Add(delegate);
}

void AudioMessageFilter::RemoveDelegate(int32 id) {
  delegates_.Remove(id);
}
