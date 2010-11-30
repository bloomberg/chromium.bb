// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Tab API implementation.
//
// Tab IDs are the window handle of the "TabWindowClass" window class
// of the whole tab.
//
// To find the chrome.window.* "window ID" we can just get the top-level parent
// window of the tab window.
//
// TODO(joi@chromium.org) Figure out what to do in IE6 (which has no tabs).

#include "ceee/ie/broker/tab_api_module.h"

#include <atlbase.h>
#include <atlcom.h>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_comptr.h"
#include "base/win/scoped_variant.h"
#include "base/win/windows_version.h"
#include "ceee/ie/broker/api_module_constants.h"
#include "ceee/ie/broker/api_module_util.h"
#include "ceee/ie/broker/executors_manager.h"
#include "ceee/ie/common/api_registration.h"
#include "ceee/ie/common/constants.h"
#include "ceee/ie/common/ie_util.h"
#include "chrome/browser/automation/extension_automation_constants.h"
#include "chrome/browser/extensions/extension_event_names.h"
#include "chrome/browser/extensions/extension_tabs_module_constants.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "ceee/common/com_utils.h"
#include "ceee/common/window_utils.h"
#include "ceee/common/windows_constants.h"


namespace ext = extension_tabs_module_constants;
namespace ext_event_names = extension_event_names;
namespace keys = extension_automation_constants;

namespace tab_api {

namespace {

// bb3147348
// Convert the tab_id parameter (always the first one) and verify that the
// event received has the right number of parameters.
// NumParam is the number of parameters we expect from events_funnel.
// AddTabParam if true, we add a Tab object to the converted_args.
template<int NumParam, bool AddTabParam>
bool ConvertTabIdEventHandler(const std::string& input_args,
                              std::string* converted_args,
                              ApiDispatcher* dispatcher) {
  DCHECK(converted_args);
  *converted_args = input_args;

  // Get the tab ID from the input arguments.
  scoped_ptr<ListValue> input_list;
  if (!api_module_util::GetListFromJsonString(input_args, &input_list)) {
    NOTREACHED() << "Invalid Arguments sent to event.";
    return false;
  }

  if (input_list == NULL || input_list->GetSize() != NumParam) {
    NOTREACHED() << "Invalid Number of Arguments sent to event.";
    return false;
  }

  int tab_handle = -1;
  bool success = input_list->GetInteger(0, &tab_handle);
  DCHECK(success) << "Failed getting the tab_id value from the list of args.";
  HWND tab_window = reinterpret_cast<HWND>(tab_handle);
  int tab_id = dispatcher->GetTabIdFromHandle(tab_window);
  DCHECK(tab_id != kInvalidChromeSessionId);
  if (tab_id == kInvalidChromeSessionId)
    return false;
  input_list->Set(0, Value::CreateIntegerValue(tab_id));

  if (AddTabParam) {
    TabApiResult result(TabApiResult::kNoRequestId);
    // Don't DCHECK here since we have cases where the tab died beforehand.
    if (!result.CreateTabValue(tab_id, -1)) {
      LOG(ERROR) << "Failed to create a value for tab: " << std::hex << tab_id;
      return false;
    }

    input_list->Append(result.value()->DeepCopy());
  }

  base::JSONWriter::Write(input_list.get(), false, converted_args);
  return true;
}

bool CeeeUnmapTabEventHandler(const std::string& input_args,
                              std::string* converted_args,
                              ApiDispatcher* dispatcher) {
  int tab_handle = reinterpret_cast<int>(INVALID_HANDLE_VALUE);

  scoped_ptr<ListValue> input_list;
  if (!api_module_util::GetListAndIntegerValue(input_args, &input_list,
                                               &tab_handle) ||
      tab_handle == kInvalidChromeSessionId) {
    NOTREACHED() << "An invalid argument was passed to UnmapTab";
    return false;
  }
#ifdef DEBUG
  int tab_id = kInvalidChromeSessionId;
  input_list->GetInteger(1, &tab_id);
  DCHECK(tab_id == dispatcher->GetTabIdFromHandle(
      reinterpret_cast<HWND>(tab_handle)));
#endif  // DEBUG

  HWND tab_window = reinterpret_cast<HWND>(tab_handle);
  dispatcher->DeleteTabHandle(tab_window);
  return false;
}

}  // namespace

void RegisterInvocations(ApiDispatcher* dispatcher) {
#define REGISTER_API_FUNCTION(func) do { dispatcher->RegisterInvocation(\
    func##Function::function_name(), NewApiInvocation< func >); }\
  while (false)
  REGISTER_TAB_API_FUNCTIONS();
#undef REGISTER_API_FUNCTION
  // Registers our private events.
  dispatcher->RegisterPermanentEventHandler(
      ceee_event_names::kCeeeOnTabUnmapped, CeeeUnmapTabEventHandler);

  // And now register the permanent event handlers.
  dispatcher->RegisterPermanentEventHandler(ext_event_names::kOnTabCreated,
                                            CreateTab::EventHandler);

  // For OnTabUpdate, we receive 2 from events_funnel, and add a Tab Parameter.
  dispatcher->RegisterPermanentEventHandler(ext_event_names::kOnTabUpdated,
                                            ConvertTabIdEventHandler<2, true>);
  dispatcher->RegisterPermanentEventHandler(ext_event_names::kOnTabAttached,
                                            ConvertTabIdEventHandler<2, false>);
  dispatcher->RegisterPermanentEventHandler(ext_event_names::kOnTabDetached,
                                            ConvertTabIdEventHandler<2, false>);
  dispatcher->RegisterPermanentEventHandler(ext_event_names::kOnTabMoved,
                                            ConvertTabIdEventHandler<2, false>);
  dispatcher->RegisterPermanentEventHandler(ext_event_names::kOnTabRemoved,
                                            ConvertTabIdEventHandler<1, false>);
  dispatcher->RegisterPermanentEventHandler(
      ext_event_names::kOnTabSelectionChanged,
      ConvertTabIdEventHandler<2, false>);
}

bool TabApiResult::CreateTabValue(int tab_id, long index) {
  ApiDispatcher* dispatcher = GetDispatcher();
  DCHECK(dispatcher != NULL);
  HWND tab_window = dispatcher->GetTabHandleFromId(tab_id);

  if (window_utils::WindowHasNoThread(tab_window)) {
    PostError(ExtensionErrorUtils::FormatErrorMessage(
        ext::kTabNotFoundError, base::IntToString(tab_id)));
    return false;
  }

  if (!IsTabWindowClass(tab_window)) {
    PostError(ExtensionErrorUtils::FormatErrorMessage(
        ext::kTabNotFoundError, base::IntToString(tab_id)));
    return false;
  }

  base::win::ScopedComPtr<ICeeeTabExecutor> executor;
  dispatcher->GetExecutor(tab_window, IID_ICeeeTabExecutor,
                          reinterpret_cast<void**>(executor.Receive()));
  if (executor == NULL) {
    LOG(WARNING) << "Failed to get an executor to get tab info.";
    PostError(api_module_constants::kInternalErrorError);
    return false;
  }

  TabInfo tab_info;
  HRESULT hr = executor->GetTabInfo(&tab_info);
  if (FAILED(hr)) {
    LOG(WARNING) << "Executor failed to get tab info." << com::LogHr(hr);
    PostError(api_module_constants::kInternalErrorError);
    return false;
  }

  scoped_ptr<DictionaryValue> result(new DictionaryValue());
  result->SetInteger(ext::kIdKey, tab_id);

  // TODO(mad@chromium.org): Support the pin field.
  result->SetBoolean(ext::kPinnedKey, false);

  // The window ID is just the window handle of the frame window, which is the
  // top-level ancestor of this window.
  HWND frame_window = window_utils::GetTopLevelParent(tab_window);
  if (frame_window == tab_window ||
      !window_utils::IsWindowClass(frame_window,
                                   windows::kIeFrameWindowClass)) {
    // If we couldn't get a valid parent frame window, then it must be because
    // the frame window (and the tab then) has been closed by now or it lives
    // under the hidden IE window.
    DCHECK(!::IsWindow(tab_window) || window_utils::IsWindowClass(frame_window,
        windows::kHiddenIeFrameWindowClass));
    PostError(ExtensionErrorUtils::FormatErrorMessage(
        ext::kTabNotFoundError, base::IntToString(tab_id)));
    return false;
  }
  int frame_window_id = dispatcher->GetWindowIdFromHandle(frame_window);
  result->SetInteger(ext::kWindowIdKey, frame_window_id);

  // Only the currently selected tab has the VS_VISIBLE style.
  result->SetBoolean(ext::kSelectedKey, TRUE == ::IsWindowVisible(tab_window));

  result->SetString(ext::kUrlKey, com::ToString(tab_info.url));
  result->SetString(ext::kTitleKey, com::ToString(tab_info.title));

  std::string status = ext::kStatusValueComplete;
  if (tab_info.status == kCeeeTabStatusLoading)
    status = ext::kStatusValueLoading;
  else
    DCHECK(tab_info.status == kCeeeTabStatusComplete) << "New Status???";

  result->SetString(ext::kStatusKey, status);

  if (tab_info.fav_icon_url != NULL) {
    result->SetString(ext::kFavIconUrlKey,
                      com::ToString(tab_info.fav_icon_url));
  }

  // When enumerating all tabs, we already have the index
  // so we can save an IPC call.
  if (index == -1) {
    // We need another executor to get the index from the frame window thread.
    base::win::ScopedComPtr<ICeeeWindowExecutor> executor;
    dispatcher->GetExecutor(frame_window, IID_ICeeeWindowExecutor,
                            reinterpret_cast<void**>(executor.Receive()));
    if (executor == NULL) {
      LOG(WARNING) << "Failed to get an executor to get tab index.";
      PostError(api_module_constants::kInternalErrorError);
      return false;
    }

    hr = executor->GetTabIndex(reinterpret_cast<CeeeWindowHandle>(tab_window),
                               &index);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to get tab info for tab: " << std::hex << tab_id <<
          ". " << com::LogHr(hr);
      PostError(api_module_constants::kInternalErrorError);
      return false;
    }
  }
  result->SetInteger(ext::kIndexKey, static_cast<int>(index));

