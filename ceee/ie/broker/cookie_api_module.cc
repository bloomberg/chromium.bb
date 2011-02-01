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
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_comptr.h"
#include "ceee/common/com_utils.h"
#include "ceee/common/process_utils_win.h"
#include "ceee/common/window_utils.h"
#include "ceee/common/windows_constants.h"
#include "ceee/ie/broker/api_module_constants.h"
#include "ceee/ie/broker/api_module_util.h"
#include "ceee/ie/broker/executors_manager.h"
#include "ceee/ie/broker/tab_api_module.h"
#include "ceee/ie/broker/window_api_module.h"
#include "ceee/ie/common/api_registration.h"
#include "chrome/browser/extensions/extension_cookies_api_constants.h"
#include "chrome/common/extensions/extension_error_utils.h"

namespace keys = extension_cookies_api_constants;

namespace cookie_api {

const char kProtectedModeStoreIdSuffix[] = "low";

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
    cookie->SetDouble(keys::kExpirationDateKey, cookie_info.expiration_date);
  cookie->SetString(keys::kStoreIdKey, com::ToString(cookie_info.store_id));
  DCHECK(value() == NULL);
  set_value(cookie.release());
  return true;
}

bool CookieApiResult::GetTabListForWindow(HWND window,
                                          scoped_ptr<ListValue>* tab_list) {
  DCHECK(tab_list);
  ApiDispatcher* dispatcher = GetDispatcher();
  DCHECK(dispatcher);
  base::win::ScopedComPtr<ICeeeWindowExecutor> executor;
  dispatcher->GetExecutor(window, IID_ICeeeWindowExecutor,
                          reinterpret_cast<void**>(executor.Receive()));
  if (executor == NULL) {
    LOG(WARNING) << "Failed to get an executor to get window tabs.";
    return false;
  }
  base::win::ScopedBstr tab_handles;
  HRESULT hr = executor->GetTabs(tab_handles.Receive());
  if (FAILED(hr)) {
    // No DCHECK, this may happen if the window/thread dies on the way.
    LOG(ERROR) << "Can't get tabs for window: " << std::hex << window <<
        ". " << com::LogHr(hr);
    return false;
  }
  scoped_ptr<ListValue> tab_list_local;
  if (!api_module_util::GetListFromJsonString(CW2A(tab_handles).m_psz,
                                              &tab_list_local)) {
    NOTREACHED() << "Invalid tabs list BSTR: " << tab_handles;
    return false;
  }
  size_t num_values = tab_list_local->GetSize();
  if (num_values % 2 != 0) {
    // Values should come in pairs, one for the handle and another one for the
    // index.
    NOTREACHED() << "Invalid tabs list BSTR: " << tab_handles;
    return false;
  }
  tab_list->reset(tab_list_local.release());
  return true;
}

HWND CookieApiResult::GetAnyTabInWindow(HWND window, bool is_protected_mode) {
  // The call to GetTabListForWindow will have logged any warnings/errors
  // already, so just return if it fails.
  scoped_ptr<ListValue> tab_list;
  if (!GetTabListForWindow(window, &tab_list))
    return NULL;
  DCHECK(tab_list.get() && tab_list->GetSize() % 2 == 0);

  // To support protected mode, we need to iterate through all the tabs in the
  // window to find one that has the proper security zone.
  // Find a tab that matches in protected mode, and return the first one
  // encountered.
  for (size_t i = 0; i < tab_list->GetSize(); i += 2) {
    int tab_value = 0;
    tab_list->GetInteger(i, &tab_value);
    HWND tab_handle = reinterpret_cast<HWND>(tab_value);
    bool tab_protected_mode = false;
    if (GetTabProtectedMode(tab_handle, &tab_protected_mode) &&
        is_protected_mode == tab_protected_mode) {
      return tab_handle;
    }
  }
  return NULL;
}

bool CookieApiResult::GetTabProtectedMode(HWND tab_window,
                                          bool* is_protected_mode) {
  DCHECK(is_protected_mode);
  if (!ExecutorsManager::IsKnownWindow(tab_window) ||
      window_utils::WindowHasNoThread(tab_window) ||
      !window_utils::IsWindowClass(tab_window, windows::kIeTabWindowClass)) {
    return false;
  }
  ApiDispatcher* dispatcher = GetDispatcher();
  DCHECK(dispatcher);
  base::win::ScopedComPtr<ICeeeTabExecutor> executor;
  dispatcher->GetExecutor(tab_window, IID_ICeeeTabExecutor,
                          reinterpret_cast<void**>(executor.Receive()));
  if (executor == NULL) {
    LOG(WARNING) << "Failed to get an executor to get tab info.";
    return false;
  }
  tab_api::TabInfo tab_info;
  HRESULT hr = executor->GetTabInfo(&tab_info);
  if (FAILED(hr)) {
    LOG(WARNING) << "Executor failed to get tab info." << com::LogHr(hr);
    return false;
  }
  *is_protected_mode = !!tab_info.protected_mode;
  return true;
}

