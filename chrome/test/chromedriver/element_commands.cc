// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/element_commands.h"

#include <list>

#include "base/callback.h"
#include "base/third_party/icu/icu_utf.h"
#include "base/values.h"
#include "chrome/test/chromedriver/basic_types.h"
#include "chrome/test/chromedriver/chrome/chrome.h"
#include "chrome/test/chromedriver/chrome/js.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/ui_events.h"
#include "chrome/test/chromedriver/chrome/web_view.h"
#include "chrome/test/chromedriver/element_util.h"
#include "chrome/test/chromedriver/key_converter.h"
#include "chrome/test/chromedriver/session.h"
#include "third_party/webdriver/atoms.h"

namespace {

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

Status SendKeysOnWindow(
    WebView* web_view,
    const string16 keys,
    bool release_modifiers) {
  std::list<KeyEvent> events;
  int sticky_modifiers = 0;
  Status status = ConvertKeysToKeyEvents(
      keys, release_modifiers, &sticky_modifiers, &events);
  if (status.IsError())
    return status;
  return web_view->DispatchKeyEvents(events);
}

Status SendKeysToElement(
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    const string16 keys) {
  bool is_displayed = false;
  Status status = IsElementDisplayed(
      session, web_view, element_id, true, &is_displayed);
  if (status.IsError())
    return status;
  if (!is_displayed)
    return Status(kElementNotVisible);
  bool is_enabled = false;
  status = IsElementEnabled(session, web_view, element_id, &is_enabled);
  if (status.IsError())
    return status;
  if (!is_enabled)
    return Status(kInvalidElementState);
  base::ListValue args;
  args.Append(CreateElement(element_id));
  scoped_ptr<base::Value> result;
  status = web_view->CallFunction(
      session->GetCurrentFrameId(), kFocusScript, args, &result);
  if (status.IsError())
    return status;
  return SendKeysOnWindow(web_view, keys, true);
}

}  // namespace

Status ExecuteElementCommand(
    const ElementCommand& command,
    Session* session,
    WebView* web_view,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  std::string id;
  if (!params.GetString("id", &id))
    return Status(kUnknownError, "'id' of element must be a string");
  return command.Run(session, web_view, id, params, value);
}

Status ExecuteFindChildElement(
    int interval_ms,
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  return FindElement(
      interval_ms, true, &element_id, session, web_view, params, value);
}

Status ExecuteFindChildElements(
    int interval_ms,
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  return FindElement(
      interval_ms, false, &element_id, session, web_view, params, value);
}

Status ExecuteHoverOverElement(
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  WebPoint location;
  Status status = GetElementClickableLocation(
      session, web_view, element_id, &location);
  if (status.IsError())
    return status;

  MouseEvent move_event(
      kMovedMouseEventType, kNoneMouseButton, location.x, location.y, 0);
  std::list<MouseEvent> events;
  events.push_back(move_event);
  status = web_view->DispatchMouseEvents(events);
  if (status.IsOk())
    session->mouse_position = location;
  return status;
}

Status ExecuteClickElement(
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  std::string tag_name;
  Status status = GetElementTagName(session, web_view, element_id, &tag_name);
  if (status.IsError())
    return status;
  if (tag_name == "option") {
    bool is_toggleable;
    status = IsOptionElementTogglable(
        session, web_view, element_id, &is_toggleable);
    if (status.IsError())
      return status;
    if (is_toggleable)
      return ToggleOptionElement(session, web_view, element_id);
    else
      return SetOptionElementSelected(session, web_view, element_id, true);
  } else {
    WebPoint location;
    status = GetElementClickableLocation(
        session, web_view, element_id, &location);
    if (status.IsError())
      return status;

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
    status = web_view->DispatchMouseEvents(events);
    if (status.IsOk())
      session->mouse_position = location;
    return status;
  }
}

Status ExecuteClearElement(
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  base::ListValue args;
  args.Append(CreateElement(element_id));
  scoped_ptr<base::Value> result;
  return web_view->CallFunction(
      session->GetCurrentFrameId(),
      webdriver::atoms::asString(webdriver::atoms::CLEAR),
      args, &result);
}

