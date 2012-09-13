// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/utility_process_host_impl.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "content/browser/browser_child_process_host_impl.h"
#include "content/common/child_process_host_impl.h"
#include "content/common/utility_messages.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/utility_process_host_client.h"
#include "content/public/common/content_switches.h"
#include "ipc/ipc_switches.h"
#include "ui/base/ui_base_switches.h"
#include "webkit/plugins/plugin_switches.h"

using content::BrowserThread;
using content::ChildProcessHost;
using content::UtilityProcessHostClient;

namespace content {

UtilityProcessHost* UtilityProcessHost::Create(
    UtilityProcessHostClient* client,
    BrowserThread::ID client_thread_id) {
  return new UtilityProcessHostImpl(client, client_thread_id);
}

}  // namespace content

UtilityProcessHostImpl::UtilityProcessHostImpl(
    UtilityProcessHostClient* client,
    BrowserThread::ID client_thread_id)
    : client_(client),
      client_thread_id_(client_thread_id),
      is_batch_mode_(false),
      no_sandbox_(false),
#if defined(OS_LINUX)
      child_flags_(ChildProcessHost::CHILD_ALLOW_SELF),
#else
      child_flags_(ChildProcessHost::CHILD_NORMAL),
#endif
      use_linux_zygote_(false),
      started_(false) {
  process_.reset(
      new BrowserChildProcessHostImpl(content::PROCESS_TYPE_UTILITY, this));
}

UtilityProcessHostImpl::~UtilityProcessHostImpl() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!is_batch_mode_);
}

bool UtilityProcessHostImpl::Send(IPC::Message* message) {
  if (!StartProcess())
    return false;

  return process_->Send(message);
}

bool UtilityProcessHostImpl::StartBatchMode()  {
  CHECK(!is_batch_mode_);
  is_batch_mode_ = StartProcess();
  Send(new UtilityMsg_BatchMode_Started());
  return is_batch_mode_;
}

void UtilityProcessHostImpl::EndBatchMode()  {
  CHECK(is_batch_mode_);
  is_batch_mode_ = false;
  Send(new UtilityMsg_BatchMode_Finished());
}

void UtilityProcessHostImpl::SetExposedDir(const FilePath& dir) {
  exposed_dir_ = dir;
}

void UtilityProcessHostImpl::DisableSandbox() {
  no_sandbox_ = true;
}

void UtilityProcessHostImpl::EnableZygote() {
  use_linux_zygote_ = true;
}

#if defined(OS_POSIX)

void UtilityProcessHostImpl::SetEnv(const base::EnvironmentVector& env) {
  env_ = env;
}

#endif  // OS_POSIX

bool UtilityProcessHostImpl::StartProcess() {
  if (started_)
    return true;
  started_ = true;

  if (is_batch_mode_)
    return true;
  // Name must be set or metrics_service will crash in any test which
  // launches a UtilityProcessHost.
  process_->SetName(ASCIIToUTF16("utility process"));

  std::string channel_id = process_->GetHost()->CreateChannel();
  if (channel_id.empty())
    return false;

  const CommandLine& browser_command_line = *CommandLine::ForCurrentProcess();
  int child_flags = child_flags_;

#if defined(OS_POSIX)
  bool has_cmd_prefix = browser_command_line.HasSwitch(
      switches::kUtilityCmdPrefix);

  // When running under gdb, forking /proc/self/exe ends up forking the gdb
  // executable instead of Chromium. It is almost safe to assume that no
  // updates will happen while a developer is running with
  // |switches::kUtilityCmdPrefix|. See ChildProcessHost::GetChildPath() for
  // a similar case with Valgrind.
  if (has_cmd_prefix)
    child_flags = ChildProcessHost::CHILD_NORMAL;
#endif

  FilePath exe_path = ChildProcessHost::GetChildPath(child_flags);
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

  if (browser_command_line.HasSwitch(switches::kChromeFrame))
    cmd_line->AppendSwitch(switches::kChromeFrame);
  if (no_sandbox_ || browser_command_line.HasSwitch(switches::kNoSandbox))
    cmd_line->AppendSwitch(switches::kNoSandbox);
  if (browser_command_line.HasSwitch(switches::kDebugPluginLoading))
    cmd_line->AppendSwitch(switches::kDebugPluginLoading);

#if defined(OS_POSIX)
  // TODO(port): Sandbox this on Linux.  Also, zygote this to work with
  // Linux updating.
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

  process_->Launch(
#if defined(OS_WIN)
      exposed_dir_,
#elif defined(OS_POSIX)
      use_zygote,
      env_,
#endif
      cmd_line);

  return true;
}

bool UtilityProcessHostImpl::OnMessageReceived(const IPC::Message& message) {
  BrowserThread::PostTask(
      client_thread_id_, FROM_HERE,
      base::Bind(base::IgnoreResult(
          &UtilityProcessHostClient::OnMessageReceived), client_.get(),
          message));
  return true;
}

void UtilityProcessHostImpl::OnProcessCrashed(int exit_code) {
  BrowserThread::PostTask(
      client_thread_id_, FROM_HERE,
      base::Bind(&UtilityProcessHostClient::OnProcessCrashed, client_.get(),
            exit_code));
}