  result->SetBoolean(ext::kIncognitoKey, ie_util::GetIEIsInPrivateBrowsing());

  if (value_ == NULL) {
    value_.reset(result.release());
  } else {
    DCHECK(value_->IsType(Value::TYPE_LIST));
    ListValue* list = reinterpret_cast<ListValue*>(value_.get());
    list->Append(result.release());
  }
  return true;
}

HRESULT TabApiResult::IsTabFromSameOrUnspecifiedFrameWindow(
    const DictionaryValue& input_dict,
    const Value* saved_window_value,
    HWND* tab_window,
    ApiDispatcher* dispatcher) {
  int tab_id = 0;
  bool success = input_dict.GetInteger(ext::kIdKey, &tab_id);
  DCHECK(success && tab_id != 0) << "The input_dict MUST have a tab ID!!!";
  DCHECK(dispatcher != NULL);
  if (!dispatcher->IsTabIdValid(tab_id)) {
    // This can happen if the tab died before we get here.
    LOG(WARNING) << "Ta ID: " << tab_id << ", not recognized.";
    return E_UNEXPECTED;
  }
  HWND input_tab_window = dispatcher->GetTabHandleFromId(tab_id);
  if (tab_window != NULL)
    *tab_window = input_tab_window;

  if (saved_window_value == NULL)
    return S_OK;

  DCHECK(saved_window_value->IsType(Value::TYPE_INTEGER));
  int saved_window_id = 0;
  success = saved_window_value->GetAsInteger(&saved_window_id);
  DCHECK(success && saved_window_id != 0);

  HWND frame_window = NULL;
  int frame_window_id = 0;
  if (!input_dict.GetInteger(ext::kWindowIdKey, &frame_window_id)) {
    // If the parent window is not specified, it is easy to fetch it ourselves.
    frame_window = window_utils::GetTopLevelParent(input_tab_window);
    frame_window_id = dispatcher->GetWindowIdFromHandle(frame_window);
    DCHECK_NE(0, frame_window_id);
  } else {
    frame_window = dispatcher->GetWindowHandleFromId(frame_window_id);
    DCHECK_EQ(window_utils::GetTopLevelParent(input_tab_window), frame_window);
  }

  return frame_window_id == saved_window_id ? S_OK : S_FALSE;
}

