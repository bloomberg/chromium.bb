// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/automation/automation_json_requests.h"

#include "base/scoped_ptr.h"
#include "base/values.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "chrome/common/automation_messages.h"
#include "chrome/common/json_value_serializer.h"
#include "chrome/test/automation/automation_proxy.h"

namespace {

bool SendAutomationJSONRequest(AutomationMessageSender* sender,
                               const DictionaryValue& request_dict,
                               DictionaryValue* reply_dict) {
  std::string request, reply;
  base::JSONWriter::Write(&request_dict, false, &request);
  bool success = false;
  if (!SendAutomationJSONRequest(sender, request, &reply, &success))
    return false;
  scoped_ptr<Value> value(base::JSONReader::Read(reply, true));
  if (!value.get() || !value->IsType(Value::TYPE_DICTIONARY)) {
    std::string command;
    request_dict.GetString("command", &command);
    LOG(ERROR) << "JSON request did not return dict: " << command << "\n";
    return false;
  }
  DictionaryValue* dict = static_cast<DictionaryValue*>(value.get());
  if (!success) {
    std::string command, error;
    request_dict.GetString("command", &command);
    dict->GetString("error", &error);
    LOG(ERROR) << "JSON request failed: " << command << "\n"
               << "    with error: " << error;
    return false;
  }
  reply_dict->MergeDictionary(dict);
  return true;
}

}  // namespace

WebKeyEvent::WebKeyEvent(automation::KeyEventTypes type,
                         ui::KeyboardCode key_code,
                         const std::string& unmodified_text,
                         const std::string& modified_text,
                         int modifiers)
    : type(type),
      key_code(key_code),
      unmodified_text(unmodified_text),
      modified_text(modified_text),
      modifiers(modifiers) {}

bool SendAutomationJSONRequest(AutomationMessageSender* sender,
                               const std::string& request,
                               std::string* reply,
                               bool* success) {
  return sender->Send(new AutomationMsg_SendJSONRequest(
      -1, request, reply, success));
}

bool SendGetIndicesFromTabIdJSONRequest(
    AutomationMessageSender* sender,
    int tab_id,
    int* browser_index,
    int* tab_index) {
  DictionaryValue request_dict;
  request_dict.SetString("command", "GetIndicesFromTab");
  request_dict.SetInteger("tab_id", tab_id);
  DictionaryValue reply_dict;
  if (!SendAutomationJSONRequest(sender, request_dict, &reply_dict))
    return false;
  if (!reply_dict.GetInteger("windex", browser_index))
    return false;
  if (!reply_dict.GetInteger("tab_index", tab_index))
    return false;
  return true;
}

bool SendGetIndicesFromTabHandleJSONRequest(
    AutomationMessageSender* sender,
    int tab_handle,
    int* browser_index,
    int* tab_index) {
  DictionaryValue request_dict;
  request_dict.SetString("command", "GetIndicesFromTab");
  request_dict.SetInteger("tab_handle", tab_handle);
  DictionaryValue reply_dict;
  if (!SendAutomationJSONRequest(sender, request_dict, &reply_dict))
    return false;
  if (!reply_dict.GetInteger("windex", browser_index))
    return false;
  if (!reply_dict.GetInteger("tab_index", tab_index))
    return false;
  return true;
}

bool SendNavigateToURLJSONRequest(
    AutomationMessageSender* sender,
    int browser_index,
    int tab_index,
    const GURL& url,
    int navigation_count,
    AutomationMsg_NavigationResponseValues* nav_response) {
  DictionaryValue dict;
  dict.SetString("command", "NavigateToURL");
  dict.SetInteger("windex", browser_index);
  dict.SetInteger("tab_index", tab_index);
  dict.SetString("url", url.possibly_invalid_spec());
  dict.SetInteger("navigation_count", navigation_count);
  DictionaryValue reply_dict;
  if (!SendAutomationJSONRequest(sender, dict, &reply_dict))
    return false;
  int response = 0;
  if (!reply_dict.GetInteger("result", &response))
    return false;
  *nav_response = static_cast<AutomationMsg_NavigationResponseValues>(response);
  return true;
}

