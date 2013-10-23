// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/video_capture_message_filter.h"

#include "content/common/media/video_capture_messages.h"
#include "content/common/view_messages.h"

namespace content {

VideoCaptureMessageFilter::VideoCaptureMessageFilter()
    : last_device_id_(0),
      channel_(NULL) {
}

void VideoCaptureMessageFilter::AddDelegate(Delegate* delegate) {
  if (++last_device_id_ <= 0)
    last_device_id_ = 1;
  while (delegates_.find(last_device_id_) != delegates_.end())
    last_device_id_++;

  if (channel_) {
    delegates_[last_device_id_] = delegate;
    delegate->OnDelegateAdded(last_device_id_);
  } else {
    pending_delegates_[last_device_id_] = delegate;
  }
}

void VideoCaptureMessageFilter::RemoveDelegate(Delegate* delegate) {
  for (Delegates::iterator it = delegates_.begin();
       it != delegates_.end(); it++) {
    if (it->second == delegate) {
      delegates_.erase(it);
      break;
    }
  }
  for (Delegates::iterator it = pending_delegates_.begin();
       it != pending_delegates_.end(); it++) {
    if (it->second == delegate) {
      pending_delegates_.erase(it);
      break;
    }
  }
}

bool VideoCaptureMessageFilter::Send(IPC::Message* message) {
  if (!channel_) {
    delete message;
    return false;
  }

  return channel_->Send(message);
}

bool VideoCaptureMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(VideoCaptureMessageFilter, message)
    IPC_MESSAGE_HANDLER(VideoCaptureMsg_BufferReady, OnBufferReceived)
    IPC_MESSAGE_HANDLER(VideoCaptureMsg_StateChanged, OnDeviceStateChanged)
    IPC_MESSAGE_HANDLER(VideoCaptureMsg_NewBuffer, OnBufferCreated)
    IPC_MESSAGE_HANDLER(VideoCaptureMsg_FreeBuffer, OnBufferDestroyed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void VideoCaptureMessageFilter::OnFilterAdded(IPC::Channel* channel) {
  DVLOG(1) << "VideoCaptureMessageFilter::OnFilterAdded()";
  channel_ = channel;

  for (Delegates::iterator it = pending_delegates_.begin();
       it != pending_delegates_.end(); it++) {
    it->second->OnDelegateAdded(it->first);
    delegates_[it->first] = it->second;
  }
  pending_delegates_.clear();
}

void VideoCaptureMessageFilter::OnFilterRemoved() {
  channel_ = NULL;
}

void VideoCaptureMessageFilter::OnChannelClosing() {
  channel_ = NULL;
}

VideoCaptureMessageFilter::~VideoCaptureMessageFilter() {}

VideoCaptureMessageFilter::Delegate* VideoCaptureMessageFilter::find_delegate(
    int device_id) const {
  Delegates::const_iterator i = delegates_.find(device_id);
  return i != delegates_.end() ? i->second : NULL;
}

void VideoCaptureMessageFilter::OnBufferCreated(
    int device_id,
    base::SharedMemoryHandle handle,
    int length,
    int buffer_id) {
  Delegate* delegate = find_delegate(device_id);
  if (!delegate) {
    DLOG(WARNING) << "OnBufferCreated: Got video frame buffer for a "
        "non-existent or removed video capture.";

    // Send the buffer back to Host in case it's waiting for all buffers
    // to be returned.
    base::SharedMemory::CloseHandle(handle);
    Send(new VideoCaptureHostMsg_BufferReady(device_id, buffer_id));
    return;
  }

  delegate->OnBufferCreated(handle, length, buffer_id);
}

void VideoCaptureMessageFilter::OnBufferReceived(
    int device_id,
    int buffer_id,
    base::Time timestamp,
    const media::VideoCaptureFormat& format) {
  Delegate* delegate = find_delegate(device_id);
  if (!delegate) {
    DLOG(WARNING) << "OnBufferReceived: Got video frame buffer for a "
        "non-existent or removed video capture.";

    // Send the buffer back to Host in case it's waiting for all buffers
    // to be returned.
    Send(new VideoCaptureHostMsg_BufferReady(device_id, buffer_id));
    return;
  }

  delegate->OnBufferReceived(buffer_id, timestamp, format);
}

void VideoCaptureMessageFilter::OnBufferDestroyed(
    int device_id,
    int buffer_id) {
  Delegate* delegate = find_delegate(device_id);
  if (!delegate) {
    DLOG(WARNING) << "OnBufferDestroyed: Instructed to free buffer for a "
        "non-existent or removed video capture.";
    return;
  }

  delegate->OnBufferDestroyed(buffer_id);
}

void VideoCaptureMessageFilter::OnDeviceStateChanged(
    int device_id,
    VideoCaptureState state) {
  Delegate* delegate = find_delegate(device_id);
  if (!delegate) {
    DLOG(WARNING) << "OnDeviceStateChanged: Got video capture event for a "
        "non-existent or removed video capture.";
    return;
  }
  delegate->OnStateChanged(state);
}

}  // namespace content
