// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/utility_process_host.h"

#include "app/app_switches.h"
#include "app/l10n_util.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/render_messages.h"
#include "ipc/ipc_switches.h"

UtilityProcessHost::UtilityProcessHost(ResourceDispatcherHost* rdh,
                                       Client* client,
                                       ChromeThread::ID client_thread_id)
    : ChildProcessHost(UTILITY_PROCESS, rdh),
      client_(client),
      client_thread_id_(client_thread_id) {
}

UtilityProcessHost::~UtilityProcessHost() {
}

bool UtilityProcessHost::StartExtensionUnpacker(const FilePath& extension) {
  // Grant the subprocess access to the entire subdir the extension file is
  // in, so that it can unpack to that dir.
  if (!StartProcess(extension.DirName()))
    return false;

  Send(new UtilityMsg_UnpackExtension(extension));
  return true;
}

bool UtilityProcessHost::StartWebResourceUnpacker(const std::string& data) {
  if (!StartProcess(FilePath()))
    return false;

  Send(new UtilityMsg_UnpackWebResource(data));
  return true;
}

bool UtilityProcessHost::StartUpdateManifestParse(const std::string& xml) {
  if (!StartProcess(FilePath()))
    return false;

  Send(new UtilityMsg_ParseUpdateManifest(xml));
  return true;
}

FilePath UtilityProcessHost::GetUtilityProcessCmd() {
  return GetChildPath();
}

bool UtilityProcessHost::StartProcess(const FilePath& exposed_dir) {
#if defined(OS_POSIX)
  // TODO(port): We should not reach here on Linux (crbug.com/22703).
  // (crbug.com/23837) covers enabling this on Linux/OS X.
  NOTREACHED();
  return false;
#endif

  // Name must be set or metrics_service will crash in any test which
  // launches a UtilityProcessHost.
  set_name(L"utility process");

  if (!CreateChannel())
    return false;

  FilePath exe_path = GetUtilityProcessCmd();
  if (exe_path.empty()) {
    NOTREACHED() << "Unable to get utility process binary name.";
    return false;
  }

  CommandLine* cmd_line = new CommandLine(exe_path);
  cmd_line->AppendSwitchWithValue(switches::kProcessType,
                                  switches::kUtilityProcess);
  cmd_line->AppendSwitchWithValue(switches::kProcessChannelID,
                                  ASCIIToWide(channel_id()));
  // Pass on the browser locale.
  std::string locale = l10n_util::GetApplicationLocale(L"");
  cmd_line->AppendSwitchWithValue(switches::kLang, ASCIIToWide(locale));

  SetCrashReporterCommandLine(cmd_line);

  const CommandLine& browser_command_line = *CommandLine::ForCurrentProcess();
  if (browser_command_line.HasSwitch(switches::kChromeFrame))
    cmd_line->AppendSwitch(switches::kChromeFrame);

#if defined(OS_POSIX)
  // TODO(port): Sandbox this on Linux.  Also, zygote this to work with
  // Linux updating.
  bool has_cmd_prefix = browser_command_line.HasSwitch(
      switches::kUtilityCmdPrefix);
  if (has_cmd_prefix) {
    // launch the utility child process with some prefix (usually "xterm -e gdb
    // --args").
    cmd_line->PrependWrapper(browser_command_line.GetSwitchValue(
        switches::kUtilityCmdPrefix));
  }

  cmd_line->AppendSwitchWithValue(switches::kUtilityProcessAllowedDir,
                                  exposed_dir.value().c_str());
#endif

  Launch(
#if defined(OS_WIN)
      exposed_dir,
#elif defined(OS_POSIX)
      base::environment_vector(),
#endif
      cmd_line);

  return true;
}

void UtilityProcessHost::OnMessageReceived(const IPC::Message& message) {
  ChromeThread::PostTask(
      client_thread_id_, FROM_HERE,
      NewRunnableMethod(client_.get(), &Client::OnMessageReceived, message));
}

void UtilityProcessHost::OnProcessCrashed() {
  ChromeThread::PostTask(
      client_thread_id_, FROM_HERE,
      NewRunnableMethod(client_.get(), &Client::OnProcessCrashed));
}

void UtilityProcessHost::Client::OnMessageReceived(
    const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(UtilityProcessHost, message)
    IPC_MESSAGE_HANDLER(UtilityHostMsg_UnpackExtension_Succeeded,
                        Client::OnUnpackExtensionSucceeded)
    IPC_MESSAGE_HANDLER(UtilityHostMsg_UnpackExtension_Failed,
                        Client::OnUnpackExtensionFailed)
    IPC_MESSAGE_HANDLER(UtilityHostMsg_UnpackWebResource_Succeeded,
                        Client::OnUnpackWebResourceSucceeded)
    IPC_MESSAGE_HANDLER(UtilityHostMsg_UnpackWebResource_Failed,
                        Client::OnUnpackWebResourceFailed)
    IPC_MESSAGE_HANDLER(UtilityHostMsg_ParseUpdateManifest_Succeeded,
                        Client::OnParseUpdateManifestSucceeded)
    IPC_MESSAGE_HANDLER(UtilityHostMsg_ParseUpdateManifest_Failed,
                        Client::OnParseUpdateManifestFailed)
  IPC_END_MESSAGE_MAP_EX()
}
