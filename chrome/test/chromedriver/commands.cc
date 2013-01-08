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
  if (params.GetString("desiredCapabilities.chromeOptions.binary", &path_str)) {
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

  base::DictionaryValue* returned_value = new base::DictionaryValue();
  returned_value->SetString("browserName", "chrome");
  out_value->reset(returned_value);
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
      session->frame, "function(){" + script + "}", *args, value);
}

Status ExecuteSwitchToFrame(
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  const base::Value* id;
  if (!params.Get("id", &id))
    return Status(kUnknownError, "missing 'id'");

  if (id->IsType(base::Value::TYPE_NULL)) {
    session->frame = "";
    return Status(kOk);
  }

  std::string evaluate_xpath_script =
      "function(xpath) {"
      "  return document.evaluate(xpath, document, null, "
      "      XPathResult.FIRST_ORDERED_NODE_TYPE, null).singleNodeValue;"
      "}";
  std::string xpath = "(/html/body//iframe|/html/frameset/frame)";
  std::string id_string;
  int id_int;
  if (id->GetAsString(&id_string)) {
    xpath += base::StringPrintf(
        "[@name=\"%s\" or @id=\"%s\"]", id_string.c_str(), id_string.c_str());
  } else if (id->GetAsInteger(&id_int)) {
    xpath += base::StringPrintf("[%d]", id_int + 1);
  } else if (id->IsType(base::Value::TYPE_DICTIONARY)) {
    // TODO(kkania): Implement.
    return Status(kUnknownError, "frame switching by element not implemented");
  } else {
    return Status(kUnknownError, "invalid 'id'");
  }
  base::ListValue args;
  args.Append(new base::StringValue(xpath));
  std::string frame;
  Status status = session->chrome->GetFrameByFunction(
      session->frame, evaluate_xpath_script, args, &frame);
  if (status.IsError())
    return status;
  session->frame = frame;
  return Status(kOk);
}
