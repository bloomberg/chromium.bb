// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/policy_tool_ui_handler.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_scheduler/post_task.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"

// static
const base::FilePath::CharType PolicyToolUIHandler::kPolicyToolSessionsDir[] =
    FILE_PATH_LITERAL("Policy sessions");

// static
const base::FilePath::CharType
    PolicyToolUIHandler::kPolicyToolDefaultSessionName[] =
        FILE_PATH_LITERAL("policy");

// static
const base::FilePath::CharType
    PolicyToolUIHandler::kPolicyToolSessionExtension[] =
        FILE_PATH_LITERAL("json");

PolicyToolUIHandler::PolicyToolUIHandler() : callback_weak_ptr_factory_(this) {}

PolicyToolUIHandler::~PolicyToolUIHandler() {}

void PolicyToolUIHandler::RegisterMessages() {
  // Set directory for storing sessions.
  sessions_dir_ =
      Profile::FromWebUI(web_ui())->GetPath().Append(kPolicyToolSessionsDir);

  web_ui()->RegisterMessageCallback(
      "initialized", base::Bind(&PolicyToolUIHandler::HandleInitializedAdmin,
                                base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "loadSession", base::Bind(&PolicyToolUIHandler::HandleLoadSession,
                                base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "updateSession", base::Bind(&PolicyToolUIHandler::HandleUpdateSession,
                                  base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "resetSession", base::Bind(&PolicyToolUIHandler::HandleResetSession,
                                 base::Unretained(this)));
}

void PolicyToolUIHandler::OnJavascriptDisallowed() {
  callback_weak_ptr_factory_.InvalidateWeakPtrs();
}

base::FilePath PolicyToolUIHandler::GetSessionPath(
    const base::FilePath::StringType& name) const {
  return sessions_dir_.Append(name).AddExtension(kPolicyToolSessionExtension);
}

base::ListValue PolicyToolUIHandler::GetSessionsList() {
  base::FilePath::StringType sessions_pattern =
      FILE_PATH_LITERAL("*.") +
      base::FilePath::StringType(kPolicyToolSessionExtension);
  base::FileEnumerator enumerator(sessions_dir_, /*recursive=*/false,
                                  base::FileEnumerator::FILES,
                                  sessions_pattern);
  // A vector of session names and their last access times.
  using Session = std::pair<base::Time, base::FilePath::StringType>;
  std::vector<Session> sessions;
  for (base::FilePath name = enumerator.Next(); !name.empty();
       name = enumerator.Next()) {
    base::File::Info info;
    base::GetFileInfo(name, &info);
    sessions.push_back(
        {info.last_accessed, name.BaseName().RemoveExtension().value()});
  }
  // Sort the sessions by the the time of last access in decreasing order.
  std::sort(sessions.begin(), sessions.end(), std::greater<Session>());

  // Convert sessions to the list containing only names.
  base::ListValue session_names;
  for (const Session& session : sessions)
    session_names.GetList().push_back(base::Value(session.second));
  return session_names;
}

void PolicyToolUIHandler::SetDefaultSessionName() {
  base::ListValue sessions = GetSessionsList();
  if (sessions.empty()) {
    // If there are no sessions, fallback to the default session name.
    session_name_ = kPolicyToolDefaultSessionName;
  } else {
    session_name_ =
        base::FilePath::FromUTF8Unsafe(sessions.GetList()[0].GetString())
            .value();
  }
}

std::string PolicyToolUIHandler::ReadOrCreateFileCallback() {
  // Create sessions directory, if it doesn't exist yet.
  // If unable to create a directory, just silently return a dictionary
  // indicating that saving was unsuccessful.
  // TODO(urusant): add a possibility to disable saving to disk in similar
  // cases.
  if (!base::CreateDirectory(sessions_dir_))
    is_saving_enabled_ = false;

  // Initialize session name if it is not initialized yet.
  if (session_name_.empty())
    SetDefaultSessionName();
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
  // a dictionary indicating that saving was unsuccessful. Potentially this can
  // also be the place to disable saving to disk.
  if (!PathExists(session_path))
    is_saving_enabled_ = false;

  // Read file contents.
  std::string contents;
  base::ReadFileToString(session_path, &contents);

  // Touch the file to remember the last session.
  base::File::Info info;
  base::GetFileInfo(session_path, &info);
  base::TouchFile(session_path, base::Time::Now(), info.last_modified);

  return contents;
}

void PolicyToolUIHandler::OnFileRead(const std::string& contents) {
  // If the saving is disabled, send a message about that to the UI.
  if (!is_saving_enabled_) {
    CallJavascriptFunction("policy.Page.disableSaving");
    return;
  }
  std::unique_ptr<base::DictionaryValue> value =
      base::DictionaryValue::From(base::JSONReader::Read(contents));

  // If contents is not a properly formed JSON string, disable editing in the
  // UI to prevent the user from accidentally overriding it.
  if (!value) {
    CallJavascriptFunction("policy.Page.setPolicyValues",
                           base::DictionaryValue());
    CallJavascriptFunction("policy.Page.disableEditing");
  } else {
    // TODO(urusant): convert the policy values so that the types are
    // consistent with actual policy types.
    CallJavascriptFunction("policy.Page.setPolicyValues", *value);
    base::PostTaskWithTraitsAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce(&PolicyToolUIHandler::GetSessionsList,
                       base::Unretained(this)),
        base::BindOnce(&PolicyToolUIHandler::OnSessionsListReceived,
                       callback_weak_ptr_factory_.GetWeakPtr()));
  }
}

void PolicyToolUIHandler::OnSessionsListReceived(base::ListValue list) {
  CallJavascriptFunction("policy.Page.setSessionsList", list);
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
  is_saving_enabled_ = true;
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
  base::FilePath::StringType new_session_name =
      base::FilePath::FromUTF8Unsafe(args->GetList()[0].GetString()).value();
  if (!IsValidSessionName(new_session_name)) {
    CallJavascriptFunction("policy.Page.showInvalidSessionNameError");
    return;
  }
  session_name_ = new_session_name;
  ImportFile();
}

bool PolicyToolUIHandler::DoUpdateSession(const std::string& contents) {
  // Sanity check that contents is not too big. Otherwise, passing it to
  // WriteFile will be int overflow.
  if (contents.size() > static_cast<size_t>(std::numeric_limits<int>::max()))
    return false;
  int bytes_written = base::WriteFile(GetSessionPath(session_name_),
                                      contents.c_str(), contents.size());
  return bytes_written == static_cast<int>(contents.size());
}

void PolicyToolUIHandler::OnSessionUpdated(bool is_successful) {
  if (!is_successful) {
    is_saving_enabled_ = false;
    CallJavascriptFunction("policy.Page.disableSaving");
  }
}

void PolicyToolUIHandler::HandleUpdateSession(const base::ListValue* args) {
  DCHECK(is_saving_enabled_);
  DCHECK_EQ(1U, args->GetSize());

  const base::DictionaryValue* policy_values = nullptr;
  args->GetDictionary(0, &policy_values);
  DCHECK(policy_values);
  std::string converted_values;
  base::JSONWriter::Write(*policy_values, &converted_values);
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BACKGROUND},
      base::BindOnce(&PolicyToolUIHandler::DoUpdateSession,
                     base::Unretained(this), converted_values),
      base::BindOnce(&PolicyToolUIHandler::OnSessionUpdated,
                     callback_weak_ptr_factory_.GetWeakPtr()));
}

void PolicyToolUIHandler::HandleResetSession(const base::ListValue* args) {
  DCHECK_EQ(0U, args->GetSize());
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BACKGROUND},
      base::BindOnce(&PolicyToolUIHandler::DoUpdateSession,
                     base::Unretained(this), "{}"),
      base::BindOnce(&PolicyToolUIHandler::OnSessionUpdated,
                     callback_weak_ptr_factory_.GetWeakPtr()));
}
