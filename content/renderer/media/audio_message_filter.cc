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

namespace content {

AudioMessageFilter* AudioMessageFilter::filter_ = NULL;

// static
AudioMessageFilter* AudioMessageFilter::Get() {
  return filter_;
}

AudioMessageFilter::AudioMessageFilter()
    : next_stream_id_(1),
      channel_(NULL) {
  DVLOG(1) << "AudioMessageFilter::AudioMessageFilter()";
  DCHECK(!filter_);
  filter_ = this;
}

int AudioMessageFilter::AddDelegate(media::AudioOutputIPCDelegate* delegate) {
  base::AutoLock guard(delegates_lock_);
  const int id = next_stream_id_++;
  delegates_.insert(std::make_pair(id, delegate));
  return id;
}

void AudioMessageFilter::RemoveDelegate(int id) {
  base::AutoLock guard(delegates_lock_);
  delegates_.erase(id);
}

void AudioMessageFilter::CreateStream(int stream_id,
                                      const media::AudioParameters& params,
                                      int input_channels) {
  Send(new AudioHostMsg_CreateStream(stream_id, params, input_channels));
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
  DVLOG(1) << "AudioMessageFilter::OnFilterAdded()";
  channel_ = channel;
}

void AudioMessageFilter::OnFilterRemoved() {
  channel_ = NULL;
}

void AudioMessageFilter::OnChannelClosing() {
  channel_ = NULL;

  DelegateMap zombies;
  {
    base::AutoLock guard(delegates_lock_);
    delegates_.swap(zombies);
  }

  LOG_IF(WARNING, !zombies.empty())
      << "Not all audio devices have been closed.";

  for (DelegateMap::const_iterator it = zombies.begin(); it != zombies.end();
       ++it) {
    it->second->OnIPCClosed();
  }
}

AudioMessageFilter::~AudioMessageFilter() {
  DVLOG(1) << "AudioMessageFilter::~AudioMessageFilter()";

  // Just in case the message filter is deleted before the channel
  // is closed and there are still living audio devices.
  OnChannelClosing();

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

  {
    base::AutoLock guard(delegates_lock_);
    DelegateMap::const_iterator it = delegates_.find(stream_id);
    if (it != delegates_.end()) {
      it->second->OnStreamCreated(handle, socket_handle, length);
      return;
    }
  }

  DLOG(WARNING) << "Got audio stream event for a non-existent or removed"
                   " audio renderer. (stream_id=" << stream_id << ").";
  base::SharedMemory::CloseHandle(handle);
  base::SyncSocket socket(socket_handle);
}

void AudioMessageFilter::OnStreamStateChanged(
    int stream_id, media::AudioOutputIPCDelegate::State state) {
  base::AutoLock guard(delegates_lock_);
  DelegateMap::const_iterator it = delegates_.find(stream_id);
  DLOG_IF(WARNING, it == delegates_.end())
      << "No delegate found for state change. " << state;
  if (it != delegates_.end())
    it->second->OnStateChanged(state);
}

}  // namespace content
