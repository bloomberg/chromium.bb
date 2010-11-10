// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Cookie API implementation.

#include "ceee/ie/broker/cookie_api_module.h"

#include <atlbase.h>
#include <atlcom.h>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/string_util.h"
#include "ceee/common/com_utils.h"
#include "ceee/common/process_utils_win.h"
#include "ceee/common/window_utils.h"
#include "ceee/common/windows_constants.h"
#include "ceee/ie/broker/api_module_constants.h"
#include "ceee/ie/broker/api_module_util.h"
#include "ceee/ie/broker/window_api_module.h"
#include "ceee/ie/common/api_registration.h"
#include "chrome/browser/extensions/extension_cookies_api_constants.h"
#include "chrome/common/extensions/extension_error_utils.h"

namespace keys = extension_cookies_api_constants;

namespace cookie_api {

void RegisterInvocations(ApiDispatcher* dispatcher) {
#define REGISTER_API_FUNCTION(func) do { dispatcher->RegisterInvocation(\
    func##Function::function_name(), NewApiInvocation< func >); }\
  while (false)
  REGISTER_COOKIE_API_FUNCTIONS();
#undef REGISTER_API_FUNCTION
  // Now register the permanent event handler.
  dispatcher->RegisterPermanentEventHandler(keys::kOnChanged,
                                            CookieChanged::EventHandler);
}

bool CookieApiResult::CreateCookieValue(const CookieInfo& cookie_info) {
  scoped_ptr<DictionaryValue> cookie(new DictionaryValue());
  cookie->SetString(keys::kNameKey, com::ToString(cookie_info.name));
  cookie->SetString(keys::kValueKey, com::ToString(cookie_info.value));
  cookie->SetString(keys::kDomainKey, com::ToString(cookie_info.domain));
  cookie->SetBoolean(keys::kHostOnlyKey, cookie_info.host_only == TRUE);
  cookie->SetString(keys::kPathKey, com::ToString(cookie_info.path));
  cookie->SetBoolean(keys::kSecureKey, cookie_info.secure == TRUE);
  cookie->SetBoolean(keys::kHttpOnlyKey, cookie_info.http_only == TRUE);
  cookie->SetBoolean(keys::kSessionKey, cookie_info.session == TRUE);
  if (cookie_info.session == FALSE)
    cookie->SetReal(keys::kExpirationDateKey, cookie_info.expiration_date);
  cookie->SetString(keys::kStoreIdKey, com::ToString(cookie_info.store_id));
  DCHECK(value() == NULL);
  set_value(cookie.release());
  return true;
}

HRESULT CookieApiResult::GetCookieInfo(
    const std::string& url, const std::string& name, HWND window,
    CookieInfo* cookie_info) {
  // Get a tab window child of the cookie store window, so that we can properly
  // access session cookies.
  HWND tab_window = NULL;
  if (!window_utils::FindDescendentWindow(
          window, windows::kIeTabWindowClass, false, &tab_window) ||
      tab_window == NULL) {
    PostError("Failed to get tab window for a given cookie store.");
    return E_FAIL;
  }

  CComPtr<ICeeeCookieExecutor> executor;
  GetDispatcher()->GetExecutor(tab_window, IID_ICeeeCookieExecutor,
                               reinterpret_cast<void**>(&executor));
  if (executor == NULL) {
    LOG(WARNING) << "Failed to get an executor to get cookie info.";
    PostError("Internal Error while getting cookie info.");
    return E_FAIL;
  }

  HRESULT hr = executor->GetCookie(
      CComBSTR(url.data()), CComBSTR(name.data()), cookie_info);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to get cookie info." << com::LogHr(hr);
    PostError("Internal Error while getting cookie info.");
  }
  return hr;
}

bool CookieApiResult::CreateCookieStoreValue(DWORD process_id,
                                             const WindowSet& windows) {
  scoped_ptr<DictionaryValue> cookie_store(new DictionaryValue());
  std::ostringstream store_id_stream;
  store_id_stream << process_id;
  // For IE CEEE, we use a string representation of the IE process ID as the
  // cookie store ID.
  cookie_store->SetString(keys::kIdKey, store_id_stream.str());
  DCHECK(windows.size());
  if (windows.size() == 0) {
    PostError(api_module_constants::kInternalErrorError);
    return false;
  }
  WindowSet::const_iterator iter = windows.begin();
  // First register the cookie store once per process.
  if (FAILED(RegisterCookieStore(*iter)))
    return false;
  // Now collect all tab IDs from each frame window into a single list.
  scoped_ptr<ListValue> tab_ids(new ListValue());
  for (; iter != windows.end(); ++iter) {
    if (!AppendToTabIdList(*iter, tab_ids.get())) {
      PostError(api_module_constants::kInternalErrorError);
      return false;
    }
  }
  cookie_store->Set(keys::kTabIdsKey, tab_ids.release());
  set_value(cookie_store.release());
  PostResult();
  return true;
}

