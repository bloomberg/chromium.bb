// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/layout_test_controller_host.h"

#include <iostream>

#include "base/message_loop.h"
#include "content/public/browser/render_view_host.h"
#include "content/shell/shell_messages.h"

namespace content {

namespace {
const int kTestTimeoutMilliseconds = 30 * 1000;
}  // namespace

std::map<RenderViewHost*, LayoutTestControllerHost*>
    LayoutTestControllerHost::controllers_;

// static
LayoutTestControllerHost* LayoutTestControllerHost::FromRenderViewHost(
    RenderViewHost* render_view_host) {
  const std::map<RenderViewHost*, LayoutTestControllerHost*>::iterator it =
      controllers_.find(render_view_host);
  if (it == controllers_.end())
    return NULL;
  return it->second;
}

LayoutTestControllerHost::LayoutTestControllerHost(
    RenderViewHost* render_view_host)
    : RenderViewHostObserver(render_view_host),
      captured_dump_(false),
      dump_as_text_(false),
      dump_child_frames_(false),
      is_printing_(false),
      should_stay_on_page_after_handling_before_unload_(false),
      wait_until_done_(false) {
  controllers_[render_view_host] = this;
}

LayoutTestControllerHost::~LayoutTestControllerHost() {
  controllers_.erase(render_view_host());
  watchdog_.Cancel();
}

void LayoutTestControllerHost::CaptureDump() {
  if (captured_dump_)
    return;
  captured_dump_ = true;

  render_view_host()->Send(
      new ShellViewMsg_CaptureTextDump(render_view_host()->GetRoutingID(),
                                       dump_as_text_,
                                       is_printing_,
                                       dump_child_frames_));
}

void LayoutTestControllerHost::TimeoutHandler() {
  std::cout << "FAIL: Timed out waiting for notifyDone to be called\n";
  std::cerr << "FAIL: Timed out waiting for notifyDone to be called\n";
  CaptureDump();
}

bool LayoutTestControllerHost::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(LayoutTestControllerHost, message)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_DidFinishLoad, OnDidFinishLoad)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_TextDump, OnTextDump)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_NotifyDone, OnNotifyDone)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_DumpAsText, OnDumpAsText)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_DumpChildFramesAsText,
                        OnDumpChildFramesAsText)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_SetPrinting, OnSetPrinting)
    IPC_MESSAGE_HANDLER(
        ShellViewHostMsg_SetShouldStayOnPageAfterHandlingBeforeUnload,
        OnSetShouldStayOnPageAfterHandlingBeforeUnload)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_WaitUntilDone, OnWaitUntilDone)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void LayoutTestControllerHost::OnDidFinishLoad() {
  if (wait_until_done_)
    return;

  CaptureDump();
}

void LayoutTestControllerHost::OnTextDump(const std::string& dump) {
  std::cout << dump;
  std::cout << "#EOF\n";
  std::cerr << "#EOF\n";

  MessageLoop::current()->PostTask(FROM_HERE, MessageLoop::QuitClosure());
}

void LayoutTestControllerHost::OnNotifyDone() {
  if (!wait_until_done_)
    return;
  watchdog_.Cancel();
  CaptureDump();
}

void LayoutTestControllerHost::OnDumpAsText() {
  dump_as_text_ = true;
}

void LayoutTestControllerHost::OnSetPrinting() {
  is_printing_ = true;
}

void LayoutTestControllerHost::OnSetShouldStayOnPageAfterHandlingBeforeUnload(
    bool should_stay_on_page) {
  should_stay_on_page_after_handling_before_unload_ = should_stay_on_page;
}

void LayoutTestControllerHost::OnDumpChildFramesAsText() {
  dump_child_frames_ = true;
}

void LayoutTestControllerHost::OnWaitUntilDone() {
  if (wait_until_done_)
    return;
  watchdog_.Reset(base::Bind(&LayoutTestControllerHost::TimeoutHandler,
                             base::Unretained(this)));
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      watchdog_.callback(),
      base::TimeDelta::FromMilliseconds(kTestTimeoutMilliseconds));
  wait_until_done_ = true;
}

}  // namespace content
