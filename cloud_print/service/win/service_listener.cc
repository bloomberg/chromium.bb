// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/service/win/service_listener.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/json/json_writer.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "chrome/installer/launcher_support/chrome_launcher_support.h"
#include "cloud_print/service/win/service_utils.h"
#include "cloud_print/service/win/setup_listener.h"
#include "ipc/ipc_channel.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/win_helper.h"

namespace {

std::string GetEnvironment(const base::FilePath& user_data_dir) {
  scoped_refptr<printing::PrintBackend> backend(
      printing::PrintBackend::CreateInstance(NULL));
  printing::PrinterList printer_list;
  backend->EnumeratePrinters(&printer_list);
  scoped_ptr<base::ListValue> printers(new base::ListValue());
  for (size_t i = 0; i < printer_list.size(); ++i) {
    printers->AppendString(printer_list[i].printer_name);
  }

  base::DictionaryValue environment;
  environment.Set(SetupListener::kPrintersJsonValueName, printers.release());
  environment.SetBoolean(SetupListener::kXpsAvailibleJsonValueName,
                         printing::XPSModule::Init());
  environment.SetString(SetupListener::kUserNameJsonValueName,
                        GetCurrentUserName());
  environment.SetString(SetupListener::kChromePathJsonValueName,
                        chrome_launcher_support::GetAnyChromePath().value());
  if (file_util::DirectoryExists(user_data_dir)) {
    base::FilePath temp_file;
    if (file_util::CreateTemporaryFileInDir(user_data_dir, &temp_file)) {
      DCHECK(file_util::PathExists(temp_file));
      environment.SetString(SetupListener::kUserDataDirJsonValueName,
                            user_data_dir.value());
      file_util::Delete(temp_file, false);
    }
  }

  std::string result;
  base::JSONWriter::Write(&environment, &result);
  return result;
}

}  // namespace

ServiceListener::ServiceListener(const base::FilePath& user_data_dir)
    : ipc_thread_(new base::Thread("ipc_thread")),
      user_data_dir_(user_data_dir) {
  ipc_thread_->StartWithOptions(base::Thread::Options(MessageLoop::TYPE_IO, 0));
  ipc_thread_->message_loop()->PostTask(FROM_HERE,
                                        base::Bind(&ServiceListener::Connect,
                                                   base::Unretained(this)));
}

ServiceListener::~ServiceListener() {
  ipc_thread_->message_loop()->PostTask(FROM_HERE,
                                        base::Bind(&ServiceListener::Disconnect,
                                                   base::Unretained(this)));
  ipc_thread_->Stop();
}

bool ServiceListener::OnMessageReceived(const IPC::Message& msg) {
  return true;
}

void ServiceListener::OnChannelConnected(int32 peer_pid) {
  IPC::Message* message = new IPC::Message(0, 0, IPC::Message::PRIORITY_NORMAL);
  message->WriteString(GetEnvironment(user_data_dir_));
  channel_->Send(message);
}

void ServiceListener::Disconnect() {
  channel_.reset();
}

void ServiceListener::Connect() {
  base::win::ScopedHandle handle(
      ::CreateFile(SetupListener::kSetupPipeName, GENERIC_READ | GENERIC_WRITE,
                   0, NULL, OPEN_EXISTING,
                   SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION |
                   FILE_FLAG_OVERLAPPED, NULL));
  if (handle.IsValid()) {
    channel_.reset(new IPC::Channel(IPC::ChannelHandle(handle),
                                    IPC::Channel::MODE_CLIENT, this));
    channel_->Connect();
  } else {
    ipc_thread_->message_loop()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&ServiceListener::Connect, base::Unretained(this)),
        base::TimeDelta::FromMilliseconds(500));
  }
}

