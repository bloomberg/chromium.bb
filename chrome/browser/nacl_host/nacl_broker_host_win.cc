// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nacl_host/nacl_broker_host_win.h"

#include "base/command_line.h"
#include "base/path_service.h"
#include "ipc/ipc_switches.h"
#include "chrome/browser/nacl_host/nacl_broker_service_win.h"
#include "chrome/browser/nacl_host/nacl_process_host.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/nacl_cmd_line.h"
#include "chrome/common/nacl_messages.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"

namespace {
// NOTE: changes to this class need to be reviewed by the security team.
class NaClBrokerSandboxedProcessLauncherDelegate
    : public content::SandboxedProcessLauncherDelegate {
 public:
  NaClBrokerSandboxedProcessLauncherDelegate() {}
  virtual ~NaClBrokerSandboxedProcessLauncherDelegate() {}

  virtual void ShouldSandbox(bool* in_sandbox) OVERRIDE {
    *in_sandbox = false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NaClBrokerSandboxedProcessLauncherDelegate);
};
}  // namespace

NaClBrokerHost::NaClBrokerHost() : is_terminating_(false) {
  process_.reset(content::BrowserChildProcessHost::Create(
      content::PROCESS_TYPE_NACL_BROKER, this));
}

NaClBrokerHost::~NaClBrokerHost() {
}

bool NaClBrokerHost::Init() {
  // Create the channel that will be used for communicating with the broker.
  std::string channel_id = process_->GetHost()->CreateChannel();
  if (channel_id.empty())
    return false;

  // Create the path to the nacl broker/loader executable.
  base::FilePath module_path;
  if (!PathService::Get(base::FILE_MODULE, &module_path))
    return false;

  base::FilePath nacl_path = module_path.DirName().Append(chrome::kNaClAppName);
  CommandLine* cmd_line = new CommandLine(nacl_path);
  nacl::CopyNaClCommandLineArguments(cmd_line);

  cmd_line->AppendSwitchASCII(switches::kProcessType,
                              switches::kNaClBrokerProcess);
  cmd_line->AppendSwitchASCII(switches::kProcessChannelID, channel_id);
  if (logging::DialogsAreSuppressed())
    cmd_line->AppendSwitch(switches::kNoErrorDialogs);

  process_->Launch(new NaClBrokerSandboxedProcessLauncherDelegate, cmd_line);
  return true;
}

bool NaClBrokerHost::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(NaClBrokerHost, msg)
    IPC_MESSAGE_HANDLER(NaClProcessMsg_LoaderLaunched, OnLoaderLaunched)
    IPC_MESSAGE_HANDLER(NaClProcessMsg_DebugExceptionHandlerLaunched,
                        OnDebugExceptionHandlerLaunched)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool NaClBrokerHost::LaunchLoader(const std::string& loader_channel_id) {
  return process_->Send(
      new NaClProcessMsg_LaunchLoaderThroughBroker(loader_channel_id));
}

void NaClBrokerHost::OnLoaderLaunched(const std::string& loader_channel_id,
                                      base::ProcessHandle handle) {
  NaClBrokerService::GetInstance()->OnLoaderLaunched(loader_channel_id, handle);
}

bool NaClBrokerHost::LaunchDebugExceptionHandler(
    int32 pid, base::ProcessHandle process_handle,
    const std::string& startup_info) {
  base::ProcessHandle broker_process = process_->GetData().handle;
  base::ProcessHandle handle_in_broker_process;
  if (!DuplicateHandle(::GetCurrentProcess(), process_handle,
                       broker_process, &handle_in_broker_process,
                       0, /* bInheritHandle= */ FALSE, DUPLICATE_SAME_ACCESS))
    return false;
  return process_->Send(new NaClProcessMsg_LaunchDebugExceptionHandler(
      pid, handle_in_broker_process, startup_info));
}

void NaClBrokerHost::OnDebugExceptionHandlerLaunched(int32 pid, bool success) {
  NaClBrokerService::GetInstance()->OnDebugExceptionHandlerLaunched(pid,
                                                                    success);
}

void NaClBrokerHost::StopBroker() {
  is_terminating_ = true;
  process_->Send(new NaClProcessMsg_StopBroker());
}