bool SendExecuteJavascriptJSONRequest(
    AutomationMessageSender* sender,
    int browser_index,
    int tab_index,
    const std::string& frame_xpath,
    const std::string& javascript,
    Value** result) {
  DictionaryValue dict;
  dict.SetString("command", "ExecuteJavascript");
  dict.SetInteger("windex", browser_index);
  dict.SetInteger("tab_index", tab_index);
  dict.SetString("frame_xpath", frame_xpath);
  dict.SetString("javascript", javascript);
  DictionaryValue reply_dict;
  if (!SendAutomationJSONRequest(sender, dict, &reply_dict))
    return false;

  std::string json;
  if (!reply_dict.GetString("result", &json)) {
    LOG(ERROR) << "Executed javascript but received no 'result'";
    return false;
  }
  // Wrap |json| in an array before deserializing because valid JSON has an
  // array or an object as the root.
  json.insert(0, "[");
  json.append("]");

  JSONStringValueSerializer deserializer(json);
  Value* value = deserializer.Deserialize(NULL, NULL);
  if (!value || !value->IsType(Value::TYPE_LIST)) {
    LOG(ERROR) << "Unable to deserialize returned JSON";
    return false;
  }
  scoped_ptr<ListValue> list(static_cast<ListValue*>(value));
  return list->Remove(0, result);
}

bool SendGoForwardJSONRequest(
    AutomationMessageSender* sender,
    int browser_index,
    int tab_index) {
  DictionaryValue dict;
  dict.SetString("command", "GoForward");
  dict.SetInteger("windex", browser_index);
  dict.SetInteger("tab_index", tab_index);
  DictionaryValue reply_dict;
  return SendAutomationJSONRequest(sender, dict, &reply_dict);
}

bool SendGoBackJSONRequest(
    AutomationMessageSender* sender,
    int browser_index,
    int tab_index) {
  DictionaryValue dict;
  dict.SetString("command", "GoBack");
  dict.SetInteger("windex", browser_index);
  dict.SetInteger("tab_index", tab_index);
  DictionaryValue reply_dict;
  return SendAutomationJSONRequest(sender, dict, &reply_dict);
}

bool SendReloadJSONRequest(
    AutomationMessageSender* sender,
    int browser_index,
    int tab_index) {
  DictionaryValue dict;
  dict.SetString("command", "Reload");
  dict.SetInteger("windex", browser_index);
  dict.SetInteger("tab_index", tab_index);
  DictionaryValue reply_dict;
  return SendAutomationJSONRequest(sender, dict, &reply_dict);
}

bool SendGetTabURLJSONRequest(
    AutomationMessageSender* sender,
    int browser_index,
    int tab_index,
    std::string* url) {
  DictionaryValue dict;
  dict.SetString("command", "GetTabURL");
  dict.SetInteger("windex", browser_index);
  dict.SetInteger("tab_index", tab_index);
  DictionaryValue reply_dict;
  if (!SendAutomationJSONRequest(sender, dict, &reply_dict))
    return false;
  return reply_dict.GetString("url", url);
}

bool SendGetTabTitleJSONRequest(
    AutomationMessageSender* sender,
    int browser_index,
    int tab_index,
    std::string* tab_title) {
  DictionaryValue dict;
  dict.SetString("command", "GetTabTitle");
  dict.SetInteger("windex", browser_index);
  dict.SetInteger("tab_index", tab_index);
  DictionaryValue reply_dict;
  if (!SendAutomationJSONRequest(sender, dict, &reply_dict))
    return false;
  return reply_dict.GetString("title", tab_title);
}

bool SendGetCookiesJSONRequest(
    AutomationMessageSender* sender,
    int browser_index,
    const std::string& url,
    std::string* cookies) {
  DictionaryValue dict;
  dict.SetString("command", "GetCookies");
  dict.SetInteger("windex", browser_index);
  dict.SetString("url", url);
  DictionaryValue reply_dict;
  if (!SendAutomationJSONRequest(sender, dict, &reply_dict))
    return false;
  return reply_dict.GetString("cookies", cookies);
}

bool SendDeleteCookieJSONRequest(
    AutomationMessageSender* sender,
    int browser_index,
    const std::string& url,
    const std::string& cookie_name) {
  DictionaryValue dict;
  dict.SetString("command", "DeleteCookie");
  dict.SetInteger("windex", browser_index);
  dict.SetString("url", url);
  dict.SetString("name", cookie_name);
  DictionaryValue reply_dict;
  return SendAutomationJSONRequest(sender, dict, &reply_dict);
}

bool SendSetCookieJSONRequest(
    AutomationMessageSender* sender,
    int browser_index,
    const std::string& url,
    const std::string& cookie) {
  DictionaryValue dict;
  dict.SetString("command", "SetCookie");
  dict.SetInteger("windex", browser_index);
  dict.SetString("url", url);
  dict.SetString("cookie", cookie);
  DictionaryValue reply_dict;
  return SendAutomationJSONRequest(sender, dict, &reply_dict);
}