HRESULT CookieApiResult::GetCookieInfo(
    const std::string& url, const std::string& name, HWND window,
    bool is_protected_mode, CookieInfo* cookie_info) {
  // Get a tab window child of the cookie store window, so that we can properly
  // access session cookies.
  HWND tab_window = GetAnyTabInWindow(window, is_protected_mode);
  if (tab_window == NULL) {
    PostError("Failed to get tab window for a given cookie store.");
    return E_FAIL;
  }

  ApiDispatcher* dispatcher = GetDispatcher();
  DCHECK(dispatcher);
  base::win::ScopedComPtr<ICeeeCookieExecutor> executor;
  dispatcher->GetExecutor(tab_window, IID_ICeeeCookieExecutor,
                          reinterpret_cast<void**>(executor.Receive()));
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

bool CookieApiResult::CreateCookieStoreValues(DWORD process_id,
                                              const WindowSet& windows) {
  DCHECK(windows.size());
  if (windows.size() == 0) {
    PostError(api_module_constants::kInternalErrorError);
    return false;
  }
  WindowSet::const_iterator iter = windows.begin();
  // First register the cookie store once per process.
  if (FAILED(RegisterCookieStore(*iter)))
    return false;
  // Now collect all tab IDs from each frame window into 2 lists, separating
  // protected mode tabs from non-protected mode tabs.
  scoped_ptr<ListValue> tab_ids(new ListValue());
  scoped_ptr<ListValue> protected_tab_ids(new ListValue());
  for (; iter != windows.end(); ++iter) {
    if (!AppendToTabIdLists(*iter, tab_ids.get(), protected_tab_ids.get())) {
      PostError(api_module_constants::kInternalErrorError);
      return false;
    }
  }
  // For IE CEEE, we use a string representation of the IE process ID as the
  // cookie store ID, plus a protected mode suffix if applicable.
  std::ostringstream store_id_stream;
  store_id_stream << process_id;
  // Now create a cookie store value for each non-empty tab list.
  if (tab_ids.get()->GetSize() > 0) {
    scoped_ptr<DictionaryValue> cookie_store(new DictionaryValue());
    cookie_store->SetString(keys::kIdKey, store_id_stream.str());
    cookie_store->Set(keys::kTabIdsKey, tab_ids.release());
    set_value(cookie_store.release());
    PostResult();
  }
  if (protected_tab_ids.get()->GetSize() > 0) {
    store_id_stream << kProtectedModeStoreIdSuffix;
    scoped_ptr<DictionaryValue> cookie_store(new DictionaryValue());
    cookie_store->SetString(keys::kIdKey, store_id_stream.str());
    cookie_store->Set(keys::kTabIdsKey, protected_tab_ids.release());
    set_value(cookie_store.release());
    PostResult();
  }
  return true;
}

HRESULT CookieApiResult::RegisterCookieStore(HWND window) {
  ApiDispatcher* dispatcher = GetDispatcher();
  DCHECK(dispatcher);
  base::win::ScopedComPtr<ICeeeCookieExecutor> executor;
  dispatcher->GetExecutor(window, IID_ICeeeCookieExecutor,
                          reinterpret_cast<void**>(executor.Receive()));
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
  ApiDispatcher* dispatcher = GetDispatcher();
  DCHECK(dispatcher);
  base::win::ScopedComPtr<ICeeeCookieExecutor> executor;
  dispatcher->GetExecutor(window, IID_ICeeeCookieExecutor,
                          reinterpret_cast<void**>(executor.Receive()));
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

bool CookieApiResult::AppendToTabIdLists(HWND window, ListValue* tab_ids,
                                         ListValue* protected_tab_ids) {
  DCHECK(tab_ids && protected_tab_ids);
  scoped_ptr<ListValue> tab_list;
  // The call to GetTabsListForWindow will have logged any warnings/errors
  // already, so just return if it fails.
  if (!GetTabListForWindow(window, &tab_list))
    return false;
  DCHECK(tab_list.get() && tab_list->GetSize() % 2 == 0);
  ApiDispatcher* dispatcher = GetDispatcher();
  DCHECK(dispatcher);
  for (size_t i = 0; i < tab_list->GetSize(); i += 2) {
    int tab_value = 0;
    tab_list->GetInteger(i, &tab_value);
    HWND tab_handle = reinterpret_cast<HWND>(tab_value);
    bool is_protected_mode = false;
    if (!GetTabProtectedMode(tab_handle, &is_protected_mode))
      continue;
    int tab_id = dispatcher->GetTabIdFromHandle(tab_handle);
    if (is_protected_mode)
      protected_tab_ids->Append(Value::CreateIntegerValue(tab_id));
    else
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
    if (!ExecutorsManager::IsKnownWindow(*iter))
      continue;
    DWORD process_id = 0;
    // Skip over windows with no thread.
    if (::GetWindowThreadProcessId(*iter, &process_id) == 0)
      continue;
    DCHECK(process_id);

    if (process_id != 0)
      (*all_windows)[process_id].insert(*iter);
  }
}

// A store ID string in IE CEEE is created by taking the ID of the process
// owning one or more IEFrame windows, and optionally appending "low" to it to
// indicate the protected mode store within the process.
// Starting with IE8, there is a 1-to-many mapping of IE processes to IEFrame
// (i.e. physical IE) windows. But 2 IEFrame windows that are in the same
// cookie store will always share the same process, so we use the process ID as
// the store ID.
// To further complicate things, windows within the same process may still
// put tabs in up to 2 different cookie stores, depending on whether those tabs
// are protected mode processes or not. So the store ID further differentiates
// these.
// In IE7, there's only 1 process for every cookie store, and all the IEFrame
// windows and the tabs live within that process, so it's still consistent with
// using the IEFrame window's proc ID as the store ID.
bool CookieApiResult::GetAnyWindowInStore(const std::string& store_id,
                                          bool allow_unregistered_store,
                                          HWND* window,
                                          bool* is_protected_mode) {
  DCHECK(window && is_protected_mode);
  DWORD store_process_id = 0;
  std::istringstream store_id_stream(store_id);
  std::string suffix;
  store_id_stream >> store_process_id >> suffix;
  if (!store_process_id) {
    PostError(ExtensionErrorUtils::FormatErrorMessage(
        keys::kInvalidStoreIdError, store_id));
    return false;
  }
  *is_protected_mode = (suffix == cookie_api::kProtectedModeStoreIdSuffix);

  WindowSet ie_frame_windows;
  window_utils::FindTopLevelWindows(windows::kIeFrameWindowClass,
                                    &ie_frame_windows);

  WindowSet::const_iterator iter = ie_frame_windows.begin();
  for (; iter != ie_frame_windows.end(); ++iter) {
    DWORD process_id = 0;
    if (!ExecutorsManager::IsKnownWindow(*iter))
      continue;

    if (::GetWindowThreadProcessId(*iter, &process_id) != 0 &&
        process_id == store_process_id) {
      if (allow_unregistered_store) {
        *window = *iter;
        return true;
      }
      HRESULT hr = CookieStoreIsRegistered(*iter);
      // If the above call failed, an error has already been posted.
      if (FAILED(hr)) {
        return false;
      } else if (hr == S_OK) {
        *window = *iter;
        return true;
      }
    }
  }
  // Matching window not found.
  PostError(ExtensionErrorUtils::FormatErrorMessage(
      keys::kInvalidStoreIdError, store_id));
  return false;
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
  bool is_protected_mode = false;
  if (details->HasKey(keys::kStoreIdKey)) {
    std::string store_id;
    if (!details->GetString(keys::kStoreIdKey, &store_id)) {
      result->PostError(api_module_constants::kInvalidArgumentsError);
      return;
    }
    // If no window was found, an error has already been posted.
    if (!result->GetAnyWindowInStore(store_id, false, &cookie_store_window,
                                     &is_protected_mode)) {
      return;
    }
  } else {
    // The store ID was unspecified or isn't a registered cookie store
    // ID. Use the current execution context's cookie store by default.
    // TODO(cindylau@chromium.org): This needs to be changed to use the
    // actual current window instead of the top IE window.
    cookie_store_window = window_api::WindowApiResult::TopIeWindow();
  }
  DCHECK(cookie_store_window);

  CookieInfo cookie_info;
  HRESULT hr = result->GetCookieInfo(url, name, cookie_store_window,
                                     is_protected_mode, &cookie_info);
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
    bool created_ok = result->CreateCookieStoreValues(iter->first,
                                                      iter->second);
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
  HWND store_window = NULL;
  bool is_protected_mode = false;
  if (!api_result->GetAnyWindowInStore(store_id, true, &store_window,
                                       &is_protected_mode)) {
    // Error was already logged by GetAnyWindowInStore.
    return false;
  }
  DCHECK(store_window);
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
