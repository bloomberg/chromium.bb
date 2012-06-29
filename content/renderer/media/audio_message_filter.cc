// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/audio_message_filter.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/time.h"
#include "content/common/child_process.h"
#include "content/common/media/audio_messages.h"
#include "content/renderer/render_thread_impl.h"
#include "ipc/ipc_logging.h"

AudioMessageFilter* AudioMessageFilter::filter_ = NULL;

// static
AudioMessageFilter* AudioMessageFilter::Get() {
  return filter_;
}

AudioMessageFilter::AudioMessageFilter()
    : channel_(NULL) {
  VLOG(1) << "AudioMessageFilter::AudioMessageFilter()";
  DCHECK(!filter_);
  filter_ = this;
}

int32 AudioMessageFilter::AddDelegate(Delegate* delegate) {
  return delegates_.Add(delegate);
}

void AudioMessageFilter::RemoveDelegate(int32 id) {
  delegates_.Remove(id);
}

bool AudioMessageFilter::Send(IPC::Message* message) {
  if (!channel_) {
    delete message;
    return false;
  }

  if (MessageLoop::current() != ChildProcess::current()->io_message_loop()) {
    // Can only access the IPC::Channel on the IPC thread since it's not thread
    // safe.
    ChildProcess::current()->io_message_loop()->PostTask(
        FROM_HERE,
        base::Bind(base::IgnoreResult(&AudioMessageFilter::Send), this,
                   message));
    return true;
  }

  return channel_->Send(message);
}

bool AudioMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AudioMessageFilter, message)
    IPC_MESSAGE_HANDLER(AudioMsg_NotifyStreamCreated, OnStreamCreated)
    IPC_MESSAGE_HANDLER(AudioMsg_NotifyStreamStateChanged, OnStreamStateChanged)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void AudioMessageFilter::OnFilterAdded(IPC::Channel* channel) {
  VLOG(1) << "AudioMessageFilter::OnFilterAdded()";
  // Captures the channel for IPC.
  channel_ = channel;
}

void AudioMessageFilter::OnFilterRemoved() {
  channel_ = NULL;
}

void AudioMessageFilter::OnChannelClosing() {
  channel_ = NULL;
}

AudioMessageFilter::~AudioMessageFilter() {
  VLOG(1) << "AudioMessageFilter::~AudioMessageFilter()";
  DCHECK(filter_);
  filter_ = NULL;
}

void AudioMessageFilter::OnStreamCreated(
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
        " audio renderer. (stream_id=" << stream_id << ").";
    base::SharedMemory::CloseHandle(handle);
    base::SyncSocket socket(socket_handle);
    return;
  }
  delegate->OnStreamCreated(handle, socket_handle, length);
}

void AudioMessageFilter::OnStreamStateChanged(
    int stream_id, AudioStreamState state) {
  Delegate* delegate = delegates_.Lookup(stream_id);
  if (!delegate) {
    DLOG(WARNING) << "Got audio stream event for a non-existent or removed"
        " audio renderer.";
    return;
  }
  delegate->OnStateChanged(state);
}
