// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/utility_process_host.h"

#include "app/app_switches.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/indexed_db_key.h"
#include "chrome/common/serialized_script_value.h"
#include "chrome/common/utility_messages.h"
#include "ipc/ipc_switches.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/ui_base_switches.h"

UtilityProcessHost::UtilityProcessHost(ResourceDispatcherHost* rdh,
                                       Client* client,
                                       BrowserThread::ID client_thread_id)
    : BrowserChildProcessHost(UTILITY_PROCESS, rdh),
      client_(client),
      client_thread_id_(client_thread_id),
      is_batch_mode_(false) {
}

UtilityProcessHost::~UtilityProcessHost() {
  DCHECK(!is_batch_mode_);
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

bool UtilityProcessHost::StartImageDecoding(
    const std::vector<unsigned char>& encoded_data) {
  if (!StartProcess(FilePath()))
    return false;

  Send(new UtilityMsg_DecodeImage(encoded_data));
  return true;
}

bool UtilityProcessHost::StartIDBKeysFromValuesAndKeyPath(
    int id, const std::vector<SerializedScriptValue>& serialized_values,
    const string16& key_path)  {
  if (!StartProcess(FilePath()))
    return false;

  Send(new UtilityMsg_IDBKeysFromValuesAndKeyPath(
      id, serialized_values, key_path));
  return true;
}

bool UtilityProcessHost::StartInjectIDBKey(
    const IndexedDBKey& key, const SerializedScriptValue& value,
    const string16& key_path) {
  if (!StartProcess(FilePath()))
    return false;

  Send(new UtilityMsg_InjectIDBKey(key, value, key_path));
  return true;
}

bool UtilityProcessHost::StartBatchMode()  {
  CHECK(!is_batch_mode_);
  is_batch_mode_ = StartProcess(FilePath());
  Send(new UtilityMsg_BatchMode_Started());
  return is_batch_mode_;
}

void UtilityProcessHost::EndBatchMode()  {
  CHECK(is_batch_mode_);
  is_batch_mode_ = false;
  Send(new UtilityMsg_BatchMode_Finished());
}

FilePath UtilityProcessHost::GetUtilityProcessCmd() {
  return GetChildPath(true);
}

bool UtilityProcessHost::StartProcess(const FilePath& exposed_dir) {
  if (is_batch_mode_)
    return true;
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
  cmd_line->AppendSwitchASCII(switches::kProcessType,
                              switches::kUtilityProcess);
  cmd_line->AppendSwitchASCII(switches::kProcessChannelID, channel_id());
  std::string locale = g_browser_process->GetApplicationLocale();
  cmd_line->AppendSwitchASCII(switches::kLang, locale);

  SetCrashReporterCommandLine(cmd_line);

  const CommandLine& browser_command_line = *CommandLine::ForCurrentProcess();
  if (browser_command_line.HasSwitch(switches::kChromeFrame))
    cmd_line->AppendSwitch(switches::kChromeFrame);

  if (browser_command_line.HasSwitch(
      switches::kEnableExperimentalExtensionApis)) {
    cmd_line->AppendSwitch(switches::kEnableExperimentalExtensionApis);
  }

#if defined(OS_POSIX)
  // TODO(port): Sandbox this on Linux.  Also, zygote this to work with
  // Linux updating.
  bool has_cmd_prefix = browser_command_line.HasSwitch(
      switches::kUtilityCmdPrefix);
  if (has_cmd_prefix) {
    // launch the utility child process with some prefix (usually "xterm -e gdb
    // --args").
    cmd_line->PrependWrapper(browser_command_line.GetSwitchValueNative(
        switches::kUtilityCmdPrefix));
  }

  cmd_line->AppendSwitchPath(switches::kUtilityProcessAllowedDir, exposed_dir);
#endif

  Launch(
#if defined(OS_WIN)
      exposed_dir,
#elif defined(OS_POSIX)
      false,
      base::environment_vector(),
#endif
      cmd_line);

  return true;
}

bool UtilityProcessHost::OnMessageReceived(const IPC::Message& message) {
  BrowserThread::PostTask(
      client_thread_id_, FROM_HERE,
      NewRunnableMethod(client_.get(), &Client::OnMessageReceived, message));
  return true;
}

void UtilityProcessHost::OnProcessCrashed(int exit_code) {
  BrowserThread::PostTask(
      client_thread_id_, FROM_HERE,
      NewRunnableMethod(client_.get(), &Client::OnProcessCrashed, exit_code));
}

bool UtilityProcessHost::CanShutdown() {
  return true;
}

bool UtilityProcessHost::Client::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
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
    IPC_MESSAGE_HANDLER(UtilityHostMsg_DecodeImage_Succeeded,
                        Client::OnDecodeImageSucceeded)
    IPC_MESSAGE_HANDLER(UtilityHostMsg_DecodeImage_Failed,
                        Client::OnDecodeImageFailed)
    IPC_MESSAGE_HANDLER(UtilityHostMsg_IDBKeysFromValuesAndKeyPath_Succeeded,
                        Client::OnIDBKeysFromValuesAndKeyPathSucceeded)
    IPC_MESSAGE_HANDLER(UtilityHostMsg_IDBKeysFromValuesAndKeyPath_Failed,
                        Client::OnIDBKeysFromValuesAndKeyPathFailed)
    IPC_MESSAGE_HANDLER(UtilityHostMsg_InjectIDBKey_Finished,
                        Client::OnInjectIDBKeyFinished)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}
