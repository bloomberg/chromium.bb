// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/shell_render_view_host_observer.h"

#include <iostream>

#include "base/message_loop.h"
#include "content/public/browser/render_view_host.h"
#include "content/shell/shell.h"
#include "content/shell/shell_messages.h"

namespace content {

ShellRenderViewHostObserver::ShellRenderViewHostObserver(
    RenderViewHost* render_view_host)
    : RenderViewHostObserver(render_view_host),
      dump_child_frames_(false) {
}

ShellRenderViewHostObserver::~ShellRenderViewHostObserver() {
}

bool ShellRenderViewHostObserver::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ShellRenderViewHostObserver, message)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_TextDump, OnTextDump)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_NotifyDone, OnNotifyDone)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_DumpAsText, OnDumpAsText)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_DumpChildFramesAsText,
                        OnDumpChildFramesAsText)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_WaitUntilDone, OnWaitUntilDone)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ShellRenderViewHostObserver::OnTextDump(const std::string& dump) {
  std::cout << "Content-Type: text/plain\n";
  std::cout << dump;
  std::cout << "#EOF\n";
  std::cerr << "#EOF\n";

  MessageLoop::current()->PostTask(FROM_HERE, MessageLoop::QuitClosure());
}

void ShellRenderViewHostObserver::OnNotifyDone() {
  render_view_host()->Send(
      new ShellViewMsg_CaptureTextDump(render_view_host()->GetRoutingID(),
                                       dump_child_frames_));
}

void ShellRenderViewHostObserver::OnDumpAsText() {
}

void ShellRenderViewHostObserver::OnDumpChildFramesAsText() {
  dump_child_frames_ = true;
}

void ShellRenderViewHostObserver::OnWaitUntilDone() {
  Shell* shell = Shell::FromRenderViewHost(render_view_host());
  shell->set_wait_until_done();
}

}  // namespace content
