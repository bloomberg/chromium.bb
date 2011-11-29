// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/automation/automation_json_requests.h"

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/format_macros.h"
#include "base/json/json_reader.h"
#include "base/json/json_value_serializer.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/stringprintf.h"
#include "base/test/test_timeouts.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/common/automation_messages.h"
#include "chrome/test/automation/automation_proxy.h"

using base::DictionaryValue;
using base::ListValue;

namespace {

bool SendAutomationJSONRequest(AutomationMessageSender* sender,
                               const DictionaryValue& request_dict,
                               DictionaryValue* reply_dict,
                               std::string* error_msg) {
  std::string request, reply;
  base::JSONWriter::Write(&request_dict, false, &request);
  std::string command;
  request_dict.GetString("command", &command);
  LOG(INFO) << "Sending '" << command << "' command.";

  base::Time before_sending = base::Time::Now();
  bool success = false;
  if (!SendAutomationJSONRequestWithDefaultTimeout(
          sender, request, &reply, &success)) {
    *error_msg = base::StringPrintf(
        "Chrome did not respond to '%s'. Elapsed time was %" PRId64 " ms. "
            "Request details: (%s).",
        command.c_str(),
        (base::Time::Now() - before_sending).InMilliseconds(),
        request.c_str());
    return false;
  }
  scoped_ptr<Value> value(base::JSONReader::Read(reply, true));
  if (!value.get() || !value->IsType(Value::TYPE_DICTIONARY)) {
    LOG(ERROR) << "JSON request did not return dict: " << command << "\n";
    return false;
  }
  DictionaryValue* dict = static_cast<DictionaryValue*>(value.get());
  if (!success) {
    std::string error;
    dict->GetString("error", &error);
    *error_msg = base::StringPrintf(
        "Internal Chrome error during '%s': (%s). Request details: (%s).",
        command.c_str(),
        error.c_str(),
        request.c_str());
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
                               int timeout_ms,
                               std::string* reply,
                               bool* success) {
  return sender->Send(new AutomationMsg_SendJSONRequest(
      -1, request, reply, success), timeout_ms);
}

bool SendAutomationJSONRequestWithDefaultTimeout(
    AutomationMessageSender* sender,
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
    int* tab_index,
    std::string* error_msg) {
  DictionaryValue request_dict;
  request_dict.SetString("command", "GetIndicesFromTab");
  request_dict.SetInteger("tab_id", tab_id);
  DictionaryValue reply_dict;
  if (!SendAutomationJSONRequest(sender, request_dict, &reply_dict, error_msg))
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
    int* tab_index,
    std::string* error_msg) {
  DictionaryValue request_dict;
  request_dict.SetString("command", "GetIndicesFromTab");
  request_dict.SetInteger("tab_handle", tab_handle);
  DictionaryValue reply_dict;
  if (!SendAutomationJSONRequest(sender, request_dict, &reply_dict, error_msg))
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
    const std::string& url,
    int navigation_count,
    AutomationMsg_NavigationResponseValues* nav_response,
    std::string* error_msg) {
  DictionaryValue dict;
  dict.SetString("command", "NavigateToURL");
  dict.SetInteger("windex", browser_index);
  dict.SetInteger("tab_index", tab_index);
  dict.SetString("url", url);
  dict.SetInteger("navigation_count", navigation_count);
  DictionaryValue reply_dict;
  if (!SendAutomationJSONRequest(sender, dict, &reply_dict, error_msg))
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
    Value** result,
    std::string* error_msg) {
  DictionaryValue dict;
  dict.SetString("command", "ExecuteJavascript");
  dict.SetInteger("windex", browser_index);
  dict.SetInteger("tab_index", tab_index);
  dict.SetString("frame_xpath", frame_xpath);
  dict.SetString("javascript", javascript);
  DictionaryValue reply_dict;
  if (!SendAutomationJSONRequest(sender, dict, &reply_dict, error_msg))
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
    int tab_index,
    std::string* error_msg) {
  DictionaryValue dict;
  dict.SetString("command", "GoForward");
  dict.SetInteger("windex", browser_index);
  dict.SetInteger("tab_index", tab_index);
  DictionaryValue reply_dict;
  return SendAutomationJSONRequest(sender, dict, &reply_dict, error_msg);
}

bool SendGoBackJSONRequest(
    AutomationMessageSender* sender,
    int browser_index,
    int tab_index,
    std::string* error_msg) {
  DictionaryValue dict;
  dict.SetString("command", "GoBack");
  dict.SetInteger("windex", browser_index);
  dict.SetInteger("tab_index", tab_index);
  DictionaryValue reply_dict;
  return SendAutomationJSONRequest(sender, dict, &reply_dict, error_msg);
}

bool SendReloadJSONRequest(
    AutomationMessageSender* sender,
    int browser_index,
    int tab_index,
    std::string* error_msg) {
  DictionaryValue dict;
  dict.SetString("command", "Reload");
  dict.SetInteger("windex", browser_index);
  dict.SetInteger("tab_index", tab_index);
  DictionaryValue reply_dict;
  return SendAutomationJSONRequest(sender, dict, &reply_dict, error_msg);
}

bool SendCaptureEntirePageJSONRequest(
    AutomationMessageSender* sender,
    int browser_index,
    int tab_index,
    const FilePath& path,
    std::string* error_msg) {
  DictionaryValue dict;
  dict.SetString("command", "CaptureEntirePage");
  dict.SetInteger("windex", browser_index);
  dict.SetInteger("tab_index", tab_index);
  dict.SetString("path", path.value());
  DictionaryValue reply_dict;

  return SendAutomationJSONRequest(sender, dict, &reply_dict, error_msg);
}

bool SendGetTabURLJSONRequest(
    AutomationMessageSender* sender,
    int browser_index,
    int tab_index,
    std::string* url,
    std::string* error_msg) {
  DictionaryValue dict;
  dict.SetString("command", "GetTabURL");
  dict.SetInteger("windex", browser_index);
  dict.SetInteger("tab_index", tab_index);
  DictionaryValue reply_dict;
  if (!SendAutomationJSONRequest(sender, dict, &reply_dict, error_msg))
    return false;
  return reply_dict.GetString("url", url);
}

bool SendGetTabTitleJSONRequest(
    AutomationMessageSender* sender,
    int browser_index,
    int tab_index,
    std::string* tab_title,
    std::string* error_msg) {
  DictionaryValue dict;
  dict.SetString("command", "GetTabTitle");
  dict.SetInteger("windex", browser_index);
  dict.SetInteger("tab_index", tab_index);
  DictionaryValue reply_dict;
  if (!SendAutomationJSONRequest(sender, dict, &reply_dict, error_msg))
    return false;
  return reply_dict.GetString("title", tab_title);
}

bool SendGetCookiesJSONRequest(
    AutomationMessageSender* sender,
    const std::string& url,
    ListValue** cookies,
    std::string* error_msg) {
  DictionaryValue dict;
  dict.SetString("command", "GetCookies");
  dict.SetString("url", url);
  DictionaryValue reply_dict;
  if (!SendAutomationJSONRequest(sender, dict, &reply_dict, error_msg))
    return false;
  Value* cookies_unscoped_value;
  if (!reply_dict.Remove("cookies", &cookies_unscoped_value))
    return false;
  scoped_ptr<Value> cookies_value(cookies_unscoped_value);
  if (!cookies_value->IsType(Value::TYPE_LIST))
    return false;
  *cookies = static_cast<ListValue*>(cookies_value.release());
  return true;
}

bool SendDeleteCookieJSONRequest(
    AutomationMessageSender* sender,
    const std::string& url,
    const std::string& cookie_name,
    std::string* error_msg) {
  DictionaryValue dict;
  dict.SetString("command", "DeleteCookie");
  dict.SetString("url", url);
  dict.SetString("name", cookie_name);
  DictionaryValue reply_dict;
  return SendAutomationJSONRequest(sender, dict, &reply_dict, error_msg);
}

bool SendSetCookieJSONRequest(
    AutomationMessageSender* sender,
    const std::string& url,
    DictionaryValue* cookie_dict,
    std::string* error_msg) {
  DictionaryValue dict;
  dict.SetString("command", "SetCookie");
  dict.SetString("url", url);
  dict.Set("cookie", cookie_dict->DeepCopy());
  DictionaryValue reply_dict;
  return SendAutomationJSONRequest(sender, dict, &reply_dict, error_msg);
}

bool SendGetTabIdsJSONRequest(
    AutomationMessageSender* sender,
    std::vector<int>* tab_ids,
    std::string* error_msg) {
  DictionaryValue dict;
  dict.SetString("command", "GetTabIds");
  DictionaryValue reply_dict;
  if (!SendAutomationJSONRequest(sender, dict, &reply_dict, error_msg))
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
    AutomationMessageSender* sender,
    int tab_id,
    bool* is_valid,
    std::string* error_msg) {
  DictionaryValue dict;
  dict.SetString("command", "IsTabIdValid");
  dict.SetInteger("id", tab_id);
  DictionaryValue reply_dict;
  if (!SendAutomationJSONRequest(sender, dict, &reply_dict, error_msg))
    return false;
  return reply_dict.GetBoolean("is_valid", is_valid);
}

bool SendCloseTabJSONRequest(
    AutomationMessageSender* sender,
    int browser_index,
    int tab_index,
    std::string* error_msg) {
  DictionaryValue dict;
  dict.SetString("command", "CloseTab");
  dict.SetInteger("windex", browser_index);
  dict.SetInteger("tab_index", tab_index);
  DictionaryValue reply_dict;
  return SendAutomationJSONRequest(sender, dict, &reply_dict, error_msg);
}

bool SendMouseMoveJSONRequest(
    AutomationMessageSender* sender,
    int browser_index,
    int tab_index,
    int x,
    int y,
    std::string* error_msg) {
  DictionaryValue dict;
  dict.SetString("command", "WebkitMouseMove");
  dict.SetInteger("windex", browser_index);
  dict.SetInteger("tab_index", tab_index);
  dict.SetInteger("x", x);
  dict.SetInteger("y", y);
  DictionaryValue reply_dict;
  return SendAutomationJSONRequest(sender, dict, &reply_dict, error_msg);
}

bool SendMouseClickJSONRequest(
    AutomationMessageSender* sender,
    int browser_index,
    int tab_index,
    automation::MouseButton button,
    int x,
    int y,
    std::string* error_msg) {
  DictionaryValue dict;
  dict.SetString("command", "WebkitMouseClick");
  dict.SetInteger("windex", browser_index);
  dict.SetInteger("tab_index", tab_index);
  dict.SetInteger("button", button);
  dict.SetInteger("x", x);
  dict.SetInteger("y", y);
  DictionaryValue reply_dict;
  return SendAutomationJSONRequest(sender, dict, &reply_dict, error_msg);
}

bool SendMouseDragJSONRequest(
    AutomationMessageSender* sender,
    int browser_index,
    int tab_index,
    int start_x,
    int start_y,
    int end_x,
    int end_y,
    std::string* error_msg) {
  DictionaryValue dict;
  dict.SetString("command", "WebkitMouseDrag");
  dict.SetInteger("windex", browser_index);
  dict.SetInteger("tab_index", tab_index);
  dict.SetInteger("start_x", start_x);
  dict.SetInteger("start_y", start_y);
  dict.SetInteger("end_x", end_x);
  dict.SetInteger("end_y", end_y);
  DictionaryValue reply_dict;
  return SendAutomationJSONRequest(sender, dict, &reply_dict, error_msg);
}

bool SendMouseButtonDownJSONRequest(
    AutomationMessageSender* sender,
    int browser_index,
    int tab_index,
    int x,
    int y,
    std::string* error_msg) {
  DictionaryValue dict;
  dict.SetString("command", "WebkitMouseButtonDown");
  dict.SetInteger("windex", browser_index);
  dict.SetInteger("tab_index", tab_index);
  dict.SetInteger("x", x);
  dict.SetInteger("y", y);
  DictionaryValue reply_dict;
  return SendAutomationJSONRequest(sender, dict, &reply_dict, error_msg);
}

bool SendMouseButtonUpJSONRequest(
    AutomationMessageSender* sender,
    int browser_index,
    int tab_index,
    int x,
    int y,
    std::string* error_msg)  {
  DictionaryValue dict;
  dict.SetString("command", "WebkitMouseButtonUp");
  dict.SetInteger("windex", browser_index);
  dict.SetInteger("tab_index", tab_index);
  dict.SetInteger("x", x);
  dict.SetInteger("y", y);
  DictionaryValue reply_dict;
  return SendAutomationJSONRequest(sender, dict, &reply_dict, error_msg);
}

bool SendMouseDoubleClickJSONRequest(
    AutomationMessageSender* sender,
    int browser_index,
    int tab_index,
    int x,
    int y,
    std::string* error_msg)  {
  DictionaryValue dict;
  dict.SetString("command", "WebkitMouseDoubleClick");
  dict.SetInteger("windex", browser_index);
  dict.SetInteger("tab_index", tab_index);
  dict.SetInteger("x", x);
  dict.SetInteger("y", y);
  DictionaryValue reply_dict;
  return SendAutomationJSONRequest(sender, dict, &reply_dict, error_msg);
}

bool SendWebKeyEventJSONRequest(
    AutomationMessageSender* sender,
    int browser_index,
    int tab_index,
    const WebKeyEvent& key_event,
    std::string* error_msg) {
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
  return SendAutomationJSONRequest(sender, dict, &reply_dict, error_msg);
}

bool SendNativeKeyEventJSONRequest(
    AutomationMessageSender* sender,
    int browser_index,
    int tab_index,
    ui::KeyboardCode key_code,
    int modifiers,
    std::string* error_msg) {
  DictionaryValue dict;
  dict.SetString("command", "SendOSLevelKeyEventToTab");
  dict.SetInteger("windex", browser_index);
  dict.SetInteger("tab_index", tab_index);
  dict.SetInteger("keyCode", key_code);
  dict.SetInteger("modifiers", modifiers);
  DictionaryValue reply_dict;
  return SendAutomationJSONRequest(sender, dict, &reply_dict, error_msg);
}

bool SendDragAndDropFilePathsJSONRequest(
    AutomationMessageSender* sender,
    int browser_index,
    int tab_index,
    int x,
    int y,
    const std::vector<FilePath::StringType>& paths,
    std::string* error_msg) {
  DictionaryValue dict;
  dict.SetString("command", "DragAndDropFilePaths");
  dict.SetInteger("windex", browser_index);
  dict.SetInteger("tab_index", tab_index);
  dict.SetInteger("x", x);
  dict.SetInteger("y", y);

  ListValue* list_value = new ListValue();
  for (size_t path_index = 0; path_index < paths.size(); ++path_index) {
    list_value->Append(Value::CreateStringValue(paths[path_index]));
  }
  dict.Set("paths", list_value);

  DictionaryValue reply_dict;
  return SendAutomationJSONRequest(sender, dict, &reply_dict, error_msg);
}

bool SendGetAppModalDialogMessageJSONRequest(
    AutomationMessageSender* sender,
    std::string* message,
    std::string* error_msg) {
  DictionaryValue dict;
  dict.SetString("command", "GetAppModalDialogMessage");
  DictionaryValue reply_dict;
  if (!SendAutomationJSONRequest(sender, dict, &reply_dict, error_msg))
    return false;
  return reply_dict.GetString("message", message);
}

bool SendAcceptOrDismissAppModalDialogJSONRequest(
    AutomationMessageSender* sender,
    bool accept,
    std::string* error_msg) {
  DictionaryValue dict;
  dict.SetString("command", "AcceptOrDismissAppModalDialog");
  dict.SetBoolean("accept", accept);
  DictionaryValue reply_dict;
  return SendAutomationJSONRequest(sender, dict, &reply_dict, error_msg);
}

bool SendAcceptPromptAppModalDialogJSONRequest(
    AutomationMessageSender* sender,
    const std::string& prompt_text,
    std::string* error_msg) {
  DictionaryValue dict;
  dict.SetString("command", "AcceptOrDismissAppModalDialog");
  dict.SetBoolean("accept", true);
  dict.SetString("prompt_text", prompt_text);
  DictionaryValue reply_dict;
  return SendAutomationJSONRequest(sender, dict, &reply_dict, error_msg);
}

bool SendWaitForAllTabsToStopLoadingJSONRequest(
    AutomationMessageSender* sender,
    std::string* error_msg) {
  DictionaryValue dict;
  dict.SetString("command", "WaitForAllTabsToStopLoading");
  DictionaryValue reply_dict;
  return SendAutomationJSONRequest(sender, dict, &reply_dict, error_msg);
}

bool SendGetChromeDriverAutomationVersion(
    AutomationMessageSender* sender,
    int* version,
    std::string* error_msg) {
  DictionaryValue dict;
  dict.SetString("command", "GetChromeDriverAutomationVersion");
  DictionaryValue reply_dict;
  if (!SendAutomationJSONRequest(sender, dict, &reply_dict, error_msg))
    return false;
  return reply_dict.GetInteger("version", version);
}

bool SendGetExtensionsInfoJSONRequest(
    AutomationMessageSender* sender,
    ListValue* extensions_list,
    std::string* error_msg) {
  DictionaryValue dict;
  dict.SetString("command", "GetExtensionsInfo");
  DictionaryValue reply_dict;
  if (!SendAutomationJSONRequest(sender, dict, &reply_dict, error_msg))
    return false;
  ListValue* extensions_list_swap;
  if (!reply_dict.GetList("extensions", &extensions_list_swap)) {
    *error_msg = "Missing or invalid 'extensions'";
    return false;
  }
  extensions_list->Swap(extensions_list_swap);
  return true;
}

bool SendInstallExtensionJSONRequest(
    AutomationMessageSender* sender,
    const FilePath& path,
    bool with_ui,
    std::string* extension_id,
    std::string* error_msg) {
  DictionaryValue dict;
  dict.SetString("command", "InstallExtension");
  dict.SetString("path", path.value());
  dict.SetBoolean("with_ui", with_ui);
  DictionaryValue reply_dict;
  if (!SendAutomationJSONRequest(sender, dict, &reply_dict, error_msg))
    return false;
  if (!reply_dict.GetString("id", extension_id)) {
    *error_msg = "Missing or invalid 'id'";
    return false;
  }
  return true;
}
