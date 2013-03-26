// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/service/win/setup_listener.h"

#include <atlbase.h>
#include <atlsecurity.h>

#include "base/bind.h"
#include "base/guid.h"
#include "base/json/json_reader.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "ipc/ipc_channel.h"

const char SetupListener::kXpsAvailibleJsonValueName[] = "xps_availible";
const char SetupListener::kChromePathJsonValueName[] = "chrome_path";
const char SetupListener::kPrintersJsonValueName[] = "printers";
const char SetupListener::kUserDataDirJsonValueName[] = "user_data_dir";
const char SetupListener::kUserNameJsonValueName[] = "user_name";
const wchar_t SetupListener::kSetupPipeName[] =
    L"\\\\.\\pipe\\CloudPrintServiceSetup";

SetupListener::SetupListener(const string16& user)
    : done_event_(new base::WaitableEvent(true, false)),
      ipc_thread_(new base::Thread("ipc_thread")),
      succeded_(false),
      is_xps_availible_(false) {
  ipc_thread_->StartWithOptions(base::Thread::Options(MessageLoop::TYPE_IO, 0));
  ipc_thread_->message_loop()->PostTask(FROM_HERE,
                                        base::Bind(&SetupListener::Connect,
                                                   base::Unretained(this),
                                                   user));
}

SetupListener::~SetupListener() {
  ipc_thread_->message_loop()->PostTask(FROM_HERE,
                                        base::Bind(&SetupListener::Disconnect,
                                                   base::Unretained(this)));
  ipc_thread_->Stop();
}

bool SetupListener::OnMessageReceived(const IPC::Message& msg) {
  PickleIterator iter(msg);
  std::string json_string;
  if (!iter.ReadString(&json_string))
    return false;
  scoped_ptr<base::Value> value(base::JSONReader::Read(json_string));
  const base::DictionaryValue* dictionary = NULL;
  if (!value || !value->GetAsDictionary(&dictionary) || !dictionary) {
    LOG(ERROR) << "Invalid response from service";
    return false;
  }

  const base::ListValue* printers = NULL;
  if (dictionary->GetList(kPrintersJsonValueName, &printers) && printers) {
    for (size_t i = 0; i < printers->GetSize(); ++i) {
      std::string printer;
      if (printers->GetString(i, &printer) && !printer.empty())
        printers_.push_back(printer);
    }
  }
  dictionary->GetBoolean(kXpsAvailibleJsonValueName, &is_xps_availible_);
  dictionary->GetString(kUserNameJsonValueName, &user_name_);

  string16 chrome_path;
  dictionary->GetString(kChromePathJsonValueName, &chrome_path);
  chrome_path_ = base::FilePath(chrome_path);

  string16 user_data_dir;
  dictionary->GetString(kUserDataDirJsonValueName, &user_data_dir);
  user_data_dir_ = base::FilePath(user_data_dir);

  succeded_ = true;
  done_event_->Signal();
  return true;
}

void SetupListener::OnChannelError() {
  done_event_->Signal();
}

bool SetupListener::WaitResponce(const base::TimeDelta& delta) {
  return done_event_->TimedWait(delta) && succeded_;
}

void SetupListener::Disconnect() {
  channel_.reset();
  ipc_thread_->message_loop()->QuitWhenIdle();
}

void SetupListener::Connect(const string16& user) {
  ATL::CDacl dacl;

  ATL::CSid user_sid;
  if (!user_sid.LoadAccount(user.c_str())) {
    LOG(ERROR) << "Unable to load Sid for" << user;
  } else {
    dacl.AddAllowedAce(user_sid, GENERIC_READ | GENERIC_WRITE);
  }

  ATL::CSecurityDesc desk;
  desk.SetDacl(dacl);

  ATL::CSecurityAttributes attribs(desk);

  base::win::ScopedHandle pipe(
      CreateNamedPipe(kSetupPipeName,
                      PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED |
                      FILE_FLAG_FIRST_PIPE_INSTANCE,
                      PIPE_TYPE_BYTE | PIPE_READMODE_BYTE, 1,
                      IPC::Channel::kReadBufferSize,
                      IPC::Channel::kReadBufferSize, 5000, &attribs));
  if (pipe.IsValid()) {
    channel_.reset(new IPC::Channel(IPC::ChannelHandle(pipe),
                                    IPC::Channel::MODE_SERVER, this));
    channel_->Connect();
  }
}

