// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/nacl/nacl_broker_listener.h"

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "chrome/common/nacl_cmd_line.h"
#include "chrome/common/nacl_debug_exception_handler_win.h"
#include "chrome/common/nacl_messages.h"
#include "content/common/sandbox_policy.h"
#include "content/public/common/content_switches.h"
#include "ipc/ipc_switches.h"

namespace {

void SendReply(IPC::Channel* channel, int32 pid, bool result) {
  channel->Send(new NaClProcessMsg_DebugExceptionHandlerLaunched(pid, result));
}

}  // namespace

NaClBrokerListener::NaClBrokerListener()
    : browser_handle_(base::kNullProcessHandle) {
}

NaClBrokerListener::~NaClBrokerListener() {
  base::CloseProcessHandle(browser_handle_);
}

void NaClBrokerListener::Listen() {
  std::string channel_name =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kProcessChannelID);
  channel_.reset(new IPC::Channel(
      channel_name, IPC::Channel::MODE_CLIENT, this));
  CHECK(channel_->Connect());
  MessageLoop::current()->Run();
}

void NaClBrokerListener::OnChannelConnected(int32 peer_pid) {
  bool res = base::OpenProcessHandle(peer_pid, &browser_handle_);
  CHECK(res);
}

bool NaClBrokerListener::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(NaClBrokerListener, msg)
    IPC_MESSAGE_HANDLER(NaClProcessMsg_LaunchLoaderThroughBroker,
                        OnLaunchLoaderThroughBroker)
    IPC_MESSAGE_HANDLER(NaClProcessMsg_LaunchDebugExceptionHandler,
                        OnLaunchDebugExceptionHandler)
    IPC_MESSAGE_HANDLER(NaClProcessMsg_StopBroker, OnStopBroker)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void NaClBrokerListener::OnChannelError() {
  // The browser died unexpectedly, quit to avoid a zombie process.
  MessageLoop::current()->Quit();
}

void NaClBrokerListener::OnLaunchLoaderThroughBroker(
    const std::string& loader_channel_id) {
  base::ProcessHandle loader_process = 0;
  base::ProcessHandle loader_handle_in_browser = 0;

  // Create the path to the nacl broker/loader executable - it's the executable
  // this code is running in.
  FilePath exe_path;
  PathService::Get(base::FILE_EXE, &exe_path);
  if (!exe_path.empty()) {
    CommandLine* cmd_line = new CommandLine(exe_path);
    nacl::CopyNaClCommandLineArguments(cmd_line);

    cmd_line->AppendSwitchASCII(switches::kProcessType,
                                switches::kNaClLoaderProcess);

    cmd_line->AppendSwitchASCII(switches::kProcessChannelID,
                                loader_channel_id);

    loader_process = sandbox::StartProcessWithAccess(cmd_line, FilePath());
    if (loader_process) {
      DuplicateHandle(::GetCurrentProcess(), loader_process,
          browser_handle_, &loader_handle_in_browser,
          PROCESS_DUP_HANDLE | PROCESS_QUERY_INFORMATION , FALSE, 0);
      base::CloseProcessHandle(loader_process);
    }
  }
  channel_->Send(new NaClProcessMsg_LoaderLaunched(loader_channel_id,
                                                   loader_handle_in_browser));
}

void NaClBrokerListener::OnLaunchDebugExceptionHandler(
    int32 pid, base::ProcessHandle process_handle,
    const std::string& startup_info) {
  NaClStartDebugExceptionHandlerThread(
      process_handle, startup_info,
      base::MessageLoopProxy::current(),
      base::Bind(SendReply, channel_.get(), pid));
}

void NaClBrokerListener::OnStopBroker() {
  MessageLoop::current()->Quit();
}
