// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_io_message_filter.h"

#include "base/logging.h"
#include "base/process_util.h"
#include "chrome/browser/renderer_host/browser_render_process_host.h"

BrowserIOMessageFilter::BrowserIOMessageFilter() : channel_(NULL) {
}

BrowserIOMessageFilter::~BrowserIOMessageFilter() {
}

void BrowserIOMessageFilter::OnFilterAdded(IPC::Channel* channel) {
  channel_ = channel;
}

void BrowserIOMessageFilter::OnChannelClosing() {
  channel_ = NULL;
}

void BrowserIOMessageFilter::OnChannelConnected(int32 peer_pid) {
  if (!base::OpenProcessHandle(peer_pid, &peer_handle_)) {
    NOTREACHED();
  }
}

bool BrowserIOMessageFilter::Send(IPC::Message* msg) {
  if (channel_)
    return channel_->Send(msg);

  delete msg;
  return false;
}

void BrowserIOMessageFilter::BadMessageReceived(uint32 msg_type) {
  BrowserRenderProcessHost::BadMessageTerminateProcess(msg_type, peer_handle());
}
