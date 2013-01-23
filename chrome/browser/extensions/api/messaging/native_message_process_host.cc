// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/messaging/native_message_process_host.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/pickle.h"
#include "base/platform_file.h"
#include "base/process_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/messaging/native_process_launcher.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/extensions/features/feature.h"
#include "content/public/common/result_codes.h"

namespace {

// Message header contains 4-byte integer size of the message.
const size_t kMessageHeaderSize = 4;

const int kExitTimeoutMS = 5000;
const uint32 kMaxMessageDataLength = 10 * 1024 * 1024;
const char kNativeHostsDirectoryName[] = "native_hosts";

}  // namespace

namespace extensions {

NativeMessageProcessHost::NativeMessageProcessHost(
    base::WeakPtr<Client> weak_client_ui,
    const std::string& native_host_name,
    int destination_port,
    scoped_ptr<NativeProcessLauncher> launcher)
    : weak_client_ui_(weak_client_ui),
      native_host_name_(native_host_name),
      destination_port_(destination_port),
      native_process_handle_(base::kNullProcessHandle),
      read_file_(base::kInvalidPlatformFileValue),
      write_file_(base::kInvalidPlatformFileValue) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // It's safe to use base::Unretained() here because NativeMessagePort always
  // deletes us on the FILE thread.
  content::BrowserThread::PostTask(content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&NativeMessageProcessHost::LaunchHostProcess,
                 base::Unretained(this), base::Passed(&launcher)));
}

NativeMessageProcessHost::~NativeMessageProcessHost() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));

  // Give the process some time to shutdown, then try and kill it.
  content::BrowserThread::PostDelayedTask(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(base::IgnoreResult(&base::KillProcess),
                 native_process_handle_,
                 content::RESULT_CODE_NORMAL_EXIT,
                 false /* don't wait for exit */),
      base::TimeDelta::FromMilliseconds(kExitTimeoutMS));
}

// static
scoped_ptr<NativeMessageProcessHost> NativeMessageProcessHost::Create(
    base::WeakPtr<Client> weak_client_ui,
    const std::string& native_host_name,
    int destination_port) {
  scoped_ptr<NativeProcessLauncher> launcher(new NativeProcessLauncher());
  return CreateWithLauncher(weak_client_ui, native_host_name, destination_port,
                            launcher.Pass());
}

// static
scoped_ptr<NativeMessageProcessHost>
NativeMessageProcessHost::CreateWithLauncher(
    base::WeakPtr<Client> weak_client_ui,
    const std::string& native_host_name,
    int destination_port,
    scoped_ptr<NativeProcessLauncher> launcher) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  scoped_ptr<NativeMessageProcessHost> process;
  if (Feature::GetCurrentChannel() > chrome::VersionInfo::CHANNEL_DEV ||
      !CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableNativeMessaging)) {
    return process.Pass();
  }

  process.reset(new NativeMessageProcessHost(
      weak_client_ui, native_host_name, destination_port, launcher.Pass()));

  return process.Pass();
}

void NativeMessageProcessHost::LaunchHostProcess(
    scoped_ptr<NativeProcessLauncher> launcher) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));

  FilePath native_host_program;
  FilePath native_host_registry;
  CHECK(PathService::Get(chrome::DIR_USER_DATA, &native_host_registry));
  native_host_registry =
      native_host_registry.AppendASCII(kNativeHostsDirectoryName);
  native_host_program = native_host_registry.AppendASCII(native_host_name_);

  // Make sure that the client is not trying to invoke something outside of the
  // proper directory. Eg. '../../dangerous_something.exe'.
  if (!file_util::ContainsPath(native_host_registry, native_host_program)) {
    LOG(ERROR) << "Could not find native host: " << native_host_name_;
    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
        base::Bind(&Client::CloseChannel, weak_client_ui_,
                   destination_port_, true));
    return;
  }

  if (!launcher->LaunchNativeProcess(native_host_program,
                                     &native_process_handle_,
                                     &read_file_,
                                     &write_file_)) {
    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
        base::Bind(&Client::CloseChannel, weak_client_ui_,
                   destination_port_, true));
    return;
  }

#if defined(OS_POSIX)
  scoped_read_file_.reset(&read_file_);
  scoped_write_file_.reset(&write_file_);
#endif  // defined(OS_POSIX)

  InitIO();
}

void NativeMessageProcessHost::Send(const std::string& json) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));

  if (native_process_handle_ == base::kNullProcessHandle) {
    // Don't need to do anything if LaunchHostProcess() didn't succeed.
    return;
  }

  // Make sure that the process has not died.
  if (base::GetTerminationStatus(native_process_handle_, NULL) !=
      base::TERMINATION_STATUS_STILL_RUNNING) {
    // Notify the message service that the channel should close.
    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
        base::Bind(&Client::CloseChannel, weak_client_ui_,
                   destination_port_, true));
  }

  WriteMessage(json);
}

bool NativeMessageProcessHost::WriteMessage(const std::string& message) {
  Pickle pickle;

  // Pickles write the length of a string before it as a uint32.
  pickle.WriteString(message);

  // Make sure that the pickle doesn't do any unexpected padding.
  CHECK_EQ(kMessageHeaderSize + message.length(), pickle.payload_size());

  if (!WriteData(write_file_, pickle.payload(), pickle.payload_size())) {
    LOG(ERROR) << "Error writing message to the native client.";
    return false;
  }

  return true;
}

bool NativeMessageProcessHost::ReadMessage(std::string* message) {
  // Read the length (uint32).
  char message_meta_data[kMessageHeaderSize];
  if (!ReadData(read_file_, message_meta_data, sizeof(message_meta_data))) {
    LOG(ERROR) << "Error reading the message length.";
    return false;
  }

  Pickle pickle;
  pickle.WriteBytes(message_meta_data, sizeof(message_meta_data));
  PickleIterator pickle_it(pickle);
  uint32 data_length;
  if (!pickle_it.ReadUInt32(&data_length)) {
    LOG(ERROR) << "Error getting the message  length from the pickle.";
    return false;
  }

  if (data_length > kMaxMessageDataLength) {
    LOG(ERROR) << data_length << " is too large for the length of a message. "
               << "Max message size is " << kMaxMessageDataLength;
    return false;
  }

  message->resize(data_length, '\0');
  if (!ReadData(read_file_, &(*message)[0], data_length)) {
    LOG(ERROR) << "Error reading the json data.";
    return false;
  }

  return true;
}

}  // namespace extensions