bool GetIntegerFromValue(
    const Value& value, const char* key_name, int* out_value) {
  switch (value.GetType()) {
    case Value::TYPE_INTEGER: {
      return value.GetAsInteger(out_value);
    }
    case Value::TYPE_DICTIONARY: {
      const DictionaryValue* dict = static_cast<const DictionaryValue*>(&value);
      if (dict->HasKey(key_name))
        return dict->GetInteger(key_name, out_value);
      *out_value = 0;
      return true;
    }
    case Value::TYPE_LIST: {
      const ListValue* args_list = static_cast<const ListValue*>(&value);
      Value* anonymous_value = NULL;
      if (!args_list->Get(0, &anonymous_value)) {
        // If given an empty list value, we return 0 so that the frame window is
        // fetched.
        *out_value = 0;
        return true;
      }
      DCHECK(anonymous_value != NULL);
      return GetIntegerFromValue(*anonymous_value, key_name, out_value);
    }
    case Value::TYPE_NULL: {
      // If given an empty list value, we return 0 so that the frame window is
      // fetched.
      *out_value = 0;
      return true;
    }
    default: {
      return false;
    }
  }
}

HWND TabApiResult::GetSpecifiedOrCurrentFrameWindow(const Value& args,
                                                    bool* specified) {
  int window_id = 0;
  if (!GetIntegerFromValue(args, ext::kWindowIdKey, &window_id)) {
    NOTREACHED() << "Invalid Arguments.";
    return NULL;
  }

  HWND frame_window = NULL;
  ApiDispatcher* dispatcher = GetDispatcher();
  DCHECK(dispatcher != NULL);
  if (window_id != 0)
    frame_window = dispatcher->GetWindowHandleFromId(window_id);

  if (!frame_window) {
    // TODO(mad@chromium.org): We currently don't have access to the
    // actual 'current' window from the point of view of the extension
    // API caller.  Use one of the top windows for now. bb2255140
    window_utils::FindDescendentWindow(NULL, windows::kIeFrameWindowClass,
                                       true, &frame_window);
    if (specified != NULL)
      *specified = false;
  } else {
    if (specified != NULL)
      *specified = true;
  }

  if (!frame_window) {
    return NULL;
  }

  if (!window_utils::IsWindowClass(frame_window, windows::kIeFrameWindowClass))
    return NULL;

  return frame_window;
}

void GetTab::Execute(const ListValue& args, int request_id) {
  scoped_ptr<TabApiResult> result(CreateApiResult(request_id));
  int tab_id = kInvalidChromeSessionId;
  if (!args.GetInteger(0, &tab_id)) {
    result->PostError(api_module_constants::kInvalidArgumentsError);
    return;
  }
  ApiDispatcher* dispatcher = GetDispatcher();
  DCHECK(dispatcher != NULL);

  if (!dispatcher->IsTabIdValid(tab_id)) {
    result->PostError(ExtensionErrorUtils::FormatErrorMessage(
        ext::kTabNotFoundError, base::IntToString(tab_id)));
    return;
  }

  HWND tab_window = dispatcher->GetTabHandleFromId(tab_id);
  if (!result->IsTabWindowClass(tab_window)) {
    result->PostError(ExtensionErrorUtils::FormatErrorMessage(
        ext::kTabNotFoundError, base::IntToString(tab_id)));
    return;
  }

  // -1 when we don't know the index.
  if (result->CreateTabValue(tab_id, -1)) {
    // CreateTabValue called PostError if it returned false.
    result->PostResult();
  }
}

void GetCurrentTab::Execute(const ListValue& args, int request_id,
                            const DictionaryValue* associated_tab) {
  // TODO(cindylau@chromium.org): This implementation of chrome.tabs.getCurrent
  // assumes that the associated tab will be that of a tool band, since that is
  // the only way we currently support running extension code in a tab context.
  // If the way we associate tool bands to IE tabs/windows changes, and/or if
  // we add other tab-related extension widgets (e.g. infobars, or directly
  // loading extension pages in host browser tabs), we will need to revisit
  // this implementation.
  scoped_ptr<TabApiResult> result(CreateApiResult(request_id));
  if (associated_tab == NULL) {
    // The associated tab may validly be NULL, for instance if the API call
    // originated from the background page.
    result->PostResult();
    return;
  }
  int tool_band_id;
  if (!associated_tab->GetInteger(ext::kIdKey, &tool_band_id)) {
    result->PostError(api_module_constants::kInternalErrorError);
    return;
  }

  ApiDispatcher* dispatcher = GetDispatcher();
  DCHECK(dispatcher != NULL);
  HWND tab_window = dispatcher->GetTabHandleFromToolBandId(tool_band_id);
  int tab_id = dispatcher->GetTabIdFromHandle(tab_window);
  if (!result->IsTabWindowClass(tab_window)) {
    result->PostError(ExtensionErrorUtils::FormatErrorMessage(
        ext::kTabNotFoundError, base::IntToString(tab_id)));
    return;
  }
  if (result->CreateTabValue(tab_id, -1))
    result->PostResult();
}

void GetSelectedTab::Execute(const ListValue& args, int request_id) {
  scoped_ptr<TabApiResult> result(CreateApiResult(request_id));

  bool specified = false;
  HWND frame_window = result->GetSpecifiedOrCurrentFrameWindow(args,
                                                               &specified);
  if (!frame_window) {
    result->PostError(ext::kNoCurrentWindowError);
    return;
  }

  // The selected tab is the only visible "TabWindowClass" window
  // that is a child of the frame window. Enumerate child windows to find it,
  // and fill in the value_ when we do.
  HWND selected_tab = NULL;
  ApiDispatcher* dispatcher = GetDispatcher();
  DCHECK(dispatcher != NULL);
  if (!window_utils::FindDescendentWindow(
      frame_window, windows::kIeTabWindowClass, true, &selected_tab) ||
      !ExecutorsManager::IsKnownWindow(selected_tab)) {
    if (specified) {
      int frame_window_id = dispatcher->GetWindowIdFromHandle(frame_window);
      // We remember the frame window if it was specified so that we only
      // react asynchronously to new tabs created in the same frame window.
      result->SetValue(ext::kWindowIdKey,
          Value::CreateIntegerValue(frame_window_id));
    }
    DCHECK(dispatcher != NULL);
    dispatcher->RegisterEphemeralEventHandler(
        ext_event_names::kOnTabCreated,
        GetSelectedTab::ContinueExecution,
        // We don't want to destroy the result in the scoped_ptr when we pass
        // it as user_data to GetSelectedTab::ContinueExecution().
        result.release());
  } else {
    int tab_id = dispatcher->GetTabIdFromHandle(selected_tab);
    DCHECK(tab_id != kInvalidChromeSessionId);
    if (result->CreateTabValue(tab_id, -1))
      result->PostResult();
  }
}

