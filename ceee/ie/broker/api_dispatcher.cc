// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Dispatcher and registry for Chrome Extension APIs.

#include "ceee/ie/broker/api_dispatcher.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "ceee/common/com_utils.h"
#include "ceee/ie/broker/chrome_postman.h"
#include "ceee/ie/broker/cookie_api_module.h"
#include "ceee/ie/broker/executors_manager.h"
#include "ceee/ie/broker/infobar_api_module.h"
#include "ceee/ie/broker/tab_api_module.h"
#include "ceee/ie/broker/webnavigation_api_module.h"
#include "ceee/ie/broker/webrequest_api_module.h"
#include "ceee/ie/broker/window_api_module.h"
#include "chrome/browser/automation/extension_automation_constants.h"

namespace keys = extension_automation_constants;

bool ApiDispatcher::IsRunningInSingleThread() {
  return thread_id_ == ::GetCurrentThreadId();
}


void ApiDispatcher::HandleApiRequest(BSTR message_text, BSTR* response) {
  DCHECK(IsRunningInSingleThread());
  DCHECK(message_text);

  scoped_ptr<Value> base_value(base::JSONReader::Read(CW2A(message_text).m_psz,
                                                false));
  DCHECK(base_value.get() && base_value->GetType() == Value::TYPE_DICTIONARY);
  if (!base_value.get() || base_value->GetType() != Value::TYPE_DICTIONARY) {
    return;
  }
  DictionaryValue* value = static_cast<DictionaryValue*>(base_value.get());

  std::string function_name;
  if (!value->GetString(keys::kAutomationNameKey, &function_name)) {
    DCHECK(false);
    return;
  }

  std::string args_string;
  if (!value->GetString(keys::kAutomationArgsKey, &args_string)) {
    DCHECK(false);
    return;
  }

  base::JSONReader reader;
  scoped_ptr<Value> args(reader.JsonToValue(args_string, false, false));
  DCHECK(args.get() && args->GetType() == Value::TYPE_LIST);
  if (!args.get() || args->GetType() != Value::TYPE_LIST) {
    return;
  }
  ListValue* args_list = static_cast<ListValue*>(args.get());

  int request_id = -1;
  if (!value->GetInteger(keys::kAutomationRequestIdKey, &request_id)) {
    DCHECK(false);
    return;
  }

  // The tab value is optional, and maybe NULL if the API call didn't originate
  // from a tab context (e.g. the API call came from the background page).
  DictionaryValue* associated_tab_value = NULL;
  value->GetDictionary(keys::kAutomationTabJsonKey, &associated_tab_value);

  DLOG(INFO) << "Request: " << request_id << ", for: " << function_name;
  FactoryMap::const_iterator iter = factories_.find(function_name);
  DCHECK(iter != factories_.end());
  if (iter == factories_.end()) {
    return;
  }

  scoped_ptr<Invocation> invocation(iter->second());
  DCHECK(invocation.get());
  invocation->Execute(*args_list, request_id, associated_tab_value);
}

void ApiDispatcher::FireEvent(const char* event_name, const char* event_args) {
  DCHECK(IsRunningInSingleThread());
  DLOG(INFO) << "ApiDispatcher::FireEvent. " << event_name << " - " <<
      event_args;
  std::string event_args_str(event_args);
  std::string event_name_str(event_name);
  // Start by going through the permanent event handlers map.
  PermanentEventHandlersMap::const_iterator iter =
      permanent_event_handlers_.find(event_name_str);
  bool fire_event = true;
  std::string converted_event_args;
  if (iter != permanent_event_handlers_.end()) {
    fire_event = iter->second(event_args_str, &converted_event_args, this);
  } else {
    // Some events don't need a handler and
    // have all the info already available.
    converted_event_args = event_args_str;
  }

  if (fire_event) {
    // The notification message is an array containing event name as a string
    // at index 0 and then a JSON encoded string of the other arguments.
    ListValue message;
    message.Append(Value::CreateStringValue(event_name_str));

    // Event messages expect a string for the arguments.
    message.Append(Value::CreateStringValue(converted_event_args));

    std::string message_str;
    base::JSONWriter::Write(&message, false, &message_str);
    ChromePostman::GetInstance()->PostMessage(CComBSTR(message_str.c_str()),
        CComBSTR(keys::kAutomationBrowserEventRequestTarget));
  }

  // We do this after firing the event, to be in the same order as Chrome.
  // And to allow reusing the args conversion from the permanent handler.
  EphemeralEventHandlersMap::iterator map_iter =
      ephemeral_event_handlers_.find(event_name_str);
  if (map_iter != ephemeral_event_handlers_.end() &&
      !map_iter->second.empty()) {
    // We must work with a copy since other handlers might get registered
    // as we call the current set of handlers (some of them chain themselves).
    EphemeralEventHandlersList handlers_list;
    map_iter->second.swap(handlers_list);
    // Swap emptied our current list, so we will need to add back the items
    // that are not affected by this call (the ones that we wouldn't want
    // to remove from the list just yet return S_FALSE). This scheme is much
    // simpler than trying to keep up with the live list as it may change.
    EphemeralEventHandlersList::iterator list_iter = handlers_list.begin();
    for (;list_iter != handlers_list.end(); ++list_iter) {
      if (list_iter->handler(converted_event_args,
                             list_iter->user_data, this) == S_FALSE) {
        map_iter->second.push_back(*list_iter);
      }
    }
  }
}

