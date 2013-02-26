// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/window_commands.h"

#include <list>

#include "base/callback.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/test/chromedriver/basic_types.h"
#include "chrome/test/chromedriver/chrome.h"
#include "chrome/test/chromedriver/element_util.h"
#include "chrome/test/chromedriver/session.h"
#include "chrome/test/chromedriver/status.h"
#include "chrome/test/chromedriver/ui_events.h"
#include "chrome/test/chromedriver/web_view.h"

namespace {

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

}  // namespace

Status ExecuteWindowCommand(
    WindowCommand command,
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  bool is_dialog_open;
  Status status = session->chrome->IsJavaScriptDialogOpen(&is_dialog_open);
  if (status.IsError())
    return status;
  if (is_dialog_open)
    return Status(kUnexpectedAlertOpen);

  WebView* web_view = NULL;
  status = session->GetTargetWindow(&web_view);
  if (status.IsError())
    return status;

  Status nav_status = web_view->WaitForPendingNavigations(session->frame);
  if (nav_status.IsError())
    return nav_status;
  status = command.Run(session, web_view, params, value);
  // Switch to main frame and retry command if subframe no longer exists.
  if (status.code() == kNoSuchFrame) {
    session->frame = "";
    nav_status = web_view->WaitForPendingNavigations(session->frame);
    if (nav_status.IsError())
      return nav_status;
    status = command.Run(session, web_view, params, value);
  }
  nav_status = web_view->WaitForPendingNavigations(session->frame);
  if (status.IsOk() && nav_status.IsError() &&
      nav_status.code() != kDisconnected)
    return nav_status;
  return status;
}

Status ExecuteGet(
    Session* session,
    WebView* web_view,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  std::string url;
  if (!params.GetString("url", &url))
    return Status(kUnknownError, "'url' must be a string");
  return web_view->Load(url);
}

Status ExecuteExecuteScript(
    Session* session,
    WebView* web_view,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  std::string script;
  if (!params.GetString("script", &script))
    return Status(kUnknownError, "'script' must be a string");
  const base::ListValue* args;
  if (!params.GetList("args", &args))
    return Status(kUnknownError, "'args' must be a list");

  return web_view->CallFunction(
      session->frame, "function(){" + script + "}", *args, value);
}

Status ExecuteSwitchToFrame(
    Session* session,
    WebView* web_view,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  const base::Value* id;
  if (!params.Get("id", &id))
    return Status(kUnknownError, "missing 'id'");

  if (id->IsType(base::Value::TYPE_NULL)) {
    session->frame = "";
    return Status(kOk);
  }

  std::string script;
  base::ListValue args;
  const base::DictionaryValue* id_dict;
  if (id->GetAsDictionary(&id_dict)) {
    script = "function(elem) { return elem; }";
    args.Append(id_dict->DeepCopy());
  } else {
    script =
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
    } else {
      return Status(kUnknownError, "invalid 'id'");
    }
    args.Append(new base::StringValue(xpath));
  }
  std::string frame;
  Status status = web_view->GetFrameByFunction(
      session->frame, script, args, &frame);
  if (status.IsError())
    return status;
  session->frame = frame;
  return Status(kOk);
}

Status ExecuteGetTitle(
    Session* session,
    WebView* web_view,
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
  return web_view->CallFunction(session->frame, kGetTitleScript, args, value);
}

Status ExecuteGetPageSource(
    Session* session,
    WebView* web_view,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  const char* kGetPageSource =
      "function() {"
      "  return new XMLSerializer().serializeToString(document);"
      "}";
  base::ListValue args;
  return web_view->CallFunction(session->frame, kGetPageSource, args, value);
}

Status ExecuteFindElement(
    int interval_ms,
    Session* session,
    WebView* web_view,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  return FindElement(interval_ms, true, NULL, session, web_view, params, value);
}

Status ExecuteFindElements(
    int interval_ms,
    Session* session,
    WebView* web_view,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  return FindElement(
      interval_ms, false, NULL, session, web_view, params, value);
}

Status ExecuteGetCurrentUrl(
    Session* session,
    WebView* web_view,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  base::ListValue args;
  return web_view->CallFunction(
      "", "function() { return document.URL; }", args, value);
}

Status ExecuteGoBack(
    Session* session,
    WebView* web_view,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  return web_view->EvaluateScript("", "window.history.back();", value);
}

Status ExecuteGoForward(
    Session* session,
    WebView* web_view,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  return web_view->EvaluateScript("", "window.history.forward();", value);
}

Status ExecuteRefresh(
    Session* session,
    WebView* web_view,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  return web_view->Reload();
}

Status ExecuteMouseMoveTo(
    Session* session,
    WebView* web_view,
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
    Status status = ScrollElementIntoView(
        session, web_view, element_id, &location);
    if (status.IsError())
      return status;
  } else {
    location = session->mouse_position;
  }

  if (has_offset) {
    location.offset(x_offset, y_offset);
  } else {
    WebSize size;
    Status status = GetElementSize(session, web_view, element_id, &size);
    if (status.IsError())
      return status;
    location.offset(size.width / 2, size.height / 2);
  }

  std::list<MouseEvent> events;
  events.push_back(
      MouseEvent(kMovedMouseEventType, kNoneMouseButton,
                 location.x, location.y, 0));
  Status status = web_view->DispatchMouseEvents(events);
  if (status.IsOk())
    session->mouse_position = location;
  return status;
}

Status ExecuteMouseClick(
    Session* session,
    WebView* web_view,
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
  return web_view->DispatchMouseEvents(events);
}

Status ExecuteMouseButtonDown(
    Session* session,
    WebView* web_view,
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
  return web_view->DispatchMouseEvents(events);
}

Status ExecuteMouseButtonUp(
    Session* session,
    WebView* web_view,
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
  return web_view->DispatchMouseEvents(events);
}

Status ExecuteMouseDoubleClick(
    Session* session,
    WebView* web_view,
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
  return web_view->DispatchMouseEvents(events);
}
