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
#include "base/third_party/icu/icu_utf.h"
#include "base/values.h"
#include "chrome/test/chromedriver/basic_types.h"
#include "chrome/test/chromedriver/chrome.h"
#include "chrome/test/chromedriver/chrome_launcher.h"
#include "chrome/test/chromedriver/element_util.h"
#include "chrome/test/chromedriver/js.h"
#include "chrome/test/chromedriver/key_converter.h"
#include "chrome/test/chromedriver/session.h"
#include "chrome/test/chromedriver/status.h"
#include "chrome/test/chromedriver/ui_events.h"
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

Status GetMouseButton(const base::DictionaryValue& params,
                      MouseButton* button) {
  int button_num;
  if (!params.GetInteger("button", &button_num)) {
    button_num = 0;  // Default to left mouse button.
  } else if (button_num < 0 || button_num > 2) {
    return Status(kUnknownError,
                  base::StringPrintf("invalid button: %d", button_num));
  }
  *button = static_cast<MouseButton>(button_num);
  return Status(kOk);
}

Status FlattenStringArray(const ListValue* src, string16* dest) {
  string16 keys;
  for (size_t i = 0; i < src->GetSize(); ++i) {
    string16 keys_list_part;
    if (!src->GetString(i, &keys_list_part))
      return Status(kUnknownError, "keys should be a string");
    for (size_t j = 0; j < keys_list_part.size(); ++j) {
      if (CBU16_IS_SURROGATE(keys_list_part[j])) {
        return Status(kUnknownError,
                      "ChromeDriver only supports characters in the BMP");
      }
    }
    keys.append(keys_list_part);
  }
  *dest = keys;
  return Status(kOk);
}

Status SendKeysOnSession(
    Session* session,
    const string16 keys,
    bool release_modifiers) {
  std::list<KeyEvent> events;
  int sticky_modifiers = 0;
  Status status = ConvertKeysToKeyEvents(
      keys, release_modifiers, &sticky_modifiers, &events);
  if (status.IsError())
    return status;
  return session->chrome->DispatchKeyEvents(events);
}

Status SendKeysToElement(
    Session* session,
    const std::string& element_id,
    const string16 keys) {
  bool is_displayed = false;
  Status status = IsElementDisplayed(session, element_id, true, &is_displayed);
  if (status.IsError())
    return status;
  if (!is_displayed)
    return Status(kElementNotVisible);
  bool is_enabled = false;
  status = IsElementEnabled(session, element_id, &is_enabled);
  if (status.IsError())
    return status;
  if (!is_enabled)
    return Status(kInvalidElementState);
  base::ListValue args;
  args.Append(CreateElement(element_id));
  scoped_ptr<base::Value> result;
  status = session->chrome->CallFunction(
      session->frame, kFocusScript, args, &result);
  if (status.IsError())
    return status;
  return SendKeysOnSession(session, keys, true);
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
  status = session->chrome->DispatchMouseEvents(events);
  if (status.IsOk())
    session->mouse_position = location;
  return status;
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
    status = session->chrome->DispatchMouseEvents(events);
    if (status.IsOk())
      session->mouse_position = location;
    return status;
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

Status ExecuteSendKeysToElement(
    Session* session,
    const std::string& element_id,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  const base::ListValue* keys_list;
  if (!params.GetList("value", &keys_list))
    return Status(kUnknownError, "'value' must be a list");

  bool is_input = false;
  Status status = IsElementAttributeEqualToIgnoreCase(
      session, element_id, "tagName", "input", &is_input);
  if (status.IsError())
    return status;
  bool is_file = false;
  status = IsElementAttributeEqualToIgnoreCase(
      session, element_id, "type", "file", &is_file);
  if (status.IsError())
    return status;
  if (is_input && is_file) {
    // TODO(chrisgao): Implement file upload.
    return Status(kUnknownError, "file upload is not implemented");
  } else {
    string16 keys;
    status = FlattenStringArray(keys_list, &keys);
    if (status.IsError())
      return status;
    return SendKeysToElement(session, element_id, keys);
  }
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

Status ExecuteMouseMoveTo(
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  std::string element_id;
  bool has_element = params.GetString("element", &element_id);
  int x_offset = 0;
  int y_offset = 0;
  bool has_offset = params.GetInteger("xoffset", &x_offset) &&
      params.GetInteger("yoffset", &y_offset);
  if (!has_element && !has_offset)
    return Status(kUnknownError, "at least an element or offset should be set");

  WebPoint location;
  if (has_element) {
    Status status = ScrollElementIntoView(session, element_id, &location);
    if (status.IsError())
      return status;
  } else {
    location = session->mouse_position;
  }

  if (has_offset) {
    location.offset(x_offset, y_offset);
  } else {
    WebSize size;
    Status status = GetElementSize(session, element_id, &size);
    if (status.IsError())
      return status;
    location.offset(size.width / 2, size.height / 2);
  }

  std::list<MouseEvent> events;
  events.push_back(
      MouseEvent(kMovedMouseEventType, kNoneMouseButton,
                 location.x, location.y, 0));
  Status status = session->chrome->DispatchMouseEvents(events);
  if (status.IsOk())
    session->mouse_position = location;
  return status;
}

Status ExecuteMouseClick(
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  MouseButton button;
  Status status = GetMouseButton(params, &button);
  if (status.IsError())
    return status;
  std::list<MouseEvent> events;
  events.push_back(
      MouseEvent(kPressedMouseEventType, button,
                 session->mouse_position.x, session->mouse_position.y, 1));
  events.push_back(
      MouseEvent(kReleasedMouseEventType, button,
                 session->mouse_position.x, session->mouse_position.y, 1));
  return session->chrome->DispatchMouseEvents(events);
}

Status ExecuteMouseButtonDown(
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  MouseButton button;
  Status status = GetMouseButton(params, &button);
  if (status.IsError())
    return status;
  std::list<MouseEvent> events;
  events.push_back(
      MouseEvent(kPressedMouseEventType, button,
                 session->mouse_position.x, session->mouse_position.y, 1));
  return session->chrome->DispatchMouseEvents(events);
}

Status ExecuteMouseButtonUp(
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  MouseButton button;
  Status status = GetMouseButton(params, &button);
  if (status.IsError())
    return status;
  std::list<MouseEvent> events;
  events.push_back(
      MouseEvent(kReleasedMouseEventType, button,
                 session->mouse_position.x, session->mouse_position.y, 1));
  return session->chrome->DispatchMouseEvents(events);
}

Status ExecuteMouseDoubleClick(
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  MouseButton button;
  Status status = GetMouseButton(params, &button);
  if (status.IsError())
    return status;
  std::list<MouseEvent> events;
  events.push_back(
      MouseEvent(kPressedMouseEventType, button,
                 session->mouse_position.x, session->mouse_position.y, 2));
  events.push_back(
      MouseEvent(kReleasedMouseEventType, button,
                 session->mouse_position.x, session->mouse_position.y, 2));
  return session->chrome->DispatchMouseEvents(events);
}