void ApiDispatcher::GetExecutor(HWND window, REFIID iid, void** executor) {
  DWORD thread_id = ::GetWindowThreadProcessId(window, NULL);
  HRESULT hr = Singleton<ExecutorsManager,
                         ExecutorsManager::SingletonTraits>::get()->
                            GetExecutor(thread_id, window, iid, executor);
  DLOG_IF(INFO, FAILED(hr)) << "Failed to get executor for window: " <<
      window << ". In thread: " << thread_id << ". " << com::LogHr(hr);
}

bool ApiDispatcher::IsTabIdValid(int tab_id) const {
  return Singleton<ExecutorsManager, ExecutorsManager::SingletonTraits>::get()->
      IsTabIdValid(tab_id);
}

HWND ApiDispatcher::GetTabHandleFromId(int tab_id) const {
  return Singleton<ExecutorsManager, ExecutorsManager::SingletonTraits>::get()->
      GetTabHandleFromId(tab_id);
}

HWND ApiDispatcher::GetTabHandleFromToolBandId(int tool_band_id) const {
  return Singleton<ExecutorsManager, ExecutorsManager::SingletonTraits>::get()->
      GetTabHandleFromToolBandId(tool_band_id);
}

HWND ApiDispatcher::GetWindowHandleFromId(int window_id) const {
  return reinterpret_cast<HWND>(window_id);
}

bool ApiDispatcher::IsTabHandleValid(HWND tab_handle) const {
  return Singleton<ExecutorsManager, ExecutorsManager::SingletonTraits>::get()->
      IsTabHandleValid(tab_handle);
}

int ApiDispatcher::GetTabIdFromHandle(HWND tab_handle) const {
  return Singleton<ExecutorsManager, ExecutorsManager::SingletonTraits>::get()->
      GetTabIdFromHandle(tab_handle);
}

int ApiDispatcher::GetWindowIdFromHandle(HWND window_handle) const {
  return reinterpret_cast<int>(window_handle);
}

void ApiDispatcher::DeleteTabHandle(HWND handle) {
  Singleton<ExecutorsManager, ExecutorsManager::SingletonTraits>::get()->
      DeleteTabHandle(handle);
}

void ApiDispatcher::RegisterInvocation(const char* function_name,
                                       InvocationFactory factory) {
  DCHECK(function_name && factory);
  // Re-registration is not expected.
  DCHECK(function_name && factories_.find(function_name) == factories_.end());
  if (function_name && factory)
    factories_[function_name] = factory;
}

void ApiDispatcher::RegisterPermanentEventHandler(
    const char* event_name, PermanentEventHandler event_handler) {
  DCHECK(event_name && event_handler);
  // Re-registration is not expected.
  DCHECK(event_name && permanent_event_handlers_.find(event_name) ==
         permanent_event_handlers_.end());
  if (event_name && event_handler)
    permanent_event_handlers_[event_name] = event_handler;
}

void ApiDispatcher::RegisterEphemeralEventHandler(
    const char* event_name,
    EphemeralEventHandler event_handler,
    InvocationResult* user_data) {
  DCHECK(IsRunningInSingleThread());
  // user_data could be NULL, we don't care.
  DCHECK(event_handler != NULL);
  if (event_handler != NULL) {
    ephemeral_event_handlers_[event_name].push_back(
        EphemeralEventHandlerTuple(event_handler, user_data));
  }
}

