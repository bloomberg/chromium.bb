// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ppapi_plugin_process_host.h"

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/process_util.h"
#include "chrome/browser/renderer_host/render_message_filter.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/render_messages.h"
#include "ipc/ipc_switches.h"
#include "ppapi/proxy/ppapi_messages.h"

PpapiPluginProcessHost::PpapiPluginProcessHost(RenderMessageFilter* filter)
    : BrowserChildProcessHost(ChildProcessInfo::PPAPI_PLUGIN_PROCESS,
                              filter->resource_dispatcher_host()),
      filter_(filter) {
}

PpapiPluginProcessHost::~PpapiPluginProcessHost() {
}

void PpapiPluginProcessHost::Init(const FilePath& path,
                                  IPC::Message* reply_msg) {
  plugin_path_ = path;
  reply_msg_.reset(reply_msg);

  if (!CreateChannel()) {
    ReplyToRenderer(NULL, IPC::ChannelHandle());
    return;
  }

  const CommandLine& browser_command_line = *CommandLine::ForCurrentProcess();
  CommandLine::StringType plugin_launcher =
      browser_command_line.GetSwitchValueNative(switches::kPpapiPluginLauncher);

  FilePath exe_path = ChildProcessHost::GetChildPath(plugin_launcher.empty());
  if (exe_path.empty()) {
    ReplyToRenderer(NULL, IPC::ChannelHandle());
    return;
  }

  CommandLine* cmd_line = new CommandLine(exe_path);
  cmd_line->AppendSwitchASCII(switches::kProcessType,
                              switches::kPpapiPluginProcess);
  cmd_line->AppendSwitchASCII(switches::kProcessChannelID, channel_id());

  if (!plugin_launcher.empty())
    cmd_line->PrependWrapper(plugin_launcher);

  // On posix, having a plugin launcher means we need to use another process
  // instead of just forking the zygote.
  Launch(
#if defined(OS_WIN)
      FilePath(),
#elif defined(OS_POSIX)
      plugin_launcher.empty(),
      base::environment_vector(),
#endif
      cmd_line);
}

bool PpapiPluginProcessHost::CanShutdown() {
  return true;
}

void PpapiPluginProcessHost::OnProcessLaunched() {
}

bool PpapiPluginProcessHost::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PpapiPluginProcessHost, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PluginLoaded, OnPluginLoaded)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled);
  return handled;
}

void PpapiPluginProcessHost::OnChannelConnected(int32 peer_pid) {
#if defined(OS_WIN)
  base::ProcessHandle plugins_renderer_handle = NULL;
  ::DuplicateHandle(::GetCurrentProcess(), filter_->peer_handle(),
                    GetChildProcessHandle(), &plugins_renderer_handle,
                    0, FALSE, DUPLICATE_SAME_ACCESS);
#elif defined(OS_POSIX)
  base::ProcessHandle plugins_renderer_handle = filter_->peer_handle();
#endif

  PpapiMsg_LoadPlugin* msg = new PpapiMsg_LoadPlugin(
      plugins_renderer_handle, plugin_path_, filter_->render_process_id());
  if (!Send(msg))  // Just send an empty handle on failure.
    ReplyToRenderer(NULL, IPC::ChannelHandle());
  // This function will result in OnChannelCreated getting called to finish.
}

void PpapiPluginProcessHost::OnChannelError() {
  if (reply_msg_.get())
    ReplyToRenderer(NULL, IPC::ChannelHandle());
}

void PpapiPluginProcessHost::OnPluginLoaded(
    const IPC::ChannelHandle& channel_handle) {
  base::ProcessHandle plugin_process = GetChildProcessHandle();
#if defined(OS_WIN)
  base::ProcessHandle renderers_plugin_handle = NULL;
  ::DuplicateHandle(::GetCurrentProcess(), plugin_process,
                    filter_->peer_handle(), &renderers_plugin_handle,
                    0, FALSE, DUPLICATE_SAME_ACCESS);
#elif defined(OS_POSIX)
  // Don't need to duplicate anything on POSIX since it's just a PID.
  base::ProcessHandle renderers_plugin_handle = plugin_process;
#endif
  ReplyToRenderer(renderers_plugin_handle, channel_handle);
}

void PpapiPluginProcessHost::ReplyToRenderer(
    base::ProcessHandle plugin_handle,
    const IPC::ChannelHandle& channel_handle) {
  DCHECK(reply_msg_.get());
  ViewHostMsg_OpenChannelToPepperPlugin::WriteReplyParams(reply_msg_.get(),
                                                          plugin_handle,
                                                          channel_handle);
  filter_->Send(reply_msg_.release());
}
