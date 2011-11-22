// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/audio_input_message_filter.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/time.h"
#include "content/common/child_process.h"
#include "content/common/media/audio_messages.h"
#include "ipc/ipc_logging.h"

AudioInputMessageFilter::AudioInputMessageFilter()
    : channel_(NULL) {
  VLOG(1) << "AudioInputMessageFilter()";
}

AudioInputMessageFilter::~AudioInputMessageFilter() {
  VLOG(1) << "AudioInputMessageFilter::~AudioInputMessageFilter()";
}

bool AudioInputMessageFilter::Send(IPC::Message* message) {
  if (!channel_) {
    delete message;
    return false;
  }

  if (MessageLoop::current() != ChildProcess::current()->io_message_loop()) {
    // Can only access the IPC::Channel on the IPC thread since it's not thread
    // safe.
    ChildProcess::current()->io_message_loop()->PostTask(
        FROM_HERE,
        base::IgnoreReturn<bool>(base::Bind(&AudioInputMessageFilter::Send,
                                            this, message)));
    return true;
  }

  return channel_->Send(message);
}

bool AudioInputMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AudioInputMessageFilter, message)
    IPC_MESSAGE_HANDLER(AudioInputMsg_NotifyLowLatencyStreamCreated,
                        OnLowLatencyStreamCreated)
    IPC_MESSAGE_HANDLER(AudioInputMsg_NotifyStreamVolume, OnStreamVolume)
    IPC_MESSAGE_HANDLER(AudioInputMsg_NotifyStreamStateChanged,
                        OnStreamStateChanged)
    IPC_MESSAGE_HANDLER(AudioInputMsg_NotifyDeviceStarted,
                        OnDeviceStarted)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void AudioInputMessageFilter::OnFilterAdded(IPC::Channel* channel) {
  VLOG(1) << "AudioInputMessageFilter::OnFilterAdded()";
  // Captures the channel for IPC.
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
#if !defined(OS_WIN)
  base::SyncSocket::Handle socket_handle = socket_descriptor.fd;
#endif
  Delegate* delegate = delegates_.Lookup(stream_id);
  if (!delegate) {
    DLOG(WARNING) << "Got audio stream event for a non-existent or removed"
        " audio capturer (stream_id=" << stream_id << ").";
    base::SharedMemory::CloseHandle(handle);
    base::SyncSocket socket(socket_handle);
    return;
  }
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

void AudioInputMessageFilter::OnStreamStateChanged(
    int stream_id, AudioStreamState state) {
  Delegate* delegate = delegates_.Lookup(stream_id);
  if (!delegate) {
    DLOG(WARNING) << "Got audio stream event for a non-existent or removed"
        " audio renderer.";
    return;
  }
  delegate->OnStateChanged(state);
}

void AudioInputMessageFilter::OnDeviceStarted(int stream_id,
                                              const std::string& device_id) {
  Delegate* delegate = delegates_.Lookup(stream_id);
  if (!delegate) {
    DLOG(WARNING) << "Got audio stream event for a non-existent or removed"
        " audio renderer.";
    return;
  }
  delegate->OnDeviceReady(device_id);
}

int32 AudioInputMessageFilter::AddDelegate(Delegate* delegate) {
  return delegates_.Add(delegate);
}

void AudioInputMessageFilter::RemoveDelegate(int32 id) {
  VLOG(1) << "AudioInputMessageFilter::RemoveDelegate(id=" << id << ")";
  delegates_.Remove(id);
}
