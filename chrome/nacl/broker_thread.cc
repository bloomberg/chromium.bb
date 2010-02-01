// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/nacl/broker_thread.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "chrome/common/sandbox_policy.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/nacl_cmd_line.h"
#include "chrome/common/nacl_messages.h"
#include "ipc/ipc_switches.h"

NaClBrokerThread::NaClBrokerThread()
    : browser_handle_(0),
      broker_services_(NULL) {
}

NaClBrokerThread::~NaClBrokerThread() {
  base::CloseProcessHandle(browser_handle_);
}

NaClBrokerThread* NaClBrokerThread::current() {
  return static_cast<NaClBrokerThread*>(ChildThread::current());
}

void NaClBrokerThread::OnControlMessageReceived(const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(NaClBrokerThread, msg)
    IPC_MESSAGE_HANDLER(NaClProcessMsg_LaunchLoaderThroughBroker,
                        OnLaunchLoaderThroughBroker)
  IPC_END_MESSAGE_MAP()
}

void NaClBrokerThread::OnLaunchLoaderThroughBroker(
    const std::wstring& loader_channel_id) {
  base::ProcessHandle loader_process = 0;
  base::ProcessHandle loader_handle_in_browser = 0;

  // Create the path to the nacl broker/loader executable - it's the executable
  // this code is running in.
  FilePath exe_path;
  PathService::Get(base::FILE_EXE, &exe_path);
  if (!exe_path.empty()) {
    CommandLine* cmd_line = new CommandLine(exe_path);
    nacl::CopyNaClCommandLineArguments(cmd_line);

    cmd_line->AppendSwitchWithValue(switches::kProcessType,
        switches::kNaClLoaderProcess);

    cmd_line->AppendSwitchWithValue(switches::kProcessChannelID,
        loader_channel_id);

    loader_process = sandbox::StartProcessWithAccess(cmd_line, FilePath());
    if (loader_process) {
      DuplicateHandle(::GetCurrentProcess(), loader_process,
        browser_handle_, &loader_handle_in_browser,
        PROCESS_DUP_HANDLE, FALSE, 0);
    }
  }
  Send(new NaClProcessMsg_LoaderLaunched(loader_channel_id,
                                         loader_handle_in_browser));
}

void NaClBrokerThread::OnChannelConnected(int32 peer_pid) {
  bool res = base::OpenProcessHandle(peer_pid, &browser_handle_);
  DCHECK(res);
  Send(new NaClProcessMsg_BrokerReady());
}