void ApiDispatcher::Invocation::Execute(const ListValue& args, int request_id) {
  NOTREACHED();
}

ApiDispatcher::InvocationResult::~InvocationResult() {
  // We must have posted the response before we died.
  DCHECK(request_id_ == kNoRequestId);
}

// A quick helper function to reuse the call to ChromePostman.
void ApiDispatcher::InvocationResult::PostResponseToChromeFrame(
    const char* response_key, const std::string& response_str) {
  DictionaryValue response_dict;
  response_dict.SetInteger(keys::kAutomationRequestIdKey, request_id_);
  response_dict.SetString(response_key, response_str);

  std::string response;
  base::JSONWriter::Write(&response_dict, false, &response);
  ChromePostman::GetInstance()->PostMessage(
      CComBSTR(response.c_str()), CComBSTR(keys::kAutomationResponseTarget));
}

// Once an invocation completes successfully, it must call this method on its
// result object to send it back to Chrome.
void ApiDispatcher::InvocationResult::PostResult() {
  // We should never post results twice, or for non requested results.
  DCHECK(request_id_ != kNoRequestId);
  if (request_id_ != kNoRequestId) {
    std::string result_str;
    // Some invocations don't return values. We can set an empty string here.
    if (value_.get() != NULL)
      base::JSONWriter::Write(value_.get(), false, &result_str);
    PostResponseToChromeFrame(keys::kAutomationResponseKey, result_str);
    // We should only post once!
    request_id_ = kNoRequestId;
  }
}

// Invocations can use this method to post an error to Chrome when it
// couldn't complete the invocation successfully.
void ApiDispatcher::InvocationResult::PostError(const std::string& error) {
  LOG(ERROR) << error;
  // Event handlers reuse InvocationResult code without a requestId,
  // so don't DCHECK as in PostResult here.
  // TODO(mad@chromium.org): Might be better to use a derived class instead.
  if (request_id_ != kNoRequestId) {
    PostResponseToChromeFrame(keys::kAutomationErrorKey, error);
    // We should only post once!
    request_id_ = kNoRequestId;
  }
}

void ApiDispatcher::InvocationResult::SetValue(const char* name, Value* value) {
  DCHECK(name != NULL && value != NULL);
  scoped_ptr<Value> value_holder(value);
  if (name == NULL || value == NULL)
    return;

  if (temp_values_.get() == NULL) {
    temp_values_.reset(new DictionaryValue);
  }

  // scoped_ptr::release() lets go of its ownership.
  temp_values_->Set(name, value_holder.release());
}

const Value* ApiDispatcher::InvocationResult::GetValue(const char* name) {
  if (temp_values_.get() == NULL)
    return NULL;

  Value* value = NULL;
  bool success = temp_values_->Get(name, &value);
  // Make sure that success means NotNull and vice versa.
  DCHECK((value != NULL || !success) && (success || value == NULL));
  return value;
}

ApiDispatcher* ApiDispatcher::InvocationResult::GetDispatcher() {
  return ProductionApiDispatcher::get();
}

ApiDispatcher* ApiDispatcher::Invocation::GetDispatcher() {
  return ProductionApiDispatcher::get();
}

// Function registration preprocessor magic. See api_registration.h for details.
#ifdef REGISTER_ALL_API_FUNCTIONS
#error Must not include api_registration.h previously in this compilation unit.
#endif  // REGISTER_ALL_API_FUNCTIONS
#define PRODUCTION_API_DISPATCHER
#define REGISTER_TAB_API_FUNCTIONS() tab_api::RegisterInvocations(this)
#define REGISTER_WINDOW_API_FUNCTIONS() window_api::RegisterInvocations(this)
#define REGISTER_COOKIE_API_FUNCTIONS() cookie_api::RegisterInvocations(this)
#define REGISTER_INFOBAR_API_FUNCTIONS() infobar_api::RegisterInvocations(this)
#define REGISTER_WEBNAVIGATION_API_FUNCTIONS() \
    webnavigation_api::RegisterInvocations(this)
#define REGISTER_WEBREQUEST_API_FUNCTIONS() \
    webrequest_api::RegisterInvocations(this)
#include "ceee/ie/common/api_registration.h"

ProductionApiDispatcher::ProductionApiDispatcher() {
  REGISTER_ALL_API_FUNCTIONS();
}
