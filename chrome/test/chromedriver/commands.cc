// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/commands.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/format_macros.h"
#include "base/rand_util.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome.h"
#include "chrome/test/chromedriver/chrome_launcher.h"
#include "chrome/test/chromedriver/session.h"
#include "chrome/test/chromedriver/status.h"

Status ExecuteNewSession(
    SessionMap* session_map,
    ChromeLauncher* launcher,
    const base::DictionaryValue& params,
    const std::string& session_id,
    scoped_ptr<base::Value>* out_value,
    std::string* out_session_id) {
  scoped_ptr<Chrome> chrome;
  FilePath::StringType path_str;
  FilePath chrome_exe;
  if (params.GetString("desiredCapabilities.chrome.binary", &path_str)) {
    chrome_exe = FilePath(path_str);
    if (!file_util::PathExists(chrome_exe)) {
      std::string message = base::StringPrintf(
          "no chrome binary at %" PRFilePath,
          path_str.c_str());
      return Status(kUnknownError, message);
    }
  }
  Status status = launcher->Launch(chrome_exe, &chrome);
  if (status.IsError())
    return Status(kSessionNotCreatedException, status.message());

  uint64 msb = base::RandUint64();
  uint64 lsb = base::RandUint64();
  std::string new_id =
      base::StringPrintf("%016" PRIx64 "%016" PRIx64, msb, lsb);
  scoped_ptr<Session> session(new Session(new_id, chrome.Pass()));
  scoped_refptr<SessionAccessor> accessor(
      new SessionAccessorImpl(session.Pass()));
  session_map->Set(new_id, accessor);

  out_value->reset(new base::StringValue(new_id));
  *out_session_id = new_id;
  return Status(kOk);
}

Status ExecuteQuitAll(
    Command quit_command,
    SessionMap* session_map,
    const base::DictionaryValue& params,
    const std::string& session_id,
    scoped_ptr<base::Value>* out_value,
    std::string* out_session_id) {
  std::vector<std::string> session_ids;
  session_map->GetKeys(&session_ids);
  for (size_t i = 0; i < session_ids.size(); ++i) {
    scoped_ptr<base::Value> unused_value;
    std::string unused_session_id;
    quit_command.Run(params, session_ids[i], &unused_value, &unused_session_id);
  }
  return Status(kOk);
}

Status ExecuteQuit(
    SessionMap* session_map,
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  CHECK(session_map->Remove(session->id));
  return session->chrome->Quit();
}

Status ExecuteGet(
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  std::string url;
  if (!params.GetString("url", &url))
    return Status(kUnknownError, "'url' must be a string");
  return session->chrome->Load(url);
}

Status ExecuteExecuteScript(
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  std::string script;
  if (!params.GetString("script", &script))
    return Status(kUnknownError, "'script' must be a string");
  const base::ListValue* args;
  if (!params.GetList("args", &args))
    return Status(kUnknownError, "'args' must be a list");

  return session->chrome->CallFunction(
      "function(){" + script + "}", *args, value);
}