HRESULT GetSelectedTab::ContinueExecution(
    const std::string& input_args,
    ApiDispatcher::InvocationResult* user_data,
    ApiDispatcher* dispatcher) {
  DCHECK(dispatcher != NULL);
  DCHECK(user_data != NULL);

  // Any tab is good for us, so relaunch the search for a selected tab
  // by using the frame window of the newly created tab.
  scoped_ptr<TabApiResult> result(static_cast<TabApiResult*>(user_data));
  scoped_ptr<ListValue> args_list;
  DictionaryValue* input_dict =
      api_module_util::GetListAndDictionaryValue(input_args, &args_list);
  if (input_dict == NULL) {
    DCHECK(false) << "Event arguments are not a list with a dictionary in it.";
    result->PostError(api_module_constants::kInternalErrorError);
    return E_INVALIDARG;
  }

  HWND tab_window = NULL;
  HRESULT hr = TabApiResult::IsTabFromSameOrUnspecifiedFrameWindow(
      *input_dict, result->GetValue(ext::kWindowIdKey), &tab_window,
      dispatcher);
  if (FAILED(hr)) {
    // The ApiDispatcher will forget about us and result's destructor will
    // free the allocation of user_data.
    return hr;
  }
  if (hr == S_FALSE) {
    // These are not the droids you are looking for. :-)
    result.release();  // The ApiDispatcher will keep it alive.
    return S_FALSE;
  }

  // We must reset the value and start from scratch in CreateTabValue.
  // TODO(mad@chromium.org): We might be able to save a few steps if
  // we support adding to existing value... Maybe...
  int tab_id = dispatcher->GetTabIdFromHandle(tab_window);
  DCHECK(tab_id != kInvalidChromeSessionId);
  result->set_value(NULL);
  if (result->CreateTabValue(tab_id, -1))
    result->PostResult();
  return S_OK;
}

bool GetAllTabsInWindowResult::Execute(BSTR tab_handles) {
  // This is a list of tab_handles as it comes from the executor, not Chrome.
  DCHECK(tab_handles);
  scoped_ptr<ListValue> tabs_list;
  if (!api_module_util::GetListFromJsonString(CW2A(tab_handles).m_psz,
                                              &tabs_list)) {
    NOTREACHED() << "Invalid tabs list BSTR: " << tab_handles;
    PostError(api_module_constants::kInternalErrorError);
    return false;
  }

  size_t num_values = tabs_list->GetSize();
  if (num_values % 2 != 0) {
    // Values should come in pairs, one for the handle and another one for the
    // index.
    NOTREACHED() << "Invalid tabs list BSTR: " << tab_handles;
    PostError(api_module_constants::kInternalErrorError);
    return false;
  }

  ApiDispatcher* dispatcher = GetDispatcher();
  DCHECK(dispatcher != NULL);

  // This will get populated by the calls to CreateTabValue in the loop below.
  value_.reset(new ListValue());
  num_values /= 2;
  for (size_t index = 0; index < num_values; ++index) {
    int tab_value = 0;
    tabs_list->GetInteger(index * 2, &tab_value);
    int tab_index = -1;
    tabs_list->GetInteger(index * 2 + 1, &tab_index);
    HWND tab_handle = reinterpret_cast<HWND>(tab_value);
    if (!ExecutorsManager::IsKnownWindow(tab_handle))
      continue;
    int tab_id = dispatcher->GetTabIdFromHandle(tab_handle);
    DCHECK(tab_id != kInvalidChromeSessionId);
    if (!CreateTabValue(tab_id, tab_index)) {
      return false;
    }
  }
  return true;
}

void GetAllTabsInWindow::Execute(const ListValue& args, int request_id) {
  scoped_ptr<GetAllTabsInWindowResult> result(CreateApiResult(request_id));
  HWND frame_window = result->GetSpecifiedOrCurrentFrameWindow(args, NULL);
  if (!frame_window) {
    result->PostError(ext::kNoCurrentWindowError);
    return;
  }

  ApiDispatcher* dispatcher = GetDispatcher();
  DCHECK(dispatcher != NULL);
  base::win::ScopedComPtr<ICeeeWindowExecutor> executor;
  dispatcher->GetExecutor(frame_window, IID_ICeeeWindowExecutor,
                          reinterpret_cast<void**>(executor.Receive()));
  if (executor == NULL) {
    LOG(WARNING) << "Failed to get an executor to get list of tabs.";
    result->PostError("Internal Error while getting all tabs in window.");
    return;
  }

  long num_tabs = 0;
  base::win::ScopedBstr tab_handles;
  HRESULT hr = executor->GetTabs(tab_handles.Receive());
  if (FAILED(hr)) {
    DCHECK(tab_handles == NULL);
    LOG(ERROR) << "Failed to get list of tabs for window: " << std::hex <<
        frame_window << ". " << com::LogHr(hr);
    result->PostError("Internal Error while getting all tabs in window.");
    return;
  }

  // Execute posted an error if it returns false.
  if (result->Execute(tab_handles))
    result->PostResult();
}

