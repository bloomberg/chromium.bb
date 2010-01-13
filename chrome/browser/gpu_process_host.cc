// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gpu_process_host.h"

#include "base/command_line.h"
#include "base/singleton.h"
#include "base/thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/child_process_launcher.h"
#include "chrome/common/child_process_host.h"
#include "chrome/common/child_process_info.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/gpu_messages.h"
#include "ipc/ipc_switches.h"

GpuProcessHost::GpuProcessHost() : last_routing_id_(1) {
  FilePath exe_path = ChildProcessHost::GetChildPath();
  if (exe_path.empty())
    return;

  std::string channel_id = ChildProcessInfo::GenerateRandomChannelID(this);
  channel_.reset(new IPC::ChannelProxy(
      channel_id,
      IPC::Channel::MODE_SERVER,
      this,
      NULL,  // No filter (for now).
      g_browser_process->io_thread()->message_loop()));

  CommandLine* cmd_line = new CommandLine(exe_path);
  cmd_line->AppendSwitchWithValue(switches::kProcessType,
                                  switches::kGpuProcess);
  cmd_line->AppendSwitchWithValue(switches::kProcessChannelID,
                                  ASCIIToWide(channel_id));

  // Spawn the child process asynchronously to avoid blocking the UI thread.
  child_process_.reset(new ChildProcessLauncher(
#if defined(OS_WIN)
      FilePath(),
#elif defined(POSIX)
      base::environment_vector(),
      channel_->GetClientFileDescriptor(),
#endif
      cmd_line,
      this));
}

GpuProcessHost::~GpuProcessHost() {
}

// static
GpuProcessHost* GpuProcessHost::Get() {
  GpuProcessHost* host = Singleton<GpuProcessHost>::get();
  if (!host->child_process_.get())
    return NULL;  // Failed to init.
  return host;
}

int32 GpuProcessHost::GetNextRoutingId() {
  return ++last_routing_id_;
}

int32 GpuProcessHost::NewRenderWidgetHostView(gfx::NativeViewId parent) {
  int32 routing_id = GetNextRoutingId();
  Send(new GpuMsg_NewRenderWidgetHostView(parent, routing_id));
  return routing_id;
}

bool GpuProcessHost::Send(IPC::Message* msg) {
  if (!channel_.get()) {
    delete msg;
    return false;
  }

  if (child_process_.get() && child_process_->IsStarting()) {
    queued_messages_.push(msg);
    return true;
  }

  return channel_->Send(msg);
}

void GpuProcessHost::OnMessageReceived(const IPC::Message& message) {
  if (message.routing_id() == MSG_ROUTING_CONTROL) {
    // We don't currently have any control messages.
    // OnControlMessageReceived(message);
  } else {
    router_.OnMessageReceived(message);
  }
}

void GpuProcessHost::OnChannelConnected(int32 peer_pid) {
}

void GpuProcessHost::OnChannelError() {
}

void GpuProcessHost::OnProcessLaunched() {
  while (!queued_messages_.empty()) {
    Send(queued_messages_.front());
    queued_messages_.pop();
  }
}

void GpuProcessHost::AddRoute(int32 routing_id,
                              IPC::Channel::Listener* listener) {
  router_.AddRoute(routing_id, listener);
}

void GpuProcessHost::RemoveRoute(int32 routing_id) {
  router_.RemoveRoute(routing_id);
}
