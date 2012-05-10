// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/automation/automation_json_requests.h"

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/format_macros.h"
#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/stringprintf.h"
#include "base/test/test_timeouts.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/common/automation_messages.h"
#include "chrome/test/automation/automation_proxy.h"

using automation::Error;
using automation::ErrorCode;
using base::DictionaryValue;
using base::ListValue;

namespace {

bool SendAutomationJSONRequest(AutomationMessageSender* sender,
                               const DictionaryValue& request_dict,
                               DictionaryValue* reply_dict,
                               Error* error) {
  std::string request, reply;
  base::JSONWriter::Write(&request_dict, &request);
  std::string command;
  request_dict.GetString("command", &command);
  LOG(INFO) << "Sending '" << command << "' command.";

  base::Time before_sending = base::Time::Now();
  bool success = false;
  if (!SendAutomationJSONRequestWithDefaultTimeout(
          sender, request, &reply, &success)) {
    *error = Error(base::StringPrintf(
        "Chrome did not respond to '%s'. Elapsed time was %" PRId64 " ms.",
        command.c_str(),
        (base::Time::Now() - before_sending).InMilliseconds()));
    LOG(INFO) << error->message();
    return false;
  }
  scoped_ptr<Value> value(
      base::JSONReader::Read(reply, base::JSON_ALLOW_TRAILING_COMMAS));
  if (!value.get() || !value->IsType(Value::TYPE_DICTIONARY)) {
    *error = Error("JSON request did not return a dictionary");
    LOG(ERROR) << "JSON request did not return dict: " << command << "\n";
    return false;
  }
  DictionaryValue* dict = static_cast<DictionaryValue*>(value.get());
  if (!success) {
    std::string error_msg;
    dict->GetString("error", &error_msg);
    int int_code = automation::kUnknownError;
    dict->GetInteger("code", &int_code);
    ErrorCode code = static_cast<ErrorCode>(int_code);
    *error = Error(code, error_msg);
    LOG(INFO) << "JSON request failed: " << command << "\n"
              << "    with error: " << error_msg;
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

WebMouseEvent::WebMouseEvent(automation::MouseEventType type,
                             automation::MouseButton button,
                             int x,
                             int y,
                             int click_count,
                             int modifiers)
    : type(type),
      button(button),
      x(x),
      y(y),
      click_count(click_count),
      modifiers(modifiers) {}

// static
WebViewId WebViewId::ForView(const AutomationId& view_id) {
  WebViewId id;
  id.old_style_ = false;
  id.id_ = view_id;
  return id;
}

// static
WebViewId WebViewId::ForOldStyleTab(int tab_id) {
  WebViewId id;
  id.old_style_ = true;
  id.tab_id_ = tab_id;
  return id;
}

WebViewId::WebViewId() : old_style_(true) {}

void WebViewId::UpdateDictionary(DictionaryValue* dict,
                                 const std::string& view_id_key) const {
  if (old_style_) {
    dict->SetInteger("id", tab_id_);
  } else {
    dict->Set(view_id_key, id_.ToValue());
  }
}

bool WebViewId::IsValid() const {
  if (old_style_)
    return tab_id_ != 0;
  else
    return id_.is_valid();
}

AutomationId WebViewId::GetId() const {
  if (old_style_)
    return AutomationId(AutomationId::kTypeTab, base::IntToString(tab_id_));
  else
    return id_;
}

bool WebViewId::IsTab() const {
  return old_style_ || id_.type() == AutomationId::kTypeTab;
}

int WebViewId::tab_id() const {
  return tab_id_;
}

bool WebViewId::old_style() const {
  return old_style_;
}

// static
WebViewLocator WebViewLocator::ForIndexPair(int browser_index, int tab_index) {
  WebViewLocator locator;
  locator.type_ = kTypeIndexPair;
  locator.locator_.index_pair.browser_index = browser_index;
  locator.locator_.index_pair.tab_index = tab_index;
  return locator;
}

// static
WebViewLocator WebViewLocator::ForViewId(const AutomationId& view_id) {
  WebViewLocator locator;
  locator.type_ = kTypeViewId;
  locator.locator_.view_id = view_id;
  return locator;
}

WebViewLocator::WebViewLocator() {}

WebViewLocator::~WebViewLocator() {}

void WebViewLocator::UpdateDictionary(
    DictionaryValue* dict, const std::string& view_id_key) const {
  if (type_ == kTypeIndexPair) {
    dict->SetInteger("windex", locator_.index_pair.browser_index);
    dict->SetInteger("tab_index", locator_.index_pair.tab_index);
  } else if (type_ == kTypeViewId) {
    dict->Set(view_id_key, locator_.view_id.ToValue());
  }
}

int WebViewLocator::browser_index() const {
  return locator_.index_pair.browser_index;
}

int WebViewLocator::tab_index() const {
  return locator_.index_pair.tab_index;
}

WebViewLocator::Locator::Locator() {}

WebViewLocator::Locator::~Locator() {}

WebViewInfo::WebViewInfo(const WebViewId& view_id,
                         const std::string& extension_id)
    : view_id(view_id),
      extension_id(extension_id) {}

WebViewInfo::~WebViewInfo() {}

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
    Error* error) {
  DictionaryValue request_dict;
  request_dict.SetString("command", "GetIndicesFromTab");
  request_dict.SetInteger("tab_id", tab_id);
  DictionaryValue reply_dict;
  if (!SendAutomationJSONRequest(sender, request_dict, &reply_dict, error))
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
    Error* error) {
  DictionaryValue request_dict;
  request_dict.SetString("command", "GetIndicesFromTab");
  request_dict.SetInteger("tab_handle", tab_handle);
  DictionaryValue reply_dict;
  if (!SendAutomationJSONRequest(sender, request_dict, &reply_dict, error))
    return false;
  if (!reply_dict.GetInteger("windex", browser_index))
    return false;
  if (!reply_dict.GetInteger("tab_index", tab_index))
    return false;
  return true;
}

bool SendNavigateToURLJSONRequest(
    AutomationMessageSender* sender,
    const WebViewLocator& locator,
    const std::string& url,
    int navigation_count,
    AutomationMsg_NavigationResponseValues* nav_response,
    Error* error) {
  DictionaryValue dict;
  dict.SetString("command", "NavigateToURL");
  locator.UpdateDictionary(&dict, "auto_id");
  dict.SetString("url", url);
  dict.SetInteger("navigation_count", navigation_count);
  DictionaryValue reply_dict;
  if (!SendAutomationJSONRequest(sender, dict, &reply_dict, error))
    return false;
  int response = 0;
  if (!reply_dict.GetInteger("result", &response))
    return false;
  *nav_response = static_cast<AutomationMsg_NavigationResponseValues>(response);
  return true;
}

bool SendExecuteJavascriptJSONRequest(
    AutomationMessageSender* sender,
    const WebViewLocator& locator,
    const std::string& frame_xpath,
    const std::string& javascript,
    Value** result,
    Error* error) {
  DictionaryValue dict;
  dict.SetString("command", "ExecuteJavascript");
  locator.UpdateDictionary(&dict, "auto_id");
  dict.SetString("frame_xpath", frame_xpath);
  dict.SetString("javascript", javascript);
  DictionaryValue reply_dict;
  if (!SendAutomationJSONRequest(sender, dict, &reply_dict, error))
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
    const WebViewLocator& locator,
    Error* error) {
  DictionaryValue dict;
  dict.SetString("command", "GoForward");
  locator.UpdateDictionary(&dict, "auto_id");
  DictionaryValue reply_dict;
  return SendAutomationJSONRequest(sender, dict, &reply_dict, error);
}

bool SendGoBackJSONRequest(
    AutomationMessageSender* sender,
    const WebViewLocator& locator,
    Error* error) {
  DictionaryValue dict;
  dict.SetString("command", "GoBack");
  locator.UpdateDictionary(&dict, "auto_id");
  DictionaryValue reply_dict;
  return SendAutomationJSONRequest(sender, dict, &reply_dict, error);
}

bool SendReloadJSONRequest(
    AutomationMessageSender* sender,
    const WebViewLocator& locator,
    Error* error) {
  DictionaryValue dict;
  dict.SetString("command", "Reload");
  locator.UpdateDictionary(&dict, "auto_id");
  DictionaryValue reply_dict;
  return SendAutomationJSONRequest(sender, dict, &reply_dict, error);
}

bool SendCaptureEntirePageJSONRequest(
    AutomationMessageSender* sender,
    const WebViewLocator& locator,
    const FilePath& path,
    Error* error) {
  DictionaryValue dict;
  dict.SetString("command", "CaptureEntirePage");
  locator.UpdateDictionary(&dict, "auto_id");
  dict.SetString("path", path.value());
  DictionaryValue reply_dict;

  return SendAutomationJSONRequest(sender, dict, &reply_dict, error);
}

#if !defined(NO_TCMALLOC) && (defined(OS_LINUX) || defined(OS_CHROMEOS))
bool SendHeapProfilerDumpJSONRequest(
    AutomationMessageSender* sender,
    const WebViewLocator& locator,
    const std::string& reason,
    Error* error) {
  DictionaryValue dict;
  dict.SetString("command", "HeapProfilerDump");
  dict.SetString("process_type", "renderer");
  dict.SetString("reason", reason);
  locator.UpdateDictionary(&dict, "auto_id");
  DictionaryValue reply_dict;

  return SendAutomationJSONRequest(sender, dict, &reply_dict, error);
}
#endif  // !defined(NO_TCMALLOC) && (defined(OS_LINUX) || defined(OS_CHROMEOS))

bool SendGetCookiesJSONRequest(
    AutomationMessageSender* sender,
    const std::string& url,
    ListValue** cookies,
    Error* error) {
  DictionaryValue dict;
  dict.SetString("command", "GetCookies");
  dict.SetString("url", url);
  DictionaryValue reply_dict;
  if (!SendAutomationJSONRequest(sender, dict, &reply_dict, error))
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
    Error* error) {
  DictionaryValue dict;
  dict.SetString("command", "DeleteCookie");
  dict.SetString("url", url);
  dict.SetString("name", cookie_name);
  DictionaryValue reply_dict;
  return SendAutomationJSONRequest(sender, dict, &reply_dict, error);
}

bool SendSetCookieJSONRequest(
    AutomationMessageSender* sender,
    const std::string& url,
    DictionaryValue* cookie_dict,
    Error* error) {
  DictionaryValue dict;
  dict.SetString("command", "SetCookie");
  dict.SetString("url", url);
  dict.Set("cookie", cookie_dict->DeepCopy());
  DictionaryValue reply_dict;
  return SendAutomationJSONRequest(sender, dict, &reply_dict, error);
}

bool SendGetTabIdsJSONRequest(
    AutomationMessageSender* sender,
    std::vector<WebViewInfo>* views,
    Error* error) {
  DictionaryValue dict;
  dict.SetString("command", "GetTabIds");
  DictionaryValue reply_dict;
  if (!SendAutomationJSONRequest(sender, dict, &reply_dict, error))
    return false;
  ListValue* id_list;
  if (reply_dict.GetList("ids", &id_list)) {
    for (size_t i = 0; i < id_list->GetSize(); ++i) {
      int id;
      if (!id_list->GetInteger(i, &id)) {
        *error = Error("Returned id in 'tab_ids' is not an integer");
        return false;
      }
      views->push_back(WebViewInfo(
          WebViewId::ForOldStyleTab(id), std::string()));
    }
  }
  return true;
}

bool SendGetWebViewsJSONRequest(
    AutomationMessageSender* sender,
    std::vector<WebViewInfo>* views,
    Error* error) {
  DictionaryValue dict;
  dict.SetString("command", "GetViews");
  DictionaryValue reply_dict;
  if (!SendAutomationJSONRequest(sender, dict, &reply_dict, error))
    return false;
  ListValue* views_list;
  if (!reply_dict.GetList("views", &views_list)) {
    *error = Error("Returned 'views' key is missing or invalid");
    return false;
  }
  for (size_t i = 0; i < views_list->GetSize(); ++i) {
    DictionaryValue* view_dict;
    if (!views_list->GetDictionary(i, &view_dict)) {
      *error = Error("Returned 'views' key contains non-dictionary values");
      return false;
    }
    AutomationId view_id;
    std::string error_msg;
    if (!AutomationId::FromValueInDictionary(
            view_dict, "auto_id", &view_id, &error_msg)) {
      *error = Error(error_msg);
      return false;
    }
    std::string extension_id;
    view_dict->GetString("extension_id", &extension_id);
    views->push_back(WebViewInfo(
        WebViewId::ForView(view_id), extension_id));
  }
  return true;
}

bool SendIsTabIdValidJSONRequest(
    AutomationMessageSender* sender,
    const WebViewId& view_id,
    bool* is_valid,
    Error* error) {
  DictionaryValue dict;
  dict.SetString("command", "IsTabIdValid");
  view_id.UpdateDictionary(&dict, "tab_id");
  DictionaryValue reply_dict;
  if (!SendAutomationJSONRequest(sender, dict, &reply_dict, error))
    return false;
  return reply_dict.GetBoolean("is_valid", is_valid);
}

bool SendDoesAutomationObjectExistJSONRequest(
    AutomationMessageSender* sender,
    const WebViewId& view_id,
    bool* does_exist,
    Error* error) {
  DictionaryValue dict;
  dict.SetString("command", "DoesAutomationObjectExist");
  view_id.UpdateDictionary(&dict, "auto_id");
  DictionaryValue reply_dict;
  if (!SendAutomationJSONRequest(sender, dict, &reply_dict, error))
    return false;
  return reply_dict.GetBoolean("does_exist", does_exist);
}

bool SendCloseViewJSONRequest(
    AutomationMessageSender* sender,
    const WebViewLocator& locator,
    Error* error) {
  DictionaryValue dict;
  dict.SetString("command", "CloseTab");
  locator.UpdateDictionary(&dict, "auto_id");
  DictionaryValue reply_dict;
  return SendAutomationJSONRequest(sender, dict, &reply_dict, error);
}

bool SendMouseMoveJSONRequestDeprecated(
    AutomationMessageSender* sender,
    const WebViewLocator& locator,
    int x,
    int y,
    Error* error) {
  DictionaryValue dict;
  dict.SetString("command", "WebkitMouseMove");
  locator.UpdateDictionary(&dict, "auto_id");
  dict.SetInteger("x", x);
  dict.SetInteger("y", y);
  DictionaryValue reply_dict;
  return SendAutomationJSONRequest(sender, dict, &reply_dict, error);
}

bool SendMouseClickJSONRequestDeprecated(
    AutomationMessageSender* sender,
    const WebViewLocator& locator,
    automation::MouseButton button,
    int x,
    int y,
    Error* error) {
  DictionaryValue dict;
  dict.SetString("command", "WebkitMouseClick");
  locator.UpdateDictionary(&dict, "auto_id");
  dict.SetInteger("button", button);
  dict.SetInteger("x", x);
  dict.SetInteger("y", y);
  DictionaryValue reply_dict;
  return SendAutomationJSONRequest(sender, dict, &reply_dict, error);
}

bool SendMouseDragJSONRequestDeprecated(
    AutomationMessageSender* sender,
    const WebViewLocator& locator,
    int start_x,
    int start_y,
    int end_x,
    int end_y,
    Error* error) {
  DictionaryValue dict;
  dict.SetString("command", "WebkitMouseDrag");
  locator.UpdateDictionary(&dict, "auto_id");
  dict.SetInteger("start_x", start_x);
  dict.SetInteger("start_y", start_y);
  dict.SetInteger("end_x", end_x);
  dict.SetInteger("end_y", end_y);
  DictionaryValue reply_dict;
  return SendAutomationJSONRequest(sender, dict, &reply_dict, error);
}

bool SendMouseButtonDownJSONRequestDeprecated(
    AutomationMessageSender* sender,
    const WebViewLocator& locator,
    int x,
    int y,
    Error* error) {
  DictionaryValue dict;
  dict.SetString("command", "WebkitMouseButtonDown");
  locator.UpdateDictionary(&dict, "auto_id");
  dict.SetInteger("x", x);
  dict.SetInteger("y", y);
  DictionaryValue reply_dict;
  return SendAutomationJSONRequest(sender, dict, &reply_dict, error);
}

bool SendMouseButtonUpJSONRequestDeprecated(
    AutomationMessageSender* sender,
    const WebViewLocator& locator,
    int x,
    int y,
    Error* error)  {
  DictionaryValue dict;
  dict.SetString("command", "WebkitMouseButtonUp");
  locator.UpdateDictionary(&dict, "auto_id");
  dict.SetInteger("x", x);
  dict.SetInteger("y", y);
  DictionaryValue reply_dict;
  return SendAutomationJSONRequest(sender, dict, &reply_dict, error);
}

bool SendMouseDoubleClickJSONRequestDeprecated(
    AutomationMessageSender* sender,
    const WebViewLocator& locator,
    int x,
    int y,
    Error* error)  {
  DictionaryValue dict;
  dict.SetString("command", "WebkitMouseDoubleClick");
  locator.UpdateDictionary(&dict, "auto_id");
  dict.SetInteger("x", x);
  dict.SetInteger("y", y);
  DictionaryValue reply_dict;
  return SendAutomationJSONRequest(sender, dict, &reply_dict, error);
}

bool SendWebKeyEventJSONRequest(
    AutomationMessageSender* sender,
    const WebViewLocator& locator,
    const WebKeyEvent& key_event,
    Error* error) {
  DictionaryValue dict;
  dict.SetString("command", "SendWebkitKeyEvent");
  locator.UpdateDictionary(&dict, "auto_id");
  dict.SetInteger("type", key_event.type);
  dict.SetInteger("nativeKeyCode", key_event.key_code);
  dict.SetInteger("windowsKeyCode", key_event.key_code);
  dict.SetString("unmodifiedText", key_event.unmodified_text);
  dict.SetString("text", key_event.modified_text);
  dict.SetInteger("modifiers", key_event.modifiers);
  dict.SetBoolean("isSystemKey", false);
  DictionaryValue reply_dict;
  return SendAutomationJSONRequest(sender, dict, &reply_dict, error);
}

bool SendNativeKeyEventJSONRequest(
    AutomationMessageSender* sender,
    const WebViewLocator& locator,
    ui::KeyboardCode key_code,
    int modifiers,
    Error* error) {
  DictionaryValue dict;
  dict.SetString("command", "SendOSLevelKeyEventToTab");
  locator.UpdateDictionary(&dict, "auto_id");
  dict.SetInteger("keyCode", key_code);
  dict.SetInteger("modifiers", modifiers);
  DictionaryValue reply_dict;
  return SendAutomationJSONRequest(sender, dict, &reply_dict, error);
}

bool SendWebMouseEventJSONRequest(
    AutomationMessageSender* sender,
    const WebViewLocator& locator,
    const WebMouseEvent& mouse_event,
    automation::Error* error) {
  DictionaryValue dict;
  dict.SetString("command", "ProcessWebMouseEvent");
  locator.UpdateDictionary(&dict, "auto_id");
  dict.SetInteger("type", mouse_event.type);
  dict.SetInteger("button", mouse_event.button);
  dict.SetInteger("x", mouse_event.x);
  dict.SetInteger("y", mouse_event.y);
  dict.SetInteger("click_count", mouse_event.click_count);
  dict.SetInteger("modifiers", mouse_event.modifiers);
  DictionaryValue reply_dict;
  return SendAutomationJSONRequest(sender, dict, &reply_dict, error);
}

bool SendDragAndDropFilePathsJSONRequest(
    AutomationMessageSender* sender,
    const WebViewLocator& locator,
    int x,
    int y,
    const std::vector<FilePath::StringType>& paths,
    Error* error) {
  DictionaryValue dict;
  dict.SetString("command", "DragAndDropFilePaths");
  locator.UpdateDictionary(&dict, "auto_id");
  dict.SetInteger("x", x);
  dict.SetInteger("y", y);

  ListValue* list_value = new ListValue();
  for (size_t path_index = 0; path_index < paths.size(); ++path_index) {
    list_value->Append(Value::CreateStringValue(paths[path_index]));
  }
  dict.Set("paths", list_value);

  DictionaryValue reply_dict;
  return SendAutomationJSONRequest(sender, dict, &reply_dict, error);
}

bool SendSetViewBoundsJSONRequest(
    AutomationMessageSender* sender,
    const WebViewId& id,
    int x,
    int y,
    int width,
    int height,
    automation::Error* error) {
  DictionaryValue dict;
  dict.SetString("command", "SetViewBounds");
  id.UpdateDictionary(&dict, "auto_id");
  dict.SetInteger("bounds.x", x);
  dict.SetInteger("bounds.y", y);
  dict.SetInteger("bounds.width", width);
  dict.SetInteger("bounds.height", height);

  DictionaryValue reply_dict;
  return SendAutomationJSONRequest(sender, dict, &reply_dict, error);
}

bool SendGetAppModalDialogMessageJSONRequest(
    AutomationMessageSender* sender,
    std::string* message,
    Error* error) {
  DictionaryValue dict;
  dict.SetString("command", "GetAppModalDialogMessage");
  DictionaryValue reply_dict;
  if (!SendAutomationJSONRequest(sender, dict, &reply_dict, error))
    return false;
  return reply_dict.GetString("message", message);
}

bool SendAcceptOrDismissAppModalDialogJSONRequest(
    AutomationMessageSender* sender,
    bool accept,
    Error* error) {
  DictionaryValue dict;
  dict.SetString("command", "AcceptOrDismissAppModalDialog");
  dict.SetBoolean("accept", accept);
  DictionaryValue reply_dict;
  return SendAutomationJSONRequest(sender, dict, &reply_dict, error);
}

bool SendAcceptPromptAppModalDialogJSONRequest(
    AutomationMessageSender* sender,
    const std::string& prompt_text,
    Error* error) {
  DictionaryValue dict;
  dict.SetString("command", "AcceptOrDismissAppModalDialog");
  dict.SetBoolean("accept", true);
  dict.SetString("prompt_text", prompt_text);
  DictionaryValue reply_dict;
  return SendAutomationJSONRequest(sender, dict, &reply_dict, error);
}

bool SendWaitForAllViewsToStopLoadingJSONRequest(
    AutomationMessageSender* sender,
    Error* error) {
  DictionaryValue dict;
  dict.SetString("command", "WaitForAllTabsToStopLoading");
  DictionaryValue reply_dict;
  return SendAutomationJSONRequest(sender, dict, &reply_dict, error);
}

bool SendGetChromeDriverAutomationVersion(
    AutomationMessageSender* sender,
    int* version,
    Error* error) {
  DictionaryValue dict;
  dict.SetString("command", "GetChromeDriverAutomationVersion");
  DictionaryValue reply_dict;
  if (!SendAutomationJSONRequest(sender, dict, &reply_dict, error))
    return false;
  return reply_dict.GetInteger("version", version);
}

bool SendInstallExtensionJSONRequest(
    AutomationMessageSender* sender,
    const FilePath& path,
    bool with_ui,
    std::string* extension_id,
    Error* error) {
  DictionaryValue dict;
  dict.SetString("command", "InstallExtension");
  dict.SetString("path", path.value());
  dict.SetBoolean("with_ui", with_ui);
  // TODO(kkania): Set correct auto_id instead of hardcoding windex.
  dict.SetInteger("windex", 0);
  DictionaryValue reply_dict;
  if (!SendAutomationJSONRequest(sender, dict, &reply_dict, error))
    return false;
  if (!reply_dict.GetString("id", extension_id)) {
    *error = Error("Missing or invalid 'id'");
    return false;
  }
  return true;
}

bool SendGetExtensionsInfoJSONRequest(
    AutomationMessageSender* sender,
    ListValue* extensions_list,
    Error* error) {
  DictionaryValue dict;
  dict.SetString("command", "GetExtensionsInfo");
  // TODO(kkania): Set correct auto_id instead of hardcoding windex.
  dict.SetInteger("windex", 0);
  DictionaryValue reply_dict;
  if (!SendAutomationJSONRequest(sender, dict, &reply_dict, error))
    return false;
  ListValue* extensions_list_swap;
  if (!reply_dict.GetList("extensions", &extensions_list_swap)) {
    *error = Error("Missing or invalid 'extensions'");
    return false;
  }
  extensions_list->Swap(extensions_list_swap);
  return true;
}

bool SendIsPageActionVisibleJSONRequest(
    AutomationMessageSender* sender,
    const WebViewId& tab_id,
    const std::string& extension_id,
    bool* is_visible,
    Error* error) {
  DictionaryValue dict;
  dict.SetString("command", "IsPageActionVisible");
  tab_id.UpdateDictionary(&dict, "auto_id");
  dict.SetString("extension_id", extension_id);
  // TODO(kkania): Set correct auto_id instead of hardcoding windex.
  dict.SetInteger("windex", 0);
  DictionaryValue reply_dict;
  if (!SendAutomationJSONRequest(sender, dict, &reply_dict, error))
    return false;
  if (!reply_dict.GetBoolean("is_visible", is_visible)) {
    *error = Error("Missing or invalid 'is_visible'");
    return false;
  }
  return true;
}

bool SendSetExtensionStateJSONRequest(
    AutomationMessageSender* sender,
    const std::string& extension_id,
    bool enable,
    bool allow_in_incognito,
    Error* error) {
  DictionaryValue dict;
  dict.SetString("command", "SetExtensionStateById");
  dict.SetString("id", extension_id);
  dict.SetBoolean("enable", enable);
  dict.SetBoolean("allow_in_incognito", allow_in_incognito);
  // TODO(kkania): Set correct auto_id instead of hardcoding windex.
  dict.SetInteger("windex", 0);
  DictionaryValue reply_dict;
  return SendAutomationJSONRequest(sender, dict, &reply_dict, error);
}

bool SendClickExtensionButtonJSONRequest(
    AutomationMessageSender* sender,
    const std::string& extension_id,
    bool browser_action,
    Error* error) {
  DictionaryValue dict;
  if (browser_action)
    dict.SetString("command", "TriggerBrowserActionById");
  else
    dict.SetString("command", "TriggerPageActionById");
  dict.SetString("id", extension_id);
  // TODO(kkania): Set correct auto_id instead of hardcoding windex.
  dict.SetInteger("windex", 0);
  dict.SetInteger("tab_index", 0);
  DictionaryValue reply_dict;
  return SendAutomationJSONRequest(sender, dict, &reply_dict, error);
}

bool SendUninstallExtensionJSONRequest(
    AutomationMessageSender* sender,
    const std::string& extension_id,
    Error* error) {
  DictionaryValue dict;
  dict.SetString("command", "UninstallExtensionById");
  dict.SetString("id", extension_id);
  // TODO(kkania): Set correct auto_id instead of hardcoding windex.
  dict.SetInteger("windex", 0);
  DictionaryValue reply_dict;
  if (!SendAutomationJSONRequest(sender, dict, &reply_dict, error))
    return false;
  bool success;
  if (!reply_dict.GetBoolean("success", &success)) {
    *error = Error("Missing or invalid 'success'");
    return false;
  }
  if (!success) {
    *error = Error("Extension uninstall not permitted");
    return false;
  }
  return true;
}

bool SendSetLocalStatePreferenceJSONRequest(
    AutomationMessageSender* sender,
    const std::string& pref,
    base::Value* value,
    Error* error) {
  DictionaryValue dict;
  dict.SetString("command", "SetLocalStatePrefs");
  dict.SetString("path", pref);
  dict.Set("value", value);
  DictionaryValue reply_dict;
  return SendAutomationJSONRequest(sender, dict, &reply_dict, error);
}

bool SendSetPreferenceJSONRequest(
    AutomationMessageSender* sender,
    const std::string& pref,
    base::Value* value,
    Error* error) {
  DictionaryValue dict;
  dict.SetString("command", "SetPrefs");
  // TODO(kkania): Set correct auto_id instead of hardcoding windex.
  dict.SetInteger("windex", 0);
  dict.SetString("path", pref);
  dict.Set("value", value);
  DictionaryValue reply_dict;
  return SendAutomationJSONRequest(sender, dict, &reply_dict, error);
}

bool SendOverrideGeolocationJSONRequest(
    AutomationMessageSender* sender,
    base::DictionaryValue* geolocation,
    Error* error) {
  scoped_ptr<DictionaryValue> dict(geolocation->DeepCopy());
  dict->SetString("command", "OverrideGeoposition");
  DictionaryValue reply_dict;
  return SendAutomationJSONRequest(sender, *dict.get(), &reply_dict, error);
}
