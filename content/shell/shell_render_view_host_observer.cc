// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/shell_render_view_host_observer.h"

#include <iostream>

#include "content/shell/shell_messages.h"

namespace content {

ShellRenderViewHostObserver::ShellRenderViewHostObserver(
    RenderViewHost* render_view_host)
    : RenderViewHostObserver(render_view_host) {
}

ShellRenderViewHostObserver::~ShellRenderViewHostObserver() {
}

bool ShellRenderViewHostObserver::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ShellRenderViewHostObserver, message)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_TextDump, OnTextDump)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ShellRenderViewHostObserver::OnTextDump(const std::string& dump) {
  std::cout << dump;
}

}  // namespace content