HRESULT CookieApiResult::RegisterCookieStore(HWND window) {
  CComPtr<ICeeeCookieExecutor> executor;
  GetDispatcher()->GetExecutor(window, IID_ICeeeCookieExecutor,
                               reinterpret_cast<void**>(&executor));
  if (executor == NULL) {
    LOG(WARNING) << "Failed to get an executor to register a cookie store.";
    PostError(api_module_constants::kInternalErrorError);
    return E_FAIL;
  }
  HRESULT hr = executor->RegisterCookieStore();
  if (FAILED(hr)) {
    // No DCHECK, this may happen if the window/thread dies on the way.
    LOG(ERROR) << "Can't register cookie store. " << com::LogHr(hr);
    PostError(api_module_constants::kInternalErrorError);
  }
  return hr;
}

HRESULT CookieApiResult::CookieStoreIsRegistered(HWND window) {
  CComPtr<ICeeeCookieExecutor> executor;
  GetDispatcher()->GetExecutor(window, IID_ICeeeCookieExecutor,
                               reinterpret_cast<void**>(&executor));
  if (executor == NULL) {
    LOG(WARNING) << "Failed to get an executor to register a cookie store.";
    PostError(api_module_constants::kInternalErrorError);
    return E_FAIL;
  }
  HRESULT hr = executor->CookieStoreIsRegistered();
  if (FAILED(hr)) {
    // No DCHECK, this may happen if the window/thread dies on the way.
    LOG(ERROR) << "Error accessing cookie store. " << com::LogHr(hr);
    PostError(api_module_constants::kInternalErrorError);
  }
  return hr;
}

bool CookieApiResult::AppendToTabIdList(HWND window, ListValue* tab_ids) {
  CComPtr<ICeeeWindowExecutor> executor;
  ApiDispatcher* dispatcher = GetDispatcher();
  DCHECK(dispatcher != NULL);
  dispatcher->GetExecutor(window, IID_ICeeeWindowExecutor,
                          reinterpret_cast<void**>(&executor));
  if (executor == NULL) {
    LOG(WARNING) << "Failed to get an executor to get window tabs.";
    return false;
  }
  CComBSTR tab_handles;
  HRESULT hr = executor->GetTabs(&tab_handles);
  if (FAILED(hr)) {
    // No DCHECK, this may happen if the window/thread dies on the way.
    LOG(ERROR) << "Can't get tabs for window: " << std::hex << window <<
        ". " << com::LogHr(hr);
    return false;
  }
  scoped_ptr<ListValue> tabs_list;
  if (!api_module_util::GetListFromJsonString(CW2A(tab_handles).m_psz,
                                              &tabs_list)) {
    NOTREACHED() << "Invalid tabs list BSTR: " << tab_handles;
    return false;
  }
  size_t num_values = tabs_list->GetSize();
  if (num_values % 2 != 0) {
    // Values should come in pairs, one for the handle and another one for the
    // index.
    NOTREACHED() << "Invalid tabs list BSTR: " << tab_handles;
    return false;
  }
  for (size_t i = 0; i < num_values; i += 2) {
    int tab_handle = 0;
    tabs_list->GetInteger(i, &tab_handle);
    int tab_id = dispatcher->GetTabIdFromHandle(
        reinterpret_cast<HWND>(tab_handle));
    tab_ids->Append(Value::CreateIntegerValue(tab_id));
  }
  return true;
}

void CookieApiResult::FindAllProcessesAndWindows(
    ProcessWindowMap* all_windows) {
  DCHECK(all_windows);
  WindowSet ie_frame_windows;
  window_utils::FindTopLevelWindows(windows::kIeFrameWindowClass,
                                    &ie_frame_windows);
  if (ie_frame_windows.empty())
    return;

  WindowSet::const_iterator iter = ie_frame_windows.begin();
  for (; iter != ie_frame_windows.end(); ++iter) {
    DWORD process_id = 0;
    // Skip over windows with no thread.
    if (::GetWindowThreadProcessId(*iter, &process_id) == 0)
      continue;
    DCHECK(process_id);

    if (process_id != 0)
      (*all_windows)[process_id].insert(*iter);
  }
}