void UpdateTab::Execute(const ListValue& args, int request_id) {
  scoped_ptr<TabApiResult> result(CreateApiResult(request_id));
  int tab_id = 0;
  if (!args.GetInteger(0, &tab_id)) {
    result->PostError(api_module_constants::kInvalidArgumentsError);
    return;
  }

  ApiDispatcher* dispatcher = GetDispatcher();
  DCHECK(dispatcher != NULL);

  if (!dispatcher->IsTabIdValid(tab_id)) {
    result->PostError(ExtensionErrorUtils::FormatErrorMessage(
        ext::kTabNotFoundError, base::IntToString(tab_id)));
    return;
  }

  HWND tab_window = dispatcher->GetTabHandleFromId(tab_id);
  if (!result->IsTabWindowClass(tab_window)) {
    result->PostError(ExtensionErrorUtils::FormatErrorMessage(
        ext::kTabNotFoundError, base::IntToString(tab_id)));
    return;
  }

  if (window_utils::WindowHasNoThread(tab_window)) {
    result->PostError(ExtensionErrorUtils::FormatErrorMessage(
        ext::kTabNotFoundError, base::IntToString(tab_id)));
    return;
  }

  DictionaryValue* update_props = NULL;
  if (!args.GetDictionary(1, &update_props)) {
    result->PostError(api_module_constants::kInvalidArgumentsError);
    return;
  }

  if (update_props->HasKey(ext::kUrlKey)) {
    std::wstring url;
    if (!update_props->GetString(ext::kUrlKey, &url)) {
      result->PostError(api_module_constants::kInvalidArgumentsError);
      return;
    }

    base::win::ScopedComPtr<ICeeeTabExecutor> executor;
    dispatcher->GetExecutor(tab_window, IID_ICeeeTabExecutor,
                            reinterpret_cast<void**>(executor.Receive()));
    if (executor == NULL) {
      LOG(WARNING) << "Failed to get an executor to navigate tab.";
      result->PostError("Internal error trying to update tab.");
      return;
    }
    HRESULT hr = executor->Navigate(base::win::ScopedBstr(url.c_str()), 0,
                                    base::win::ScopedBstr(L"_top"));
    // Don't DCHECK here, see the comment at the bottom of
    // CeeeExecutor::Navigate().
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to navigate tab: " << std::hex << tab_id <<
          " to " << url << ". " << com::LogHr(hr);
      result->PostError("Internal error trying to update tab.");
      return;
    }
  }

  if (update_props->HasKey(ext::kSelectedKey)) {
    bool selected = false;
    if (!update_props->GetBoolean(ext::kSelectedKey, &selected)) {
      result->PostError(api_module_constants::kInvalidArgumentsError);
      return;
    }

    // We only take action if the user wants to select the tab; this function
    // does not actually let you deselect a tab.
    if (selected) {
      base::win::ScopedComPtr<ICeeeWindowExecutor> frame_executor;
      dispatcher->GetExecutor(
          window_utils::GetTopLevelParent(tab_window), IID_ICeeeWindowExecutor,
          reinterpret_cast<void**>(frame_executor.Receive()));
      if (frame_executor == NULL) {
        LOG(WARNING) << "Failed to get a frame executor to select tab.";
        result->PostError("Internal error trying to select tab.");
        return;
      }
      HRESULT hr = frame_executor->SelectTab(
          reinterpret_cast<CeeeWindowHandle>(tab_window));
      if (FAILED(hr)) {
        LOG(ERROR) << "Failed to select tab: " << std::hex << tab_id << ". " <<
            com::LogHr(hr);
        result->PostError("Internal error trying to select tab.");
        return;
      }
    }
  }
  // TODO(mad@chromium.org): Check if we need to wait for the
  // tabs.onUpdated event to make sure that the update was fully
  // completed (e.g., Navigate above is async).
  if (result->CreateTabValue(tab_id, -1))
    result->PostResult();
}

void RemoveTab::Execute(const ListValue& args, int request_id) {
  scoped_ptr<TabApiResult> result(CreateApiResult(request_id));
  int tab_id;
  if (!args.GetInteger(0, &tab_id)) {
    result->PostError(api_module_constants::kInvalidArgumentsError);
    return;
  }

  ApiDispatcher* dispatcher = GetDispatcher();
  DCHECK(dispatcher != NULL);

  if (!dispatcher->IsTabIdValid(tab_id)) {
    result->PostError(ExtensionErrorUtils::FormatErrorMessage(
        ext::kTabNotFoundError, base::IntToString(tab_id)));
    return;
  }

  HWND tab_window = dispatcher->GetTabHandleFromId(tab_id);
  if (!result->IsTabWindowClass(tab_window)) {
    result->PostError(ExtensionErrorUtils::FormatErrorMessage(
        ext::kTabNotFoundError, base::IntToString(tab_id)));
    return;
  }

  base::win::ScopedComPtr<ICeeeWindowExecutor> frame_executor;
  dispatcher->GetExecutor(window_utils::GetTopLevelParent(tab_window),
                          IID_ICeeeWindowExecutor,
                          reinterpret_cast<void**>(frame_executor.Receive()));
  if (frame_executor == NULL) {
    LOG(WARNING) << "Failed to get a frame executor to select tab.";
    result->PostError("Internal error trying to select tab.");
    return;
  }
  HRESULT hr = frame_executor->RemoveTab(
      reinterpret_cast<CeeeWindowHandle>(tab_window));
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to remove tab: " << std::hex << tab_id << ". " <<
        com::LogHr(hr);
    result->PostError("Internal error trying to remove tab.");
    return;
  }

  // Now we must wait for the tab removal to be completely done before
  // posting the response back to Chrome Frame.
  // And we remember the tab identifier so that we can recognize the event.
  result->SetValue(ext::kTabIdKey, Value::CreateIntegerValue(tab_id));
  dispatcher->RegisterEphemeralEventHandler(ext_event_names::kOnTabRemoved,
                                            RemoveTab::ContinueExecution,
                                            result.release());
}

HRESULT RemoveTab::ContinueExecution(const std::string& input_args,
                                     ApiDispatcher::InvocationResult* user_data,
                                     ApiDispatcher* dispatcher) {
  DCHECK(user_data != NULL);
  DCHECK(dispatcher != NULL);

  scoped_ptr<TabApiResult> result(static_cast<TabApiResult*>(user_data));

  scoped_ptr<ListValue> args_list;
  int tab_id = 0;
  if (!api_module_util::GetListAndIntegerValue(input_args, &args_list,
                                               &tab_id)) {
    NOTREACHED() << "Event arguments are not a list with an integer in it.";
    result->PostError(api_module_constants::kInternalErrorError);
    return E_INVALIDARG;
  }

  const Value* saved_tab_value = result->GetValue(ext::kTabIdKey);
  DCHECK(saved_tab_value != NULL &&
         saved_tab_value->IsType(Value::TYPE_INTEGER));
  int saved_tab_id = 0;
  bool success = saved_tab_value->GetAsInteger(&saved_tab_id);
  DCHECK(success && saved_tab_id != 0);
  if (saved_tab_id == tab_id) {
    // The tabs.remove callback doesn't have any arguments.
    result->set_value(NULL);
    result->PostResult();
    return S_OK;
  } else {
    // release doesn't destroy result, we need to keep it for next try.
    result.release();
    return S_FALSE;  // S_FALSE keeps us in the queue.
  }
}

