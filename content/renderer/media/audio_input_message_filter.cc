// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/audio_input_message_filter.h"

#include "base/message_loop.h"
#include "base/time.h"
#include "content/common/audio_messages.h"
#include "ipc/ipc_logging.h"

AudioInputMessageFilter::AudioInputMessageFilter(int32 route_id)
    : channel_(NULL),
      route_id_(route_id),
      message_loop_(NULL) {
  VLOG(1) << "AudioInputMessageFilter(route_id=" << route_id << ")";
}

AudioInputMessageFilter::~AudioInputMessageFilter() {}

// Called on the IPC thread.
bool AudioInputMessageFilter::Send(IPC::Message* message) {
  if (!channel_) {
    delete message;
    return false;
  }

  if (MessageLoop::current() != message_loop_) {
    // Can only access the IPC::Channel on the IPC thread since it's not thread
    // safe.
    message_loop_->PostTask(
        FROM_HERE, NewRunnableMethod(this,
                                     &AudioInputMessageFilter::Send,
                                     message));
    return true;
  }

  message->set_routing_id(route_id_);
  return channel_->Send(message);
}

bool AudioInputMessageFilter::OnMessageReceived(const IPC::Message& message) {
  if (message.routing_id() != route_id_)
    return false;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AudioInputMessageFilter, message)
    IPC_MESSAGE_HANDLER(AudioInputMsg_NotifyLowLatencyStreamCreated,
                        OnLowLatencyStreamCreated)
    IPC_MESSAGE_HANDLER(AudioInputMsg_NotifyStreamVolume, OnStreamVolume)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void AudioInputMessageFilter::OnFilterAdded(IPC::Channel* channel) {
  VLOG(1) << "AudioInputMessageFilter::OnFilterAdded()";
  // Captures the message loop for IPC.
  message_loop_ = MessageLoop::current();
  channel_ = channel;
}

void AudioInputMessageFilter::OnFilterRemoved() {
  channel_ = NULL;
}

void AudioInputMessageFilter::OnChannelClosing() {
  channel_ = NULL;
}

void AudioInputMessageFilter::OnLowLatencyStreamCreated(
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
        " audio capturer.";
    return;
  }
#if !defined(OS_WIN)
  base::SyncSocket::Handle socket_handle = socket_descriptor.fd;
#endif
  // Forward message to the stream delegate.
  delegate->OnLowLatencyCreated(handle, socket_handle, length);
}

void AudioInputMessageFilter::OnStreamVolume(int stream_id, double volume) {
  Delegate* delegate = delegates_.Lookup(stream_id);
  if (!delegate) {
    DLOG(WARNING) << "Got audio stream event for a non-existent or removed"
        " audio capturer.";
    return;
  }
  delegate->OnVolume(volume);
}

int32 AudioInputMessageFilter::AddDelegate(Delegate* delegate) {
  return delegates_.Add(delegate);
}

void AudioInputMessageFilter::RemoveDelegate(int32 id) {
  delegates_.Remove(id);
}
