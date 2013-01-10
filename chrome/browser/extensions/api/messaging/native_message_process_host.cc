// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/messaging/native_message_process_host.h"

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/pickle.h"
#include "base/process_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/messaging/native_process_launcher.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/extensions/features/feature.h"
#include "content/public/common/result_codes.h"

namespace {

const int kExitTimeoutMS = 5000;
const uint32 kMaxMessageDataLength = 10 * 1024 * 1024;
const char kNativeHostsDirectoryName[] = "native_hosts";

}  // namespace

namespace extensions {

NativeMessageProcessHost::NativeMessageProcessHost(
    base::WeakPtr<Client> weak_client_ui,
    int destination_port,
    base::ProcessHandle native_process_handle,
    FileHandle read_file,
    FileHandle write_file,
    bool is_send_message)
    : weak_client_ui_(weak_client_ui),
      destination_port_(destination_port),
      native_process_handle_(native_process_handle),
      read_file_(read_file),
      write_file_(write_file),
      scoped_read_file_(&read_file_),
      scoped_write_file_(&write_file_),
      is_send_message_(is_send_message) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  InitIO();
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
void NativeMessageProcessHost::Create(base::WeakPtr<Client> weak_client_ui,
                                      const std::string& native_app_name,
                                      const std::string& connection_message,
                                      int destination_port,
                                      MessageType type,
                                      CreateCallback callback) {
  NativeProcessLauncher launcher;
  CreateWithLauncher(weak_client_ui, native_app_name, connection_message,
                     destination_port, type, callback, launcher);
}

// static
void NativeMessageProcessHost::CreateWithLauncher(
    base::WeakPtr<Client> weak_client_ui,
    const std::string& native_app_name,
    const std::string& connection_message,
    int destination_port,
    MessageType type,
    CreateCallback callback,
    const NativeProcessLauncher& launcher) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  DCHECK(type == TYPE_SEND_MESSAGE_REQUEST || type == TYPE_CONNECT);

  ScopedHost process;
  if (Feature::GetCurrentChannel() > chrome::VersionInfo::CHANNEL_DEV ||
      !CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableNativeMessaging)) {
    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                     base::Bind(callback,
                                                base::Passed(&process)));
    return;
  }

  FilePath native_host_program;
  FilePath native_host_registry;
  CHECK(PathService::Get(chrome::DIR_USER_DATA, &native_host_registry));
  native_host_registry =
      native_host_registry.AppendASCII(kNativeHostsDirectoryName);
  native_host_program = native_host_registry.AppendASCII(native_app_name);

  // Make sure that the client is not trying to invoke something outside of the
  // proper directory. Eg. '../../dangerous_something.exe'.
  if (!file_util::ContainsPath(native_host_registry, native_host_program)) {
    LOG(ERROR) << "Could not find native host: " << native_app_name;
    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                     base::Bind(callback,
                                                base::Passed(&process)));
    return;
  }

  FileHandle read_handle;
  FileHandle write_handle;
  base::ProcessHandle native_process_handle;

  if (!launcher.LaunchNativeProcess(native_host_program,
                                    &native_process_handle,
                                    &read_handle,
                                    &write_handle)) {
    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                     base::Bind(callback,
                                                base::Passed(&process)));
    return;
  }

  process.reset(new NativeMessageProcessHost(
      weak_client_ui, destination_port, native_process_handle, read_handle,
      write_handle, type == TYPE_SEND_MESSAGE_REQUEST));

  process->SendImpl(type, connection_message);

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(callback, base::Passed(&process)));
}

void NativeMessageProcessHost::SendImpl(MessageType type,
                                        const std::string& json) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));

  // Make sure that the process has not died.
  if (base::GetTerminationStatus(native_process_handle_, NULL) !=
      base::TERMINATION_STATUS_STILL_RUNNING) {
    // Notify the message service that the channel should close.
    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
        base::Bind(&Client::CloseChannel, weak_client_ui_,
                   destination_port_, true));
  }

  WriteMessage(type, json);
}

bool NativeMessageProcessHost::WriteMessage(MessageType type,
                                            const std::string& message) {
  Pickle pickle;

  // Pickles will always pad bytes to 32-bit alignment, so just use a unit32.
  pickle.WriteUInt32(type);

  // Pickles write the length of a string before it as a uint32.
  pickle.WriteString(message);

  // Make sure that the pickle doesn't do any unexpected padding.
  CHECK(8 + message.length() == pickle.payload_size());

  if (!WriteData(write_file_, const_cast<const Pickle*>(&pickle)->payload(),
                 pickle.payload_size())) {
    LOG(ERROR) << "Error writing message to the native client.";
    return false;
  }

  return true;
}

bool NativeMessageProcessHost::ReadMessage(MessageType* type,
                                           std::string* message) {
  // Read the type (uint32) and length (uint32).
  char message_meta_data[8];
  if (!ReadData(read_file_, message_meta_data, 8)) {
    LOG(ERROR) << "Error reading the message type and length.";
    return false;
  }

  Pickle pickle;
  pickle.WriteBytes(message_meta_data, 8);
  PickleIterator pickle_it(pickle);
  uint32 uint_type;
  uint32 data_length;
  if (!pickle_it.ReadUInt32(&uint_type) ||
      !pickle_it.ReadUInt32(&data_length)) {
    LOG(ERROR) << "Error getting the message type and length from the pickle.";
    return false;
  }

  if (uint_type >= NUM_MESSAGE_TYPES) {
    LOG(ERROR) << type << " is not a valid message type.";
    return false;
  }

  if ((is_send_message_ && (uint_type != TYPE_SEND_MESSAGE_RESPONSE)) ||
      (!is_send_message_ && (uint_type != TYPE_CONNECT_MESSAGE))) {
    LOG(ERROR) << "Recieved a message of type " << uint_type << ". "
               << "Expecting a message of type "
               << (is_send_message_ ? TYPE_SEND_MESSAGE_RESPONSE :
                                      TYPE_CONNECT_MESSAGE);
    return false;
  }
  *type = static_cast<MessageType>(uint_type);

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
