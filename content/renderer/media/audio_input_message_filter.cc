// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/audio_input_message_filter.h"

#include "base/bind.h"
#include "base/message_loop_proxy.h"
#include "content/common/media/audio_messages.h"
#include "ipc/ipc_logging.h"

namespace content {

AudioInputMessageFilter* AudioInputMessageFilter::filter_ = NULL;

AudioInputMessageFilter::AudioInputMessageFilter(
    const scoped_refptr<base::MessageLoopProxy>& io_message_loop)
    : channel_(NULL),
      io_message_loop_(io_message_loop) {
  DCHECK(!filter_);
  filter_ = this;
}

AudioInputMessageFilter::~AudioInputMessageFilter() {
  DCHECK_EQ(filter_, this);
  filter_ = NULL;
}

// static.
AudioInputMessageFilter* AudioInputMessageFilter::Get() {
  return filter_;
}

void AudioInputMessageFilter::Send(IPC::Message* message) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  if (!channel_) {
    delete message;
  } else {
    channel_->Send(message);
  }
}

bool AudioInputMessageFilter::OnMessageReceived(const IPC::Message& message) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AudioInputMessageFilter, message)
    IPC_MESSAGE_HANDLER(AudioInputMsg_NotifyStreamCreated,
                        OnStreamCreated)
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
  DCHECK(io_message_loop_->BelongsToCurrentThread());

  // Captures the channel for IPC.
  channel_ = channel;
}

void AudioInputMessageFilter::OnFilterRemoved() {
  DCHECK(io_message_loop_->BelongsToCurrentThread());

  // Once removed, a filter will not be used again.  At this time all
  // delegates must be notified so they release their reference.
  OnChannelClosing();
}

void AudioInputMessageFilter::OnChannelClosing() {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  channel_ = NULL;

  DLOG_IF(WARNING, !delegates_.IsEmpty())
      << "Not all audio devices have been closed.";

  IDMap<media::AudioInputIPCDelegate>::iterator it(&delegates_);
  while (!it.IsAtEnd()) {
    it.GetCurrentValue()->OnIPCClosed();
    delegates_.Remove(it.GetCurrentKey());
    it.Advance();
  }
}

void AudioInputMessageFilter::OnStreamCreated(
    int stream_id,
    base::SharedMemoryHandle handle,
#if defined(OS_WIN)
    base::SyncSocket::Handle socket_handle,
#else
    base::FileDescriptor socket_descriptor,
#endif
    uint32 length) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());

#if !defined(OS_WIN)
  base::SyncSocket::Handle socket_handle = socket_descriptor.fd;
#endif
  media::AudioInputIPCDelegate* delegate = delegates_.Lookup(stream_id);
  if (!delegate) {
    DLOG(WARNING) << "Got audio stream event for a non-existent or removed"
                  << " audio capturer (stream_id=" << stream_id << ").";
    base::SharedMemory::CloseHandle(handle);
    base::SyncSocket socket(socket_handle);
    return;
  }
  // Forward message to the stream delegate.
  delegate->OnStreamCreated(handle, socket_handle, length);
}

void AudioInputMessageFilter::OnStreamVolume(int stream_id, double volume) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  media::AudioInputIPCDelegate* delegate = delegates_.Lookup(stream_id);
  if (!delegate) {
    DLOG(WARNING) << "Got audio stream event for a non-existent or removed"
                  << " audio capturer.";
    return;
  }
  delegate->OnVolume(volume);
}

void AudioInputMessageFilter::OnStreamStateChanged(
    int stream_id, media::AudioInputIPCDelegate::State state) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  media::AudioInputIPCDelegate* delegate = delegates_.Lookup(stream_id);
  if (!delegate) {
    DLOG(WARNING) << "Got audio stream event for a non-existent or removed"
                  << " audio renderer.";
    return;
  }
  delegate->OnStateChanged(state);
}

void AudioInputMessageFilter::OnDeviceStarted(int stream_id,
                                              const std::string& device_id) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  media::AudioInputIPCDelegate* delegate = delegates_.Lookup(stream_id);
  if (!delegate) {
    NOTREACHED();
    return;
  }
  delegate->OnDeviceReady(device_id);
}

int AudioInputMessageFilter::AddDelegate(
    media::AudioInputIPCDelegate* delegate) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  return delegates_.Add(delegate);
}

void AudioInputMessageFilter::RemoveDelegate(int id) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  delegates_.Remove(id);
}

void AudioInputMessageFilter::CreateStream(int stream_id,
                                           const media::AudioParameters& params,
                                           const std::string& device_id,
                                           bool automatic_gain_control) {
  Send(new AudioInputHostMsg_CreateStream(
      stream_id, params, device_id, automatic_gain_control));
}

void AudioInputMessageFilter::AssociateStreamWithConsumer(int stream_id,
                                                          int render_view_id) {
  Send(new AudioInputHostMsg_AssociateStreamWithConsumer(
      stream_id, render_view_id));
}

void AudioInputMessageFilter::StartDevice(int stream_id, int session_id) {
  Send(new AudioInputHostMsg_StartDevice(stream_id, session_id));
}

void AudioInputMessageFilter::RecordStream(int stream_id) {
  Send(new AudioInputHostMsg_RecordStream(stream_id));
}

void AudioInputMessageFilter::CloseStream(int stream_id) {
  Send(new AudioInputHostMsg_CloseStream(stream_id));
}

void AudioInputMessageFilter::SetVolume(int stream_id, double volume) {
  Send(new AudioInputHostMsg_SetVolume(stream_id, volume));
}

}  // namespace content
