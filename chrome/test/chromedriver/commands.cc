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
#include "base/time.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome.h"
#include "chrome/test/chromedriver/chrome_launcher.h"
#include "chrome/test/chromedriver/session.h"
#include "chrome/test/chromedriver/status.h"
#include "third_party/webdriver/atoms.h"

namespace {

Status FindElementByJs(
    bool only_one,
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  std::string strategy;
  if (!params.GetString("using", &strategy))
    return Status(kUnknownError, "'using' must be a string");
  std::string target;
  if (!params.GetString("value", &target))
    return Status(kUnknownError, "'value' must be a string");

  if (strategy == "class name")
    strategy = "className";
  else if (strategy == "css selector")
    strategy = "css";
  else if (strategy == "link text")
    strategy = "linkText";
  else if (strategy == "partial link text")
    strategy = "partialLinkText";
  else if (strategy == "tag name")
    strategy = "tagName";

  std::string script;
  if (only_one)
    script = webdriver::atoms::asString(webdriver::atoms::FIND_ELEMENT);
  else
    script = webdriver::atoms::asString(webdriver::atoms::FIND_ELEMENTS);
  scoped_ptr<base::DictionaryValue> locator(new base::DictionaryValue());
  locator->SetString(strategy, target);
  base::ListValue arguments;
  arguments.Append(locator.release());

  base::Time start_time = base::Time::Now();
  while (true) {
    scoped_ptr<base::Value> temp;
    Status status = session->chrome->CallFunction(
        session->frame, script, arguments, &temp);
    if (status.IsError())
      return status;

    if (!temp->IsType(base::Value::TYPE_NULL)) {
      if (only_one) {
        value->reset(temp.release());
        return Status(kOk);
      } else {
        base::ListValue* result;
        if (!temp->GetAsList(&result))
          return Status(kUnknownError, "script returns unexpected result");

        if (result->GetSize() > 0U) {
          value->reset(temp.release());
          return Status(kOk);
        }
      }
    }

    if ((base::Time::Now() - start_time).InMilliseconds() >=
        session->implicit_wait) {
      if (only_one) {
        return Status(kNoSuchElement);
      } else {
        value->reset(new base::ListValue());
        return Status(kOk);
      }
    }
    base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(50));
  }

  return Status(kUnknownError);
}

}  // namespace

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

Status ExecuteGetTitle(
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  const char* kGetTitleScript =
      "function() {"
      "  if (document.title)"
      "    return document.title;"
      "  else"
      "    return document.URL;"
      "}";
  base::ListValue args;
  return session->chrome->CallFunction(
      session->frame, kGetTitleScript, args, value);
}

Status ExecuteFindElement(
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  return FindElementByJs(true, session, params, value);
}

Status ExecuteFindElements(
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  return FindElementByJs(false, session, params, value);
}

Status ExecuteSetTimeout(
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  int ms;
  if (!params.GetInteger("ms", &ms) || ms < 0)
    return Status(kUnknownError, "'ms' must be a non-negative integer");
  std::string type;
  if (!params.GetString("type", &type))
    return Status(kUnknownError, "'type' must be a string");
  if (type == "implicit")
    session->implicit_wait = ms;
  else if (type == "script")
    session->script_timeout = ms;
  else if (type == "page load")
    session->page_load_timeout = ms;
  else
    return Status(kUnknownError, "unknown type of timeout:" + type);
  return Status(kOk);
}

Status ExecuteGetCurrentUrl(
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  base::ListValue args;
  return session->chrome->CallFunction(
      "", "function() { return document.URL; }", args, value);
}

Status ExecuteGoBack(
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  return session->chrome->EvaluateScript(
      "", "window.history.back();", value);
}

Status ExecuteGoForward(
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  return session->chrome->EvaluateScript(
      "", "window.history.forward();", value);
}

Status ExecuteRefresh(
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  return session->chrome->Reload();
}
