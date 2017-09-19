// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/policy_tool_ui_handler.h"

#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_scheduler/post_task.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"

PolicyToolUIHandler::PolicyToolUIHandler() : callback_weak_ptr_factory_(this) {}

PolicyToolUIHandler::~PolicyToolUIHandler() {}

void PolicyToolUIHandler::RegisterMessages() {
  // Set directory for storing sessions.
  sessions_dir_ = Profile::FromWebUI(web_ui())->GetPath().Append(
      FILE_PATH_LITERAL("Policy sessions"));
  // Set current session name.
  // TODO(urusant): do so in a smarter way, e.g. choose the last edited session.
  session_name_ = FILE_PATH_LITERAL("policy");

  web_ui()->RegisterMessageCallback(
      "initialized", base::Bind(&PolicyToolUIHandler::HandleInitializedAdmin,
                                base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "loadSession", base::Bind(&PolicyToolUIHandler::HandleLoadSession,
                                base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "updateSession", base::Bind(&PolicyToolUIHandler::HandleUpdateSession,
                                  base::Unretained(this)));
}

void PolicyToolUIHandler::OnJavascriptDisallowed() {
  callback_weak_ptr_factory_.InvalidateWeakPtrs();
}

base::FilePath PolicyToolUIHandler::GetSessionPath(
    const base::FilePath::StringType& name) const {
  return sessions_dir_.Append(name).AddExtension(FILE_PATH_LITERAL("json"));
}

std::string PolicyToolUIHandler::ReadOrCreateFileCallback() {
  // Create sessions directory, if it doesn't exist yet.
  // If unable to create a directory, just silently return a dictionary
  // indicating that logging was unsuccessful.
  // TODO(urusant): add a possibility to disable logging to disk in similar
  // cases.
  if (!base::CreateDirectory(sessions_dir_))
    return "{\"logged\": false}";

  const base::FilePath session_path = GetSessionPath(session_name_);
  // Check if the file for the current session already exists. If not, create it
  // and put an empty dictionary in it.
  base::File session_file(session_path, base::File::Flags::FLAG_CREATE |
                                            base::File::Flags::FLAG_WRITE);

  // If unable to open the file, just return an empty dictionary.
  if (session_file.created()) {
    session_file.WriteAtCurrentPos("{}", 2);
    return "{}";
  }
  session_file.Close();

  // Check that the file exists by now. If it doesn't, it means that at least
  // one of the filesystem operations wasn't successful. In this case, return
  // a dictionary indicating that logging was unsuccessful. Potentially this can
  // also be the place to disable logging to disk.
  if (!PathExists(session_path)) {
    return "{\"logged\": false}";
  }
  // Read file contents.
  std::string contents;
  base::ReadFileToString(session_path, &contents);
  return contents;
}

void PolicyToolUIHandler::OnFileRead(const std::string& contents) {
  std::unique_ptr<base::DictionaryValue> value =
      base::DictionaryValue::From(base::JSONReader::Read(contents));

  // If contents is not a properly formed JSON string, we alert the user about
  // it and send an empty dictionary instead. Note that the broken session file
  // would be overrided when the user edits something, so any manual changes
  // that made it invalid will be lost.
  // TODO(urusant): do it in a smarter way, e.g. revert to a previous session or
  // a new session with a generated name.
  if (!value) {
    value = base::MakeUnique<base::DictionaryValue>();
    ShowErrorMessageToUser("errorFileCorrupted");
  } else {
    bool logged;
    if (value->GetBoolean("logged", &logged)) {
      if (!logged) {
        ShowErrorMessageToUser("errorLoggingDisabled");
      }
      value->Remove("logged", nullptr);
    }
  }
  CallJavascriptFunction("policy.Page.setPolicyValues", *value);
}

void PolicyToolUIHandler::ImportFile() {
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&PolicyToolUIHandler::ReadOrCreateFileCallback,
                     base::Unretained(this)),
      base::BindOnce(&PolicyToolUIHandler::OnFileRead,
                     callback_weak_ptr_factory_.GetWeakPtr()));
}

void PolicyToolUIHandler::HandleInitializedAdmin(const base::ListValue* args) {
  DCHECK_EQ(0U, args->GetSize());
  AllowJavascript();
  SendPolicyNames();
  ImportFile();
}

bool PolicyToolUIHandler::IsValidSessionName(
    const base::FilePath::StringType& name) const {
  // Check if the session name is valid, which means that it doesn't use
  // filesystem navigation (e.g. ../ or nested folder).
  base::FilePath session_path = GetSessionPath(name);
  return !session_path.empty() && session_path.DirName() == sessions_dir_;
}

void PolicyToolUIHandler::HandleLoadSession(const base::ListValue* args) {
  DCHECK_EQ(1U, args->GetSize());
#if defined(OS_WIN)
  base::FilePath::StringType new_session_name =
      base::UTF8ToUTF16(args->GetList()[0].GetString());
#else
  base::FilePath::StringType new_session_name = args->GetList()[0].GetString();
#endif
  if (!IsValidSessionName(new_session_name)) {
    ShowErrorMessageToUser("errorInvalidSessionName");
    return;
  }
  session_name_ = new_session_name;
  ImportFile();
}

void PolicyToolUIHandler::DoUpdateSession(const std::string& contents) {
  if (base::WriteFile(GetSessionPath(session_name_), contents.c_str(),
                      contents.size()) < static_cast<int>(contents.size())) {
    ShowErrorMessageToUser("errorLoggingDisabled");
  }
}

void PolicyToolUIHandler::HandleUpdateSession(const base::ListValue* args) {
  DCHECK_EQ(1U, args->GetSize());

  const base::DictionaryValue* policy_values = nullptr;
  args->GetDictionary(0, &policy_values);
  DCHECK(policy_values);
  std::string converted_values;
  base::JSONWriter::Write(*policy_values, &converted_values);
  base::PostTaskWithTraits(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BACKGROUND},
      base::BindOnce(&PolicyToolUIHandler::DoUpdateSession,
                     callback_weak_ptr_factory_.GetWeakPtr(),
                     converted_values));
}

void PolicyToolUIHandler::ShowErrorMessageToUser(
    const std::string& message_name) {
  CallJavascriptFunction("policy.showErrorMessage", base::Value(message_name));
}
