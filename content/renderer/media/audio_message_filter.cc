// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/audio_message_filter.h"

#include "base/bind.h"
#include "base/message_loop_proxy.h"
#include "content/common/media/audio_messages.h"
#include "content/renderer/render_thread_impl.h"
#include "ipc/ipc_logging.h"

namespace content {

AudioMessageFilter* AudioMessageFilter::filter_ = NULL;

// static
AudioMessageFilter* AudioMessageFilter::Get() {
  return filter_;
}

AudioMessageFilter::AudioMessageFilter(
    const scoped_refptr<base::MessageLoopProxy>& io_message_loop)
    : channel_(NULL),
      audio_hardware_config_(NULL),
      io_message_loop_(io_message_loop) {
  DCHECK(!filter_);
  filter_ = this;
}

int AudioMessageFilter::AddDelegate(media::AudioOutputIPCDelegate* delegate) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  return delegates_.Add(delegate);
}

void AudioMessageFilter::RemoveDelegate(int id) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  delegates_.Remove(id);
}

void AudioMessageFilter::CreateStream(int stream_id,
                                      const media::AudioParameters& params) {
  Send(new AudioHostMsg_CreateStream(stream_id, params));
}

void AudioMessageFilter::AssociateStreamWithProducer(int stream_id,
                                                     int render_view_id) {
  Send(new AudioHostMsg_AssociateStreamWithProducer(stream_id, render_view_id));
}

void AudioMessageFilter::PlayStream(int stream_id) {
  Send(new AudioHostMsg_PlayStream(stream_id));
}

void AudioMessageFilter::PauseStream(int stream_id) {
  Send(new AudioHostMsg_PauseStream(stream_id));
}

void AudioMessageFilter::FlushStream(int stream_id) {
  Send(new AudioHostMsg_FlushStream(stream_id));
}

void AudioMessageFilter::CloseStream(int stream_id) {
  Send(new AudioHostMsg_CloseStream(stream_id));
}

void AudioMessageFilter::SetVolume(int stream_id, double volume) {
  Send(new AudioHostMsg_SetVolume(stream_id, volume));
}

void AudioMessageFilter::Send(IPC::Message* message) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  if (!channel_) {
    delete message;
  } else {
    channel_->Send(message);
  }
}

bool AudioMessageFilter::OnMessageReceived(const IPC::Message& message) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AudioMessageFilter, message)
    IPC_MESSAGE_HANDLER(AudioMsg_NotifyStreamCreated, OnStreamCreated)
    IPC_MESSAGE_HANDLER(AudioMsg_NotifyStreamStateChanged, OnStreamStateChanged)
    IPC_MESSAGE_HANDLER(AudioMsg_NotifyDeviceChanged, OnOutputDeviceChanged)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void AudioMessageFilter::OnFilterAdded(IPC::Channel* channel) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  channel_ = channel;
}

void AudioMessageFilter::OnFilterRemoved() {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  channel_ = NULL;
}

void AudioMessageFilter::OnChannelClosing() {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  channel_ = NULL;

  DLOG_IF(WARNING, !delegates_.IsEmpty())
      << "Not all audio devices have been closed.";

  IDMap<media::AudioOutputIPCDelegate>::iterator it(&delegates_);
  while (!it.IsAtEnd()) {
    it.GetCurrentValue()->OnIPCClosed();
    delegates_.Remove(it.GetCurrentKey());
    it.Advance();
  }
}

AudioMessageFilter::~AudioMessageFilter() {
  CHECK(delegates_.IsEmpty());
  DCHECK_EQ(filter_, this);
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
  DCHECK(io_message_loop_->BelongsToCurrentThread());

#if !defined(OS_WIN)
  base::SyncSocket::Handle socket_handle = socket_descriptor.fd;
#endif

  media::AudioOutputIPCDelegate* delegate = delegates_.Lookup(stream_id);
  if (!delegate) {
    DLOG(WARNING) << "Got OnStreamCreated() event for a non-existent or removed"
                  << " audio renderer. (stream_id=" << stream_id << ").";
    base::SharedMemory::CloseHandle(handle);
    base::SyncSocket socket(socket_handle);
    return;
  }
  delegate->OnStreamCreated(handle, socket_handle, length);
}

void AudioMessageFilter::OnStreamStateChanged(
    int stream_id, media::AudioOutputIPCDelegate::State state) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  media::AudioOutputIPCDelegate* delegate = delegates_.Lookup(stream_id);
  if (!delegate) {
    DLOG(WARNING) << "Got OnStreamStateChanged() event for a non-existent or"
                  << " removed audio renderer.  State: " << state;
    return;
  }
  delegate->OnStateChanged(state);
}

void AudioMessageFilter::OnOutputDeviceChanged(int stream_id,
                                               int new_buffer_size,
                                               int new_sample_rate) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  base::AutoLock auto_lock(lock_);

  // Ignore the message if an audio hardware config hasn't been created; this
  // can occur if the renderer is using the high latency audio path.
  // TODO(dalecurtis): After http://crbug.com/173435 is fixed, convert to CHECK.
  if (!audio_hardware_config_)
    return;

  audio_hardware_config_->UpdateOutputConfig(new_buffer_size, new_sample_rate);
}

void AudioMessageFilter::SetAudioHardwareConfig(
    media::AudioHardwareConfig* config) {
  base::AutoLock auto_lock(lock_);
  audio_hardware_config_ = config;
}

}  // namespace content
