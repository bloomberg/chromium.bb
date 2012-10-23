// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/command_executor_impl.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/string_number_conversions.h"
#include "base/values.h"
#include "chrome/test/chromedriver/status.h"

namespace {

void ExecuteQuit(
    const base::DictionaryValue& params,
    Status* status,
    scoped_ptr<base::Value>* value) {
}

}  // namespace

CommandExecutorImpl::CommandExecutorImpl()
    : next_session_id_(1) {
  session_command_map_.insert(std::make_pair("quit", base::Bind(
      &ExecuteQuit)));
}

CommandExecutorImpl::~CommandExecutorImpl() {}

void CommandExecutorImpl::ExecuteCommand(
    const std::string& name,
    const base::DictionaryValue& params,
    const std::string& session_id,
    StatusCode* status_code,
    scoped_ptr<base::Value>* value,
    std::string* out_session_id) {
  base::AutoLock auto_lock(lock_);
  Status status(kOk);
  ExecuteCommandInternal(name, params, session_id, &status, value,
                         out_session_id);
  *status_code = status.code();
  if (status.IsError()) {
    base::DictionaryValue* error = new base::DictionaryValue();
    error->SetString("message", status.message());
    value->reset(error);
  }
  if (!*value)
    value->reset(base::Value::CreateNullValue());
}

void CommandExecutorImpl::ExecuteCommandInternal(
    const std::string& name,
    const base::DictionaryValue& params,
    const std::string& session_id,
    Status* status,
    scoped_ptr<base::Value>* value,
    std::string* out_session_id) {
  *out_session_id = session_id;
  if (name == "newSession") {
    ExecuteNewSession(out_session_id);
    return;
  }

  // Although most versions of the C++ std lib supposedly guarantee
  // thread safety for concurrent reads, better safe than sorry.
  SessionCommandMap::const_iterator iter = session_command_map_.find(name);
  if (iter != session_command_map_.end()) {
    SessionCommandCallback command_callback = iter->second;
    base::AutoUnlock auto_unlock(lock_);
    command_callback.Run(params, status, value);
    return;
  }

  *status = Status(kUnknownCommand, name);
}

void CommandExecutorImpl::ExecuteNewSession(std::string* session_id) {
  *session_id = base::IntToString(next_session_id_++);
}
