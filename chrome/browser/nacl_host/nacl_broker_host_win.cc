// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nacl_host/nacl_broker_host_win.h"

#include "base/command_line.h"
#include "base/path_service.h"
#include "ipc/ipc_switches.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/nacl_host/nacl_broker_service_win.h"
#include "chrome/browser/nacl_host/nacl_process_host.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/nacl_cmd_line.h"
#include "chrome/common/nacl_messages.h"

NaClBrokerHost::NaClBrokerHost(
    ResourceDispatcherHost* resource_dispatcher_host)
    : BrowserChildProcessHost(NACL_BROKER_PROCESS, resource_dispatcher_host),
      stopping_(false) {
}

NaClBrokerHost::~NaClBrokerHost() {
}

URLRequestContext* NaClBrokerHost::GetRequestContext(
    uint32 request_id,
    const ViewHostMsg_Resource_Request& request_data) {
  return NULL;
}

bool NaClBrokerHost::Init() {
  // Create the channel that will be used for communicating with the broker.
  if (!CreateChannel())
    return false;

  // Create the path to the nacl broker/loader executable.
  FilePath module_path;
  if (!PathService::Get(base::FILE_MODULE, &module_path))
    return false;

  FilePath nacl_path = module_path.DirName().Append(chrome::kNaClAppName);
  CommandLine* cmd_line = new CommandLine(nacl_path);
  nacl::CopyNaClCommandLineArguments(cmd_line);

  cmd_line->AppendSwitchWithValue(switches::kProcessType,
      switches::kNaClBrokerProcess);

  cmd_line->AppendSwitchWithValue(switches::kProcessChannelID,
      ASCIIToWide(channel_id()));

  BrowserChildProcessHost::Launch(FilePath(), cmd_line);
  return true;
}

void NaClBrokerHost::OnMessageReceived(const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(NaClBrokerHost, msg)
    IPC_MESSAGE_HANDLER(NaClProcessMsg_LoaderLaunched, OnLoaderLaunched)
  IPC_END_MESSAGE_MAP()
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