void CreateTab::Execute(const ListValue& args, int request_id) {
  // TODO(joi@chromium.org) Handle setting remaining tab properties
  // ('title' and 'favIconUrl') if/when CE adds them (this is per a
  // TODO for rafaelw@chromium.org in the extensions code).
  scoped_ptr<TabApiResult> result(CreateApiResult(request_id));
  DictionaryValue* input_dict = NULL;
  if (!args.GetDictionary(0, &input_dict)) {
    result->PostError(api_module_constants::kInvalidArgumentsError);
    return;
  }
  bool specified = false;
  HWND frame_window = result->GetSpecifiedOrCurrentFrameWindow(*input_dict,
                                                               &specified);
  if (!frame_window) {
    result->PostError(ext::kNoCurrentWindowError);
    return;
  }

  ApiDispatcher* dispatcher = GetDispatcher();
  DCHECK(dispatcher != NULL);
  // In case the frame window wasn't specified, we must remember it for later
  // use when we react to events below.
  if (specified) {
    int frame_window_id = dispatcher->GetWindowIdFromHandle(frame_window);
    result->SetValue(
        ext::kWindowIdKey, Value::CreateIntegerValue(frame_window_id));
  }

  std::string url_string(chrome::kAboutBlankURL);  // default if no URL provided
  if (input_dict->HasKey(ext::kUrlKey)) {
    if (!input_dict->GetString(ext::kUrlKey, &url_string)) {
      result->PostError(api_module_constants::kInvalidArgumentsError);
      return;
    }

    GURL url(url_string);
    if (!url.is_valid()) {
      // TODO(joi@chromium.org) See if we can support absolute paths in IE (see
      // extension_tabs_module.cc, AbsolutePath function and its uses)
      result->PostError(ExtensionErrorUtils::FormatErrorMessage(
          ext::kInvalidUrlError, url_string));
      return;
    }
    // Remember the URL, we will use it to recognize the event below.
    result->SetValue(ext::kUrlKey, Value::CreateStringValue(url_string));
  }

  bool selected = true;
  if (input_dict->HasKey(ext::kSelectedKey)) {
    if (!input_dict->GetBoolean(ext::kSelectedKey, &selected)) {
      result->PostError(api_module_constants::kInvalidArgumentsError);
      return;
    }
  }

  if (input_dict->HasKey(ext::kIndexKey)) {
    int index = -1;
    if (!input_dict->GetInteger(ext::kIndexKey, &index)) {
      result->PostError(api_module_constants::kInvalidArgumentsError);
      return;
    }
    result->SetValue(ext::kIndexKey, Value::CreateIntegerValue(index));
  }

  // We will have some work pending, even after we completed the tab creation,
  // because the tab creation itself is asynchronous and we must wait for it
  // to complete before we can post the complete result.

  // UNFORTUNATELY, this scheme doesn't work in protected mode for some reason.
  // So bb2284073 & bb2492252 might still occur there.
  std::wstring url_wstring = UTF8ToWide(url_string);
  if (base::win::GetVersion() < base::win::VERSION_VISTA) {
    base::win::ScopedComPtr<IWebBrowser2> browser;
    HRESULT hr = ie_util::GetWebBrowserForTopLevelIeHwnd(
        frame_window, NULL, browser.Receive());
    DCHECK(SUCCEEDED(hr)) << "Can't get the browser for window: " <<
        frame_window;
    if (FAILED(hr)) {
      result->PostError(api_module_constants::kInternalErrorError);
      return;
    }

    long flags = selected ? navOpenInNewTab : navOpenInBackgroundTab;
    hr = browser->Navigate(
        base::win::ScopedBstr(url_wstring.c_str()),
        const_cast<VARIANT*>(&base::win::ScopedVariant(flags)),
        const_cast<VARIANT*>(&base::win::ScopedVariant(L"_blank")),
        const_cast<VARIANT*>(&base::win::ScopedVariant()),  // Post data
        const_cast<VARIANT*>(&base::win::ScopedVariant()));  // Headers
    DCHECK(SUCCEEDED(hr)) << "Failed to create tab. " << com::LogHr(hr);
    if (FAILED(hr)) {
      result->PostError("Internal error while trying to create tab.");
      return;
    }
  } else {
    // To create a new tab, we find an existing tab in the desired window (there
    // is always at least one), and use it to navigate to a new tab.
    HWND existing_tab = ExecutorsManager::FindTabChild(frame_window);
    DCHECK(existing_tab != NULL) << "Can't find an existing tab for" <<
        frame_window;
    if (existing_tab == NULL) {
      result->PostError("Internal error while trying to create tab.");
      return;
    }

    base::win::ScopedComPtr<ICeeeTabExecutor> executor;
    dispatcher->GetExecutor(existing_tab, IID_ICeeeTabExecutor,
                            reinterpret_cast<void**>(executor.Receive()));
    if (executor == NULL) {
      LOG(WARNING) << "Failed to get an executor to create a tab.";
      result->PostError("Internal error while trying to create tab.");
      return;
    }

    long flags = selected ? navOpenInNewTab : navOpenInBackgroundTab;
    HRESULT hr = executor->Navigate(base::win::ScopedBstr(url_wstring.c_str()),
                                    flags, base::win::ScopedBstr(L"_blank"));
    // We can DCHECK here because navigating to a new tab shouldn't fail as
    // described in the comment at the bottom of CeeeExecutor::Navigate().
    DCHECK(SUCCEEDED(hr)) << "Failed to create tab. " << com::LogHr(hr);
    if (FAILED(hr)) {
      result->PostError("Internal error while trying to create tab.");
      return;
    }
  }

  // And now we must wait for the new tab to be created before we can respond.
  dispatcher->RegisterEphemeralEventHandler(
      ext_event_names::kOnTabCreated,
      CreateTab::ContinueExecution,
      // We don't want to destroy the result in the scoped_ptr when we pass
      // it as user_data to CreateTab::ContinueExecution().
      result.release());
}

