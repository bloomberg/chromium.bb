// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/broker/nacl_broker_listener.h"

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/path_service.h"
#include "components/nacl/common/nacl_cmd_line.h"
#include "components/nacl/common/nacl_debug_exception_handler_win.h"
#include "components/nacl/common/nacl_messages.h"
#include "components/nacl/common/nacl_switches.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/sandbox_init.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_switches.h"
#include "sandbox/win/src/sandbox_policy.h"

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
  channel_ = IPC::Channel::CreateClient(channel_name, this);
  CHECK(channel_->Connect());
  base::MessageLoop::current()->Run();
}

// NOTE: changes to this method need to be reviewed by the security team.
void NaClBrokerListener::PreSpawnTarget(sandbox::TargetPolicy* policy,
                                        bool* success) {
  // This code is duplicated in chrome_content_browser_client.cc.

  // Allow the server side of a pipe restricted to the "chrome.nacl."
  // namespace so that it cannot impersonate other system or other chrome
  // service pipes.
  sandbox::ResultCode result = policy->AddRule(
      sandbox::TargetPolicy::SUBSYS_NAMED_PIPES,
      sandbox::TargetPolicy::NAMEDPIPES_ALLOW_ANY,
      L"\\\\.\\pipe\\chrome.nacl.*");
  *success = (result == sandbox::SBOX_ALL_OK);
}

void NaClBrokerListener::OnChannelConnected(int32 peer_pid) {
  bool res = base::OpenPrivilegedProcessHandle(peer_pid, &browser_handle_);
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
  base::MessageLoop::current()->Quit();
}

void NaClBrokerListener::OnLaunchLoaderThroughBroker(
    const std::string& loader_channel_id) {
  base::ProcessHandle loader_process = 0;
  base::ProcessHandle loader_handle_in_browser = 0;

  // Create the path to the nacl broker/loader executable - it's the executable
  // this code is running in.
  base::FilePath exe_path;
  PathService::Get(base::FILE_EXE, &exe_path);
  if (!exe_path.empty()) {
    CommandLine* cmd_line = new CommandLine(exe_path);
    nacl::CopyNaClCommandLineArguments(cmd_line);

    cmd_line->AppendSwitchASCII(switches::kProcessType,
                                switches::kNaClLoaderProcess);

    cmd_line->AppendSwitchASCII(switches::kProcessChannelID,
                                loader_channel_id);

    loader_process = content::StartSandboxedProcess(this, cmd_line);
    if (loader_process) {
      // Note: PROCESS_DUP_HANDLE is necessary here, because:
      // 1) The current process is the broker, which is the loader's parent.
      // 2) The browser is not the loader's parent, and so only gets the
      //    access rights we confer here.
      // 3) The browser calls DuplicateHandle to set up communications with
      //    the loader.
      // 4) The target process handle to DuplicateHandle needs to have
      //    PROCESS_DUP_HANDLE access rights.
      DuplicateHandle(::GetCurrentProcess(), loader_process,
          browser_handle_, &loader_handle_in_browser,
          PROCESS_DUP_HANDLE | PROCESS_QUERY_INFORMATION | PROCESS_TERMINATE,
          FALSE, 0);
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
  base::MessageLoop::current()->Quit();
}