Status ExecuteSendKeysToElement(
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  const base::ListValue* keys_list;
  if (!params.GetList("value", &keys_list))
    return Status(kUnknownError, "'value' must be a list");

  bool is_input = false;
  Status status = IsElementAttributeEqualToIgnoreCase(
      session, web_view, element_id, "tagName", "input", &is_input);
  if (status.IsError())
    return status;
  bool is_file = false;
  status = IsElementAttributeEqualToIgnoreCase(
      session, web_view, element_id, "type", "file", &is_file);
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
    return SendKeysToElement(session, web_view, element_id, keys);
  }
}

Status ExecuteSubmitElement(
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  base::ListValue args;
  args.Append(CreateElement(element_id));
  return web_view->CallFunction(
      session->GetCurrentFrameId(),
      webdriver::atoms::asString(webdriver::atoms::SUBMIT),
      args,
      value);
}

Status ExecuteGetElementText(
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  base::ListValue args;
  args.Append(CreateElement(element_id));
  return web_view->CallFunction(
      session->GetCurrentFrameId(),
      webdriver::atoms::asString(webdriver::atoms::GET_TEXT),
      args,
      value);
}

Status ExecuteGetElementValue(
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  base::ListValue args;
  args.Append(CreateElement(element_id));
  return web_view->CallFunction(
      session->GetCurrentFrameId(),
      "function(elem) { return elem['value'] }",
      args,
      value);
}

Status ExecuteGetElementTagName(
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  base::ListValue args;
  args.Append(CreateElement(element_id));
  return web_view->CallFunction(
      session->GetCurrentFrameId(),
      "function(elem) { return elem.tagName.toLowerCase() }",
      args,
      value);
}

Status ExecuteIsElementSelected(
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  base::ListValue args;
  args.Append(CreateElement(element_id));
  return web_view->CallFunction(
      session->GetCurrentFrameId(),
      webdriver::atoms::asString(webdriver::atoms::IS_SELECTED),
      args,
      value);
}

Status ExecuteIsElementEnabled(
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  base::ListValue args;
  args.Append(CreateElement(element_id));
  return web_view->CallFunction(
      session->GetCurrentFrameId(),
      webdriver::atoms::asString(webdriver::atoms::IS_ENABLED),
      args,
      value);
}

Status ExecuteIsElementDisplayed(
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  base::ListValue args;
  args.Append(CreateElement(element_id));
  return web_view->CallFunction(
      session->GetCurrentFrameId(),
      webdriver::atoms::asString(webdriver::atoms::IS_DISPLAYED),
      args,
      value);
}

Status ExecuteGetElementLocation(
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  base::ListValue args;
  args.Append(CreateElement(element_id));
  return web_view->CallFunction(
      session->GetCurrentFrameId(),
      webdriver::atoms::asString(webdriver::atoms::GET_LOCATION),
      args,
      value);
}

Status ExecuteGetElementLocationOnceScrolledIntoView(
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  base::ListValue args;
  args.Append(CreateElement(element_id));
  return web_view->CallFunction(
      session->GetCurrentFrameId(),
      webdriver::atoms::asString(webdriver::atoms::GET_LOCATION_IN_VIEW),
      args,
      value);
}

Status ExecuteGetElementSize(
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  base::ListValue args;
  args.Append(CreateElement(element_id));
  return web_view->CallFunction(
      session->GetCurrentFrameId(),
      webdriver::atoms::asString(webdriver::atoms::GET_SIZE),
      args,
      value);
}

Status ExecuteGetElementAttribute(
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  std::string name;
  if (!params.GetString("name", &name))
    return Status(kUnknownError, "missing 'name'");
  return GetElementAttribute(session, web_view, element_id, name, value);
}

Status ExecuteGetElementValueOfCSSProperty(
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  base::ListValue args;
  args.Append(CreateElement(element_id));
  return web_view->CallFunction(
      session->GetCurrentFrameId(),
      webdriver::atoms::asString(webdriver::atoms::GET_EFFECTIVE_STYLE),
      args,
      value);
}

Status ExecuteElementEquals(
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  std::string other_element_id;
  if (!params.GetString("other", &other_element_id))
    return Status(kUnknownError, "'other' must be a string");
  value->reset(new base::FundamentalValue(element_id == other_element_id));
  return Status(kOk);
}
