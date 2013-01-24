// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/commands.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/environment.h"
#include "base/file_util.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/stringprintf.h"
#include "base/sys_info.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome.h"
#include "chrome/test/chromedriver/chrome_launcher.h"
#include "chrome/test/chromedriver/element_util.h"
#include "chrome/test/chromedriver/session.h"
#include "chrome/test/chromedriver/status.h"
#include "chrome/test/chromedriver/util.h"
#include "chrome/test/chromedriver/version.h"
#include "third_party/webdriver/atoms.h"

namespace {

Status CheckChromeVersion(Chrome* chrome, std::string* chrome_version) {
  scoped_ptr<base::Environment> env(base::Environment::Create());
  scoped_ptr<base::Value> chrome_version_value;
  std::string temp_chrome_version;
  Status status = chrome->EvaluateScript(
      "",
      "navigator.appVersion.match(/Chrome\\/.* /)[0].split('/')[1].trim()",
      &chrome_version_value);
  if (status.IsError() ||
      !chrome_version_value->GetAsString(&temp_chrome_version))
    return Status(kUnknownError, "unable to detect Chrome version");

  int build_no;
  std::vector<std::string> chrome_version_parts;
  base::SplitString(temp_chrome_version, '.', &chrome_version_parts);
  if (chrome_version_parts.size() != 4 ||
      !base::StringToInt(chrome_version_parts[2], &build_no)) {
    return Status(kUnknownError, "unrecognized Chrome version: " +
        temp_chrome_version);
  }
  // Allow the version check to be skipped for testing/development purposes.
  if (!env->HasVar("IGNORE_CHROME_VERSION")) {
    if (build_no < kMinimumSupportedChromeBuildNo) {
      return Status(kUnknownError, "Chrome version must be >= " +
          GetMinimumSupportedChromeVersion());
    }
  }
  *chrome_version = temp_chrome_version;
  return Status(kOk);
}

}  // namespace

Status ExecuteGetStatus(
    const base::DictionaryValue& params,
    const std::string& session_id,
    scoped_ptr<base::Value>* out_value,
    std::string* out_session_id) {
  base::DictionaryValue build;
  build.SetString("version", "alpha");

  base::DictionaryValue os;
  os.SetString("name", base::SysInfo::OperatingSystemName());
  os.SetString("version", base::SysInfo::OperatingSystemVersion());
  os.SetString("arch", base::SysInfo::OperatingSystemArchitecture());

  base::DictionaryValue info;
  info.Set("build", build.DeepCopy());
  info.Set("os", os.DeepCopy());
  out_value->reset(info.DeepCopy());
  return Status(kOk);
}

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

  std::string chrome_version;
  status = CheckChromeVersion(chrome.get(), &chrome_version);
  if (status.IsError()) {
    chrome->Quit();
    return status;
  }

  std::string new_id = session_id;
  if (new_id.empty())
    new_id = GenerateId();
  scoped_ptr<Session> session(new Session(new_id, chrome.Pass()));
  scoped_refptr<SessionAccessor> accessor(
      new SessionAccessorImpl(session.Pass()));
  session_map->Set(new_id, accessor);

  base::DictionaryValue* returned_value = new base::DictionaryValue();
  returned_value->SetString("browserName", "chrome");
  returned_value->SetString("version", chrome_version);
  returned_value->SetString("driverVersion", kChromeDriverVersion);
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
    int interval_ms,
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  return FindElement(interval_ms, true, NULL, session, params, value);
}

Status ExecuteFindElements(
    int interval_ms,
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  return FindElement(interval_ms, false, NULL, session, params, value);
}

Status ExecuteFindChildElement(
    int interval_ms,
    Session* session,
    const std::string& element_id,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  return FindElement(
      interval_ms, true, &element_id, session, params, value);
}

Status ExecuteFindChildElements(
    int interval_ms,
    Session* session,
    const std::string& element_id,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  return FindElement(
      interval_ms, false, &element_id, session, params, value);
}

Status ExecuteHoverOverElement(
    Session* session,
    const std::string& element_id,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  WebPoint location;
  Status status = GetElementClickableLocation(
      session, element_id, &location, NULL);
  if (status.IsError())
    return status;

  MouseEvent move_event(
      kMovedMouseEventType, kNoneMouseButton, location.x, location.y, 0);
  std::list<MouseEvent> events;
  events.push_back(move_event);
  return session->chrome->DispatchMouseEvents(events);
}

Status ExecuteClickElement(
    Session* session,
    const std::string& element_id,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  std::string tag_name;
  Status status = GetElementTagName(session, element_id, &tag_name);
  if (status.IsError())
    return status;
  if (tag_name == "option") {
    bool is_toggleable;
    status = IsOptionElementTogglable(session, element_id, &is_toggleable);
    if (status.IsError())
      return status;
    if (is_toggleable)
      return ToggleOptionElement(session, element_id);
    else
      return SetOptionElementSelected(session, element_id, true);
  } else {
    WebPoint location;
    bool is_clickable;
    status = GetElementClickableLocation(
        session, element_id, &location, &is_clickable);
    if (status.IsError())
      return status;
    if (!is_clickable)
      return Status(kUnknownError, status.message());

    std::list<MouseEvent> events;
    events.push_back(
        MouseEvent(kMovedMouseEventType, kNoneMouseButton,
                   location.x, location.y, 0));
    events.push_back(
        MouseEvent(kPressedMouseEventType, kLeftMouseButton,
                   location.x, location.y, 1));
    events.push_back(
        MouseEvent(kReleasedMouseEventType, kLeftMouseButton,
                   location.x, location.y, 1));
    return session->chrome->DispatchMouseEvents(events);
  }
}

Status ExecuteClearElement(
    Session* session,
    const std::string& element_id,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  base::ListValue args;
  args.Append(CreateElement(element_id));
  scoped_ptr<base::Value> result;
  return session->chrome->CallFunction(
      session->frame,
      webdriver::atoms::asString(webdriver::atoms::CLEAR),
      args, &result);
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
