// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/utility_process_host.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "content/common/child_process_host_impl.h"
#include "content/common/utility_messages.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_switches.h"
#include "ipc/ipc_switches.h"
#include "ui/base/ui_base_switches.h"
#include "webkit/plugins/plugin_switches.h"

using content::BrowserThread;
using content::ChildProcessHost;

UtilityProcessHost::Client::Client() {
}

UtilityProcessHost::Client::~Client() {
}

void UtilityProcessHost::Client::OnProcessCrashed(int exit_code) {
}

bool UtilityProcessHost::Client::OnMessageReceived(
    const IPC::Message& message) {
  return false;
}

UtilityProcessHost::UtilityProcessHost(Client* client,
                                       BrowserThread::ID client_thread_id)
    : BrowserChildProcessHost(content::PROCESS_TYPE_UTILITY),
      client_(client),
      client_thread_id_(client_thread_id),
      is_batch_mode_(false),
      no_sandbox_(false),
#if defined(OS_LINUX)
      child_flags_(ChildProcessHost::CHILD_ALLOW_SELF),
      use_linux_zygote_(true),
#else
      child_flags_(ChildProcessHost::CHILD_NORMAL),
      use_linux_zygote_(false),
#endif
      started_(false) {
}

UtilityProcessHost::~UtilityProcessHost() {
  DCHECK(!is_batch_mode_);
}

bool UtilityProcessHost::Send(IPC::Message* message) {
  if (!StartProcess())
    return false;

  return BrowserChildProcessHost::Send(message);
}

bool UtilityProcessHost::StartBatchMode()  {
  CHECK(!is_batch_mode_);
  is_batch_mode_ = StartProcess();
  Send(new UtilityMsg_BatchMode_Started());
  return is_batch_mode_;
}

void UtilityProcessHost::EndBatchMode()  {
  CHECK(is_batch_mode_);
  is_batch_mode_ = false;
  Send(new UtilityMsg_BatchMode_Finished());
}

FilePath UtilityProcessHost::GetUtilityProcessCmd() {
  return ChildProcessHost::GetChildPath(child_flags_);
}

bool UtilityProcessHost::StartProcess() {
  if (started_)
    return true;
  started_ = true;

  if (is_batch_mode_)
    return true;
  // Name must be set or metrics_service will crash in any test which
  // launches a UtilityProcessHost.
  set_name(ASCIIToUTF16("utility process"));

  std::string channel_id = child_process_host()->CreateChannel();
  if (channel_id.empty())
    return false;

  FilePath exe_path = GetUtilityProcessCmd();
  if (exe_path.empty()) {
    NOTREACHED() << "Unable to get utility process binary name.";
    return false;
  }

  CommandLine* cmd_line = new CommandLine(exe_path);
  cmd_line->AppendSwitchASCII(switches::kProcessType,
                              switches::kUtilityProcess);
  cmd_line->AppendSwitchASCII(switches::kProcessChannelID, channel_id);
  std::string locale =
      content::GetContentClient()->browser()->GetApplicationLocale();
  cmd_line->AppendSwitchASCII(switches::kLang, locale);

  const CommandLine& browser_command_line = *CommandLine::ForCurrentProcess();
  if (browser_command_line.HasSwitch(switches::kChromeFrame))
    cmd_line->AppendSwitch(switches::kChromeFrame);
  if (no_sandbox_ || browser_command_line.HasSwitch(switches::kNoSandbox))
    cmd_line->AppendSwitch(switches::kNoSandbox);
  if (browser_command_line.HasSwitch(switches::kDebugPluginLoading))
    cmd_line->AppendSwitch(switches::kDebugPluginLoading);

#if defined(OS_POSIX)
  // TODO(port): Sandbox extension unpacking on Linux.
  bool has_cmd_prefix = browser_command_line.HasSwitch(
      switches::kUtilityCmdPrefix);
  if (has_cmd_prefix) {
    // launch the utility child process with some prefix (usually "xterm -e gdb
    // --args").
    cmd_line->PrependWrapper(browser_command_line.GetSwitchValueNative(
        switches::kUtilityCmdPrefix));
  }

  cmd_line->AppendSwitchPath(switches::kUtilityProcessAllowedDir, exposed_dir_);
#endif

  bool use_zygote = false;

#if defined(OS_LINUX)
  use_zygote = !no_sandbox_ && use_linux_zygote_;
#endif

  Launch(
#if defined(OS_WIN)
      exposed_dir_,
#elif defined(OS_POSIX)
      use_zygote,
      env_,
#endif
      cmd_line);

  return true;
}

bool UtilityProcessHost::OnMessageReceived(const IPC::Message& message) {
  BrowserThread::PostTask(
      client_thread_id_, FROM_HERE,
      base::IgnoreReturn<bool>(
          base::Bind(&Client::OnMessageReceived, client_.get(), message)));
  return true;
}

void UtilityProcessHost::OnProcessCrashed(int exit_code) {
  BrowserThread::PostTask(
      client_thread_id_, FROM_HERE,
      base::Bind(&Client::OnProcessCrashed, client_.get(), exit_code));
}
