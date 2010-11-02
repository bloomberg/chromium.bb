// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implementations for common functions for various API implementation modules.

#include "ceee/ie/broker/common_api_module.h"

#include <atlbase.h>
#include <atlcom.h>  // Must be included AFTER base.

#include "base/string_number_conversions.h"
#include "ceee/ie/broker/api_module_constants.h"
#include "ceee/ie/broker/tab_api_module.h"
#include "ceee/ie/common/ie_util.h"
#include "chrome/browser/extensions/extension_tabs_module_constants.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "ceee/common/com_utils.h"
#include "ceee/common/window_utils.h"
#include "ceee/common/windows_constants.h"

namespace ext = extension_tabs_module_constants;

namespace common_api {

bool CommonApiResult::IsTabWindowClass(HWND window) {
  return window_utils::IsWindowClass(window, windows::kIeTabWindowClass);
}

bool CommonApiResult::IsIeFrameClass(HWND window) {
  return window_utils::IsWindowClass(window, windows::kIeFrameWindowClass);
}

HWND CommonApiResult::TopIeWindow() {
  HWND window = NULL;
  if (window_utils::FindDescendentWindow(NULL, windows::kIeFrameWindowClass,
                                         true, &window)) {
    return window;
  }
  NOTREACHED() << "How come we can't find a Top IE Window???";
  return NULL;
}

void CommonApiResult::SetResultFromWindowInfo(
    HWND window, const CeeeWindowInfo& window_info, bool populate_tabs) {
  scoped_ptr<DictionaryValue> dict(new DictionaryValue());
  ApiDispatcher* dispatcher = GetDispatcher();
  DCHECK(dispatcher != NULL);
  int window_id = dispatcher->GetWindowIdFromHandle(window);
  dict->SetInteger(ext::kIdKey, window_id);
  dict->SetBoolean(ext::kFocusedKey, window_info.focused != FALSE);

  dict->SetInteger(ext::kLeftKey, window_info.rect.left);
  dict->SetInteger(ext::kTopKey, window_info.rect.top);
  dict->SetInteger(ext::kWidthKey, window_info.rect.right -
                                   window_info.rect.left);
  dict->SetInteger(ext::kHeightKey, window_info.rect.bottom -
                                    window_info.rect.top);
  if (populate_tabs) {
    DCHECK(window_info.tab_list != NULL);
    Value* tab_list_value = CreateTabList(window_info.tab_list);
    // DCHECK yet continue if we get a NULL tab list since we may get here
    // before it is available.
    DCHECK(tab_list_value != NULL);
    if (!tab_list_value)
      tab_list_value = Value::CreateNullValue();
    dict->Set(ext::kTabsKey, tab_list_value);
  }

  dict->SetBoolean(ext::kIncognitoKey, ie_util::GetIEIsInPrivateBrowsing());

  // TODO(mad@chromium.org): for now, always setting to "normal" since we don't
  // yet have a way to tell if the window is a popup or not.
  dict->SetString(ext::kWindowTypeKey, ext::kWindowTypeValueNormal);

  DCHECK(value_ == NULL);
  value_.reset(dict.release());
}

Value* CommonApiResult::CreateTabList(BSTR tab_list) {
  // No need for a request_id.
  tab_api::GetAllTabsInWindowResult result(kNoRequestId);
  result.Execute(tab_list);
  const Value* tab_list_value = result.value();
  // No DCHECK as this will happen if we try to update a window
  // that has been closed.
  if (tab_list_value) {
    return tab_list_value->DeepCopy();
  }
  LOG(WARNING) << "Can't get info for tab_ids: '" << tab_list << "'.";
  return NULL;
}

bool CommonApiResult::CreateWindowValue(HWND window, bool populate_tabs) {
  ApiDispatcher* dispatcher = GetDispatcher();
  DCHECK(dispatcher != NULL);
  int window_id = dispatcher->GetWindowIdFromHandle(window);
  if (window_utils::WindowHasNoThread(window)) {
    PostError(ExtensionErrorUtils::FormatErrorMessage(
        ext::kWindowNotFoundError, base::IntToString(window_id)));
    return false;
  }

  if (!IsIeFrameClass(window)) {
    PostError(ExtensionErrorUtils::FormatErrorMessage(
        ext::kWindowNotFoundError, base::IntToString(window_id)));
    return false;
  }

  CComPtr<ICeeeWindowExecutor> executor;
  dispatcher->GetExecutor(window, IID_ICeeeWindowExecutor,
                          reinterpret_cast<void**>(&executor));
  if (executor == NULL) {
    LOG(WARNING) << "Failed to get an executor to get window info.";
    PostError(api_module_constants::kInternalErrorError);
    return false;
  }

  common_api::WindowInfo window_info;
  HRESULT hr = executor->GetWindow(populate_tabs, &window_info);
  if (FAILED(hr)) {
    // No DCHECK, this may happen if the window/thread dies on the way.
    LOG(ERROR) << "Can't get info for window: " << std::hex << window <<
        ". " << com::LogHr(hr);
    PostError(api_module_constants::kInternalErrorError);
    return false;
  }
  SetResultFromWindowInfo(window, window_info, populate_tabs);
  return true;
}

WindowInfo::WindowInfo() {
  focused = FALSE;
  rect.bottom = -1;
  rect.top = -1;
  rect.left = -1;
  rect.right = -1;
  tab_list = NULL;
}

WindowInfo::~WindowInfo() {
  // SysFreeString accepts NULL pointers.
  ::SysFreeString(tab_list);
}

}  // namespace common_api
