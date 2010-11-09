// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Infobar API implementation.

#include "ceee/ie/broker/infobar_api_module.h"

#include <atlbase.h>
#include <atlcom.h>  // Must be included AFTER base.

#include "base/string_number_conversions.h"
#include "ceee/common/com_utils.h"
#include "ceee/ie/broker/api_module_constants.h"
#include "ceee/ie/common/api_registration.h"
#include "chrome/browser/extensions/extension_infobar_module_constants.h"
#include "chrome/common/extensions/extension_error_utils.h"

namespace ext = extension_infobar_module_constants;

namespace infobar_api {

const char kOnDocumentCompleteEventName[] = "infobar.onDocumentComplete";

void RegisterInvocations(ApiDispatcher* dispatcher) {
#define REGISTER_API_FUNCTION(func) do { dispatcher->RegisterInvocation(\
    func##Function::function_name(), NewApiInvocation< func >); } while (false)
  REGISTER_INFOBAR_API_FUNCTIONS();
#undef REGISTER_API_FUNCTION
}

void ShowInfoBar::Execute(const ListValue& args, int request_id) {
  scoped_ptr<InfobarApiResult> result(CreateApiResult(request_id));

  // Get the first parameter (object details).
  DictionaryValue* dict;
  if (!args.GetDictionary(0, &dict)) {
    result->PostError(api_module_constants::kInvalidArgumentsError);
    return;
  }

  // The dictionary should have both tabId and path properties.
  int tab_id;
  std::string path;
  if (!dict->GetInteger(ext::kTabId, &tab_id) ||
      !dict->GetString(ext::kHtmlPath, &path)) {
    result->PostError(api_module_constants::kInvalidArgumentsError);
    return;
  }

  ApiDispatcher* dispatcher = GetDispatcher();
  DCHECK(dispatcher != NULL);
  HWND tab_window = dispatcher->GetTabHandleFromId(tab_id);
  if (!result->IsTabWindowClass(tab_window)) {
    result->PostError(ExtensionErrorUtils::FormatErrorMessage(
        ext::kTabNotFoundError, base::IntToString(tab_id)).c_str());
    return;
  }

  CComPtr<ICeeeInfobarExecutor> executor;
  GetDispatcher()->GetExecutor(tab_window, IID_ICeeeInfobarExecutor,
                               reinterpret_cast<void**>(&executor));
  if (executor == NULL) {
    LOG(WARNING) << "Failed to get an executor to show infobar.";
    result->PostError(api_module_constants::kInternalErrorError);
    return;
  }
  CeeeWindowHandle window_handle;
  HRESULT hr = executor->ShowInfobar(CComBSTR(path.data()), &window_handle);
  if (FAILED(hr)) {
    // No DCHECK, this may happen if the window/thread dies on the way.
    LOG(ERROR) << "Can't show infobar. " << com::LogHr(hr) << " path=" << path;
    result->PostError(api_module_constants::kInternalErrorError);
    return;
  }

  // If we created a window then we have to wait until the document is complete
  // before responding. If window is not created then we can post result now.
  if (0 == window_handle) {
    result->PostResult();
  } else {
    // Store the window information returned by ShowInfobar().
    result->CreateWindowValue(dispatcher->GetWindowHandleFromId(window_handle),
        false);

    dispatcher->RegisterEphemeralEventHandler(kOnDocumentCompleteEventName,
        ShowInfoBar::ContinueExecution, result.release());
  }
}

HRESULT ShowInfoBar::ContinueExecution(
    const std::string& input_args,
    ApiDispatcher::InvocationResult* user_data,
    ApiDispatcher* dispatcher) {
  DCHECK(user_data != NULL);
  DCHECK(dispatcher != NULL);

  scoped_ptr<InfobarApiResult> result(
      static_cast<InfobarApiResult*>(user_data));
  result->PostResult();
  return S_OK;
}

}  // namespace infobar_api