HWND CookieApiResult::GetWindowFromStoreId(const std::string& store_id,
                                           bool allow_unregistered_store) {
  DWORD store_process_id = 0;
  std::istringstream store_id_stream(store_id);
  store_id_stream >> store_process_id;
  if (!store_process_id) {
    PostError(ExtensionErrorUtils::FormatErrorMessage(
        keys::kInvalidStoreIdError, store_id));
    return NULL;
  }

  WindowSet ie_frame_windows;
  window_utils::FindTopLevelWindows(windows::kIeFrameWindowClass,
                                    &ie_frame_windows);

  WindowSet::const_iterator iter = ie_frame_windows.begin();
  for (; iter != ie_frame_windows.end(); ++iter) {
    DWORD process_id = 0;
    if (::GetWindowThreadProcessId(*iter, &process_id) != 0 &&
        process_id == store_process_id) {
      if (allow_unregistered_store)
        return *iter;
      HRESULT hr = CookieStoreIsRegistered(*iter);
      // If the above call failed, an error has already been posted.
      if (FAILED(hr)) {
        return NULL;
      } else if (hr == S_OK) {
        return *iter;
      }
    }
  }
  // Matching window not found.
  PostError(ExtensionErrorUtils::FormatErrorMessage(
      keys::kInvalidStoreIdError, store_id));
  return NULL;
}

void GetCookie::Execute(const ListValue& args, int request_id) {
  scoped_ptr<CookieApiResult> result(CreateApiResult(request_id));

  DictionaryValue* details;
  if (!args.GetDictionary(0, &details)) {
    result->PostError(api_module_constants::kInvalidArgumentsError);
    return;
  }

  std::string url;
  if (!details->GetString(keys::kUrlKey, &url)) {
    result->PostError(api_module_constants::kInvalidArgumentsError);
    return;
  }
  if (!GURL(url).is_valid()) {
    result->PostError(ExtensionErrorUtils::FormatErrorMessage(
      keys::kInvalidUrlError, url));
    return;
  }
  // TODO(cindylau@chromium.org): Add extension host permissions
  // checks against the URL.

  std::string name;
  if (!details->GetString(keys::kNameKey, &name)) {
    result->PostError(api_module_constants::kInvalidArgumentsError);
    return;
  }

  HWND cookie_store_window = NULL;
  if (details->HasKey(keys::kStoreIdKey)) {
    std::string store_id;
    if (!details->GetString(keys::kStoreIdKey, &store_id)) {
      result->PostError(api_module_constants::kInvalidArgumentsError);
      return;
    }
    cookie_store_window = result->GetWindowFromStoreId(store_id, false);
    // If no window was found, an error has already been posted.
    if (!cookie_store_window)
      return;
  } else {
    // The store ID was unspecified or isn't a registered cookie store
    // ID. Use the current execution context's cookie store by default.
    // TODO(cindylau@chromium.org): We currently don't have a way to
    // get the current execution context, so we are using the top IE
    // window for now.
    cookie_store_window = window_api::WindowApiResult::TopIeWindow();
  }
  DCHECK(cookie_store_window);

  CookieInfo cookie_info;
  HRESULT hr = result->GetCookieInfo(url, name, cookie_store_window,
                                     &cookie_info);
  // If the call failed, an error has already been posted.
  if (FAILED(hr))
    return;
  if (hr == S_OK) {
    DCHECK(WideToASCII(com::ToString(cookie_info.name)) == name);
    if (!result->CreateCookieValue(cookie_info))
      return;
  }
  result->PostResult();
}

void GetAllCookies::Execute(const ListValue& args, int request_id) {
  scoped_ptr<CookieApiResult> result(CreateApiResult(request_id));
  result->PostError("Not implemented.");
}

void SetCookie::Execute(const ListValue& args, int request_id) {
  scoped_ptr<CookieApiResult> result(CreateApiResult(request_id));
  result->PostError("Not implemented.");
}

void RemoveCookie::Execute(const ListValue& args, int request_id) {
  scoped_ptr<CookieApiResult> result(CreateApiResult(request_id));
  result->PostError("Not implemented.");
}

