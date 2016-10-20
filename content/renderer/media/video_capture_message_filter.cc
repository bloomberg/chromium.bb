// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/video_capture_message_filter.h"

#include "content/common/media/video_capture_messages.h"
#include "content/common/view_messages.h"
#include "ipc/ipc_sender.h"

namespace content {

VideoCaptureMessageFilter::VideoCaptureMessageFilter()
    : last_device_id_(0), channel_(nullptr) {}

void VideoCaptureMessageFilter::AddDelegate(Delegate* delegate) {
  DVLOG(1) << __func__;
  last_device_id_++;
  DCHECK_GE(last_device_id_, 1);
  DCHECK(!base::ContainsKey(delegates_, last_device_id_));

  if (channel_) {
    delegates_[last_device_id_] = delegate;
    delegate->OnDelegateAdded(last_device_id_);
  } else {
    pending_delegates_[last_device_id_] = delegate;
  }
}

void VideoCaptureMessageFilter::RemoveDelegate(Delegate* delegate) {
  DVLOG(1) << __func__;
  for (Delegates::iterator it = delegates_.begin(); it != delegates_.end();
       it++) {
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

bool VideoCaptureMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(VideoCaptureMessageFilter, message)
    IPC_MESSAGE_HANDLER(VideoCaptureMsg_Noop, DoNothing)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void VideoCaptureMessageFilter::OnFilterAdded(IPC::Channel* channel) {
  DVLOG(1) << __func__;
  channel_ = channel;
  for (const auto& pending_delegate : pending_delegates_) {
    pending_delegate.second->OnDelegateAdded(pending_delegate.first);
    delegates_[pending_delegate.first] = pending_delegate.second;
  }
  pending_delegates_.clear();
}

void VideoCaptureMessageFilter::OnFilterRemoved() {
  channel_ = nullptr;
}

void VideoCaptureMessageFilter::OnChannelClosing() {
  channel_ = nullptr;
}

VideoCaptureMessageFilter::~VideoCaptureMessageFilter() {}

}  // namespace content