HRESULT CreateTab::ContinueExecution(const std::string& input_args,
                                     ApiDispatcher::InvocationResult* user_data,
                                     ApiDispatcher* dispatcher) {
  DCHECK(user_data != NULL);
  DCHECK(dispatcher != NULL);

  scoped_ptr<TabApiResult> result(static_cast<TabApiResult*>(user_data));
  // Check if it has been created with the same info we were created for.
  scoped_ptr<ListValue> args_list;
  DictionaryValue* input_dict =
      api_module_util::GetListAndDictionaryValue(input_args, &args_list);
  if (input_dict == NULL) {
    DCHECK(false) << "Event arguments are not a list with a dictionary in it.";
    result->PostError(api_module_constants::kInvalidArgumentsError);
    return E_INVALIDARG;
  }

  HWND tab_window = NULL;
  HRESULT hr = TabApiResult::IsTabFromSameOrUnspecifiedFrameWindow(
      *input_dict, result->GetValue(ext::kWindowIdKey), &tab_window,
      dispatcher);
  if (FAILED(hr)) {
    // The ApiDispatcher will forget about us and result's destructor will
    // free the allocation of user_data.
    return hr;
  }
  if (hr == S_FALSE) {
    // These are not the droids you are looking for. :-)
    result.release();  // The ApiDispatcher will keep it alive.
    return S_FALSE;
  }

  std::string event_url;
  bool success = input_dict->GetString(ext::kUrlKey, &event_url);
  DCHECK(success) << "The event MUST send a URL!!!";
  // if we didn't specify a URL, we should have navigated to about blank.
  std::string requested_url(chrome::kAboutBlankURL);
  // Ignore failures here, we fall back to the default about blank.
  const Value* url_value = result->GetValue(ext::kUrlKey);
  DCHECK(url_value != NULL && url_value->IsType(Value::TYPE_STRING));
  if (url_value != NULL && url_value->IsType(Value::TYPE_STRING)) {
    bool success = url_value->GetAsString(&requested_url);
    DCHECK(success) << "url_value->GetAsString() Failed!";
  }

  if (GURL(event_url) != GURL(requested_url)) {
    result.release();  // The ApiDispatcher will keep it alive.
    return S_FALSE;
  }

  // We can't rely on selected, since it may have changed if
  // another tab creation was made before we got to broadcast the completion
  // of this one, so we will assume this one is ours.

  // Now move the tab to desired index if specified, we couldn't do it until we
  // had a tab_id.
  long destination_index = -1;
  const Value* index_value = result->GetValue(ext::kIndexKey);
  if (index_value != NULL) {
    DCHECK(index_value->IsType(Value::TYPE_INTEGER));
    int destination_index_int = -1;
    bool success = index_value->GetAsInteger(&destination_index_int);
    DCHECK(success) << "index_value->GetAsInteger()";

    HWND frame_window = window_utils::GetTopLevelParent(tab_window);
    base::win::ScopedComPtr<ICeeeWindowExecutor> frame_executor;
    dispatcher->GetExecutor(frame_window, __uuidof(ICeeeWindowExecutor),
                            reinterpret_cast<void**>(frame_executor.Receive()));
    if (frame_executor == NULL) {
      LOG(WARNING) << "Failed to get an executor for the frame.";
      result->PostError("Internal error while trying to move created tab.");
      return E_UNEXPECTED;
    }

    destination_index = static_cast<long>(destination_index_int);
    HRESULT hr = frame_executor->MoveTab(
        reinterpret_cast<CeeeWindowHandle>(tab_window), destination_index);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to move tab: " << std::hex << tab_window << ". " <<
          com::LogHr(hr);
      result->PostError("Internal error while trying to move created tab.");
      return E_UNEXPECTED;
    }
  }

  // We must reset current state before calling CreateTabValue.
  result->set_value(NULL);
  // TODO(mad@chromium.org): Do we need to go through CreateTabValue?
  // Maybe we already have enough info available to create the
  // response???
  int tab_id = dispatcher->GetTabIdFromHandle(tab_window);
  DCHECK(tab_id != kInvalidChromeSessionId);
  if (result->CreateTabValue(tab_id, destination_index))
    result->PostResult();
  return S_OK;
}

bool CreateTab::EventHandler(const std::string& input_args,
                             std::string* converted_args,
                             ApiDispatcher* dispatcher) {
  DCHECK(converted_args);
  *converted_args = input_args;

  scoped_ptr<ListValue> input_list;
  DictionaryValue* input_dict =
      api_module_util::GetListAndDictionaryValue(input_args, &input_list);
  if (input_dict == NULL) {
    DCHECK(false) << "Input arguments are not a list with a dictionary in it.";
    return false;
  }

  // Check if we got the index, this would mean we already have all we need.
  int int_value = -1;
  if (input_dict->GetInteger(ext::kIndexKey, &int_value)) {
    // We should also have all other non-optional values
    DCHECK(input_dict->GetInteger(ext::kWindowIdKey, &int_value));
    bool bool_value = false;
    DCHECK(input_dict->GetBoolean(ext::kSelectedKey, &bool_value));
    return false;
  }

  // Get the complete tab info from the tab_handle coming from IE.
  // Yes, this is actually a tab handle and not an ID that's in the dict.
  int tab_handle = reinterpret_cast<int>(INVALID_HANDLE_VALUE);
  bool success = input_dict->GetInteger(ext::kIdKey, &tab_handle);
  DCHECK(success) << "Couldn't get the tab ID key from the input args.";
  int tab_id = dispatcher->GetTabIdFromHandle(
      reinterpret_cast<HWND>(tab_handle));
  DCHECK(tab_id != kInvalidChromeSessionId);

  TabApiResult result(TabApiResult::kNoRequestId);
  if (result.CreateTabValue(tab_id, -1)) {
    input_list->Set(0, result.value()->DeepCopy());
    base::JSONWriter::Write(input_list.get(), false, converted_args);
    return true;
  } else {
    // Don't DCHECK, this can happen if we close the window while tabs are
    // being created.
    // TODO(mad@chromium.org): Find a way to DCHECK that the window is
    // actually closing.
    return false;
  }
}