void GetAllCookieStores::Execute(const ListValue& args, int request_id) {
  scoped_ptr<IterativeCookieApiResult> result(CreateApiResult(request_id));

  // TODO(cindylau@chromium.org): Restrict incognito (InPrivate)
  // windows if incognito is not enabled for the extension. Right now
  // CEEE has no mechanism to discover the extension's
  // incognito-enabled setting, so adding support here is premature.
  CookieApiResult::ProcessWindowMap all_windows;
  CookieApiResult::FindAllProcessesAndWindows(&all_windows);

  if (all_windows.empty()) {
    result->FlushAllPosts();
    return;
  }

  CookieApiResult::ProcessWindowMap::const_iterator iter = all_windows.begin();
  for (; iter != all_windows.end(); ++iter) {
    bool created_ok = result->CreateCookieStoreValue(iter->first, iter->second);
    LOG_IF(WARNING, !created_ok) << "Could not create cookie store value:"
        << result->LastError();
  }

  if (result->IsEmpty())  // This is an error!
    result->PostError(keys::kNoCookieStoreFoundError);

  result->FlushAllPosts();
}

// Static wrapper for the real event handler implementation.
bool CookieChanged::EventHandler(const std::string& input_args,
                                 std::string* converted_args,
                                 ApiDispatcher* dispatcher) {
  CookieChanged event_handler;
  return event_handler.EventHandlerImpl(input_args, converted_args);
}

// Handles a cookies.onChanged event by verifying that the store ID is
// registered. If not, this function registers the store ID.
bool CookieChanged::EventHandlerImpl(const std::string& input_args,
                                     std::string* converted_args) {
  DCHECK(converted_args);
  // We don't need to modify the arguments, we only need to verify that the
  // store ID is registered.
  *converted_args = input_args;

  // Get the cookie info from the input arguments.
  scoped_ptr<Value> input_val(base::JSONReader::Read(input_args, true));
  DCHECK(input_val.get() != NULL);
  if (input_val == NULL) {
    LOG(ERROR) << "Invalid Arguments sent to CookieChangedEventHandler()";
    return false;
  }

  ListValue* input_list = static_cast<ListValue*>(input_val.get());
  DCHECK(input_list && input_list->GetSize() == 1);

  DictionaryValue* changeInfo = NULL;
  bool success = input_list->GetDictionary(0, &changeInfo);
  DictionaryValue* cookie = NULL;
  if (success && changeInfo && changeInfo->HasKey(keys::kCookieKey))
    success = changeInfo->GetDictionary(keys::kCookieKey, &cookie);
  if (!success || cookie == NULL) {
    NOTREACHED() << "Failed to get the cookie value from the list of args.";
    return false;
  }
  std::string store_id;
  if (cookie->HasKey(keys::kStoreIdKey))
    success = cookie->GetString(keys::kStoreIdKey, &store_id);
  if (!success || store_id.size() == 0) {
    NOTREACHED() << "Failed to get the store ID value from the cookie arg.";
    return false;
  }

  scoped_ptr<CookieApiResult> api_result(CreateApiResult());
  HWND store_window = api_result->GetWindowFromStoreId(store_id, true);
  if (store_window == NULL) {
    // Error was already logged by GetWindowFromStoreId.
    return false;
  }
  HRESULT is_registered = api_result->CookieStoreIsRegistered(store_window);
  if (FAILED(is_registered)) {
    // Error was already logged by CookieStoreIsRegistered.
    return false;
  }
  if (is_registered == S_FALSE) {
    // The store ID has not yet been queried by the user; register it here
    // before exposing it to the user.
    is_registered = api_result->RegisterCookieStore(store_window);
  }
  if (is_registered != S_OK) {
    // Any errors should have already been logged by RegisterCookieStore.
    return false;
  }

  return true;
}

CookieInfo::CookieInfo() {
  name = NULL;
  value = NULL;
  domain = NULL;
  host_only = false;
  path = NULL;
  secure = false;
  http_only = false;
  session = false;
  expiration_date = 0;
  store_id = NULL;
}

CookieInfo::~CookieInfo() {
  // SysFreeString accepts NULL pointers.
  ::SysFreeString(name);
  ::SysFreeString(value);
  ::SysFreeString(domain);
  ::SysFreeString(path);
  ::SysFreeString(store_id);
}

}  // namespace cookie_api