bool SendGetTabIdsJSONRequest(
    AutomationMessageSender* sender, std::vector<int>* tab_ids) {
  DictionaryValue dict;
  dict.SetString("command", "GetTabIds");
  DictionaryValue reply_dict;
  if (!SendAutomationJSONRequest(sender, dict, &reply_dict))
    return false;
  ListValue* id_list;
  if (!reply_dict.GetList("ids", &id_list)) {
    LOG(ERROR) << "Returned 'ids' key is missing or invalid";
    return false;
  }
  std::vector<int> temp_ids;
  for (size_t i = 0; i < id_list->GetSize(); ++i) {
    int id;
    if (!id_list->GetInteger(i, &id)) {
      LOG(ERROR) << "Returned 'ids' key contains non-integer values";
      return false;
    }
    temp_ids.push_back(id);
  }
  *tab_ids = temp_ids;
  return true;
}

bool SendIsTabIdValidJSONRequest(
    AutomationMessageSender* sender, int tab_id, bool* is_valid) {
  DictionaryValue dict;
  dict.SetString("command", "IsTabIdValid");
  dict.SetInteger("id", tab_id);
  DictionaryValue reply_dict;
  if (!SendAutomationJSONRequest(sender, dict, &reply_dict))
    return false;
  return reply_dict.GetBoolean("is_valid", is_valid);
}

bool SendCloseTabJSONRequest(
    AutomationMessageSender* sender, int browser_index, int tab_index) {
  DictionaryValue dict;
  dict.SetString("command", "CloseTab");
  dict.SetInteger("windex", browser_index);
  dict.SetInteger("tab_index", tab_index);
  DictionaryValue reply_dict;
  return SendAutomationJSONRequest(sender, dict, &reply_dict);
}

bool SendMouseMoveJSONRequest(
    AutomationMessageSender* sender,
    int browser_index,
    int tab_index,
    int x,
    int y) {
  DictionaryValue dict;
  dict.SetString("command", "WebkitMouseMove");
  dict.SetInteger("windex", browser_index);
  dict.SetInteger("tab_index", tab_index);
  dict.SetInteger("x", x);
  dict.SetInteger("y", y);
  DictionaryValue reply_dict;
  return SendAutomationJSONRequest(sender, dict, &reply_dict);
}

bool SendMouseClickJSONRequest(
    AutomationMessageSender* sender,
    int browser_index,
    int tab_index,
    automation::MouseButton button,
    int x,
    int y) {
  // TODO get rid of the evil flags.
  DictionaryValue dict;
  dict.SetString("command", "WebkitMouseClick");
  dict.SetInteger("windex", browser_index);
  dict.SetInteger("tab_index", tab_index);
  dict.SetInteger("button", button);
  dict.SetInteger("x", x);
  dict.SetInteger("y", y);
  DictionaryValue reply_dict;
  return SendAutomationJSONRequest(sender, dict, &reply_dict);
}

bool SendMouseDragJSONRequest(
    AutomationMessageSender* sender,
    int browser_index,
    int tab_index,
    int start_x,
    int start_y,
    int end_x,
    int end_y) {
  DictionaryValue dict;
  dict.SetString("command", "WebkitMouseDrag");
  dict.SetInteger("windex", browser_index);
  dict.SetInteger("tab_index", tab_index);
  dict.SetInteger("start_x", start_x);
  dict.SetInteger("start_y", start_y);
  dict.SetInteger("end_x", end_x);
  dict.SetInteger("end_y", end_y);
  DictionaryValue reply_dict;
  return SendAutomationJSONRequest(sender, dict, &reply_dict);
}

bool SendWebKeyEventJSONRequest(
    AutomationMessageSender* sender,
    int browser_index,
    int tab_index,
    const WebKeyEvent& key_event) {
  DictionaryValue dict;
  dict.SetString("command", "SendWebkitKeyEvent");
  dict.SetInteger("windex", browser_index);
  dict.SetInteger("tab_index", tab_index);
  dict.SetInteger("type", key_event.type);
  dict.SetInteger("nativeKeyCode", key_event.key_code);
  dict.SetInteger("windowsKeyCode", key_event.key_code);
  dict.SetString("unmodifiedText", key_event.unmodified_text);
  dict.SetString("text", key_event.modified_text);
  dict.SetInteger("modifiers", key_event.modifiers);
  dict.SetBoolean("isSystemKey", false);
  DictionaryValue reply_dict;
  return SendAutomationJSONRequest(sender, dict, &reply_dict);
}

bool SendWaitForAllTabsToStopLoadingJSONRequest(
    AutomationMessageSender* sender) {
  DictionaryValue dict;
  dict.SetString("command", "WaitForAllTabsToStopLoading");
  DictionaryValue reply_dict;
  return SendAutomationJSONRequest(sender, dict, &reply_dict);
}
