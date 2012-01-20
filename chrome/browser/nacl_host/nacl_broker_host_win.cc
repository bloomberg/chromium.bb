// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
#include "content/public/common/child_process_host.h"

NaClBrokerHost::NaClBrokerHost()
    : BrowserChildProcessHost(content::PROCESS_TYPE_NACL_BROKER),
      stopping_(false) {
}

NaClBrokerHost::~NaClBrokerHost() {
}

bool NaClBrokerHost::Init() {
  // Create the channel that will be used for communicating with the broker.
  std::string channel_id = child_process_host()->CreateChannel();
  if (channel_id.empty())
    return false;

  // Create the path to the nacl broker/loader executable.
  FilePath module_path;
  if (!PathService::Get(base::FILE_MODULE, &module_path))
    return false;

  FilePath nacl_path = module_path.DirName().Append(chrome::kNaClAppName);
  CommandLine* cmd_line = new CommandLine(nacl_path);
  nacl::CopyNaClCommandLineArguments(cmd_line);

  cmd_line->AppendSwitchASCII(switches::kProcessType,
                              switches::kNaClBrokerProcess);
  cmd_line->AppendSwitchASCII(switches::kProcessChannelID, channel_id);
  if (logging::DialogsAreSuppressed())
    cmd_line->AppendSwitch(switches::kNoErrorDialogs);

  BrowserChildProcessHost::Launch(FilePath(), cmd_line);
  return true;
}

bool NaClBrokerHost::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(NaClBrokerHost, msg)
    IPC_MESSAGE_HANDLER(NaClProcessMsg_LoaderLaunched, OnLoaderLaunched)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool NaClBrokerHost::LaunchLoader(
    const std::wstring& loader_channel_id) {
  return Send(new NaClProcessMsg_LaunchLoaderThroughBroker(loader_channel_id));
}

void NaClBrokerHost::OnLoaderLaunched(const std::wstring& loader_channel_id,
                                      base::ProcessHandle handle) {
  NaClBrokerService::GetInstance()->OnLoaderLaunched(loader_channel_id, handle);
}

void NaClBrokerHost::StopBroker() {
  stopping_ = true;
  Send(new NaClProcessMsg_StopBroker());
}