void MoveTab::Execute(const ListValue& args, int request_id) {
  scoped_ptr<TabApiResult> result(CreateApiResult(request_id));
  int tab_id = 0;
  if (!args.GetInteger(0, &tab_id)) {
    result->PostError(api_module_constants::kInvalidArgumentsError);
    return;
  }

  ApiDispatcher* dispatcher = GetDispatcher();
  DCHECK(dispatcher != NULL);

  if (!dispatcher->IsTabIdValid(tab_id)) {
    result->PostError(ExtensionErrorUtils::FormatErrorMessage(
        ext::kTabNotFoundError, base::IntToString(tab_id)));
    return;
  }

  HWND tab_window = dispatcher->GetTabHandleFromId(tab_id);
  if (!result->IsTabWindowClass(tab_window)) {
    result->PostError(ExtensionErrorUtils::FormatErrorMessage(
        ext::kTabNotFoundError, base::IntToString(tab_id)));
    return;
  }

  DictionaryValue* update_props = NULL;
  if (!args.GetDictionary(1, &update_props)) {
    NOTREACHED() << "Can't get update properties from dictionary argument";
    result->PostError(api_module_constants::kInvalidArgumentsError);
    return;
  }

  if (update_props->HasKey(ext::kWindowIdKey)) {
    // TODO(joi@chromium.org) Move to shared constants file
    result->PostError("Moving tabs between windows is not supported.");
    return;
  }

  int new_index = -1;
  if (!update_props->GetInteger(ext::kIndexKey, &new_index)) {
    NOTREACHED() << "Can't get tab index from update properties.";
    result->PostError(api_module_constants::kInvalidArgumentsError);
    return;
  }

  HWND frame_window = window_utils::GetTopLevelParent(tab_window);

  base::win::ScopedComPtr<ICeeeWindowExecutor> frame_executor;
  dispatcher->GetExecutor(frame_window, IID_ICeeeWindowExecutor,
                          reinterpret_cast<void**>(frame_executor.Receive()));
  if (frame_executor == NULL) {
    LOG(WARNING) << "Failed to get an executor for the frame.";
    result->PostError("Internal Error while trying to move tab.");
    return;
  }

  HRESULT hr = frame_executor->MoveTab(
      reinterpret_cast<CeeeWindowHandle>(tab_window), new_index);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to move tab: " << std::hex << tab_id << ". " <<
        com::LogHr(hr);
    result->PostError("Internal Error while trying to move tab.");
    return;
  }
  if (result->CreateTabValue(tab_id, new_index))
    result->PostResult();
}

ApiDispatcher::InvocationResult* TabsInsertCode::ExecuteImpl(
    const ListValue& args,
    int request_id,
    CeeeTabCodeType type,
    int* tab_id,
    HRESULT* hr) {
  scoped_ptr<ApiDispatcher::InvocationResult> result(
      CreateApiResult(request_id));
  // TODO(ericdingle@chromium.org): This needs to support when NULL is
  // sent in as the first parameter.
  if (!args.GetInteger(0, tab_id)) {
    result->PostError(api_module_constants::kInvalidArgumentsError);
    return NULL;
  }

  DictionaryValue* dict;
  if (!args.GetDictionary(1, &dict)) {
    result->PostError(api_module_constants::kInvalidArgumentsError);
    return NULL;
  }

  // The dictionary should have either a code property or a file property,
  // but not both.
  std::string code;
  std::string file;
  if (dict->HasKey(ext::kCodeKey) && dict->HasKey(ext::kFileKey)) {
    result->PostError(ext::kMoreThanOneValuesError);
    return NULL;
  } else if (dict->HasKey(ext::kCodeKey)) {
    dict->GetString(ext::kCodeKey, &code);
  } else if (dict->HasKey(ext::kFileKey)) {
    dict->GetString(ext::kFileKey, &file);
  } else {
    result->PostError(ext::kNoCodeOrFileToExecuteError);
    return NULL;
  }

  // All frames is optional.  If not specified, the default value is false.
  bool all_frames;
  if (!dict->GetBoolean(ext::kAllFramesKey, &all_frames))
    all_frames = false;

  ApiDispatcher* dispatcher = GetDispatcher();
  DCHECK(dispatcher != NULL);

  if (!dispatcher->IsTabIdValid(*tab_id)) {
    result->PostError(ExtensionErrorUtils::FormatErrorMessage(
        ext::kTabNotFoundError, base::IntToString(*tab_id)));
    return NULL;
  }

  HWND tab_window = dispatcher->GetTabHandleFromId(*tab_id);
  if (!TabApiResult::IsTabWindowClass(tab_window)) {
    result->PostError(ExtensionErrorUtils::FormatErrorMessage(
        ext::kTabNotFoundError, base::IntToString(*tab_id)));
    return NULL;
  }

  base::win::ScopedComPtr<ICeeeTabExecutor> tab_executor;
  dispatcher->GetExecutor(tab_window, IID_ICeeeTabExecutor,
                          reinterpret_cast<void**>(tab_executor.Receive()));
  if (tab_executor == NULL) {
    LOG(WARNING) << "Failed to get an executor for the frame.";
    result->PostError("Internal Error while trying to insert code in tab.");
    return NULL;
  }

  *hr = tab_executor->InsertCode(
      base::win::ScopedBstr(ASCIIToWide(code).c_str()),
      base::win::ScopedBstr(ASCIIToWide(file).c_str()), all_frames, type);
  return result.release();
}

void TabsExecuteScript::Execute(const ListValue& args, int request_id) {
  int tab_id;
  HRESULT hr = S_OK;
  scoped_ptr<ApiDispatcher::InvocationResult> result(
      TabsInsertCode::ExecuteImpl(
          args, request_id, kCeeeTabCodeTypeJs, &tab_id, &hr));
  if (result.get() == NULL)
    return;

  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to execute script in tab: " <<
                  std::hex <<
                  tab_id <<
                  ". " <<
                  com::LogHr(hr);
    result->PostError("Internal Error while trying to execute script in tab.");
  } else {
    result->PostResult();
  }
}

void TabsInsertCSS::Execute(const ListValue& args, int request_id) {
  int tab_id;
  HRESULT hr = S_OK;
  scoped_ptr<ApiDispatcher::InvocationResult> result(
      TabsInsertCode::ExecuteImpl(
          args, request_id, kCeeeTabCodeTypeCss, &tab_id, &hr));
  if (result.get() == NULL)
    return;

  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to insert CSS in tab: " <<
                  std::hex <<
                  tab_id <<
                  ". " <<
                  com::LogHr(hr);
    result->PostError("Internal Error while trying to insert CSS in tab.");
  } else {
    result->PostResult();
  }
}

TabInfo::TabInfo() {
  url = NULL;
  title = NULL;
  status = kCeeeTabStatusLoading;
  fav_icon_url = NULL;
  protected_mode = FALSE;
}

TabInfo::~TabInfo() {
  Clear();
}

void TabInfo::Clear() {
  // SysFreeString accepts NULL pointers.
  ::SysFreeString(url);
  url = NULL;
  ::SysFreeString(title);
  title = NULL;
  ::SysFreeString(fav_icon_url);
  fav_icon_url = NULL;

  status = kCeeeTabStatusLoading;
  protected_mode = FALSE;
}

}  // namespace tab_api
