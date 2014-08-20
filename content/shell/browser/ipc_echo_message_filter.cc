// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/ipc_echo_message_filter.h"

#include "content/shell/common/shell_messages.h"

namespace content {

IPCEchoMessageFilter::IPCEchoMessageFilter()
    : BrowserMessageFilter(ShellMsgStart) {
}

IPCEchoMessageFilter::~IPCEchoMessageFilter() {
}

bool IPCEchoMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(IPCEchoMessageFilter, message)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_EchoPing, OnEchoPing)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void IPCEchoMessageFilter::OnEchoPing(int routing_id, int id,
                                    const std::string& body) {
  Send(new ShellViewMsg_EchoPong(routing_id, id, body));
}

}  // namespace content
