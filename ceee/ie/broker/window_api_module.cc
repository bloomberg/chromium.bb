// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Window API implementation.
//
// Window IDs are the HWND of the top-level frame window of IE.

#include "ceee/ie/broker/window_api_module.h"

#include <atlbase.h>
#include <iepmapi.h>
#include <sddl.h>
#include <set>

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/scoped_comptr_win.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/win/scoped_bstr.h"
#include "ceee/common/com_utils.h"
#include "ceee/common/process_utils_win.h"
#include "ceee/common/windows_constants.h"
#include "ceee/common/window_utils.h"
#include "ceee/ie/broker/api_module_constants.h"
#include "ceee/ie/broker/api_module_util.h"
#include "ceee/ie/broker/tab_api_module.h"
#include "ceee/ie/common/ie_util.h"
#include "ceee/ie/common/api_registration.h"
#include "chrome/browser/extensions/extension_event_names.h"
#include "chrome/browser/extensions/extension_tabs_module_constants.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "googleurl/src/gurl.h"

namespace ext = extension_tabs_module_constants;
namespace ext_event_names = extension_event_names;

namespace window_api {

// We sometimes need to wait for window creation and removal to be completed
// even though we receive an event about them, these events sometimes arrive
// before the creation/removal is really totally completed.
const int kMaxDelayMs = 5000;  // 5 seconds may be needed on slow machines.
const int kDelayMs = 50;

void RegisterInvocations(ApiDispatcher* dispatcher) {
#define REGISTER_API_FUNCTION(func) do { dispatcher->RegisterInvocation(\
    func##Function::function_name(), NewApiInvocation< func >); } while (false)
  REGISTER_WINDOW_API_FUNCTIONS();
#undef REGISTER_API_FUNCTION
  dispatcher->RegisterInvocation(CreateWindowFunction::function_name(),
                                 NewApiInvocation< CreateWindowFunc >);
  // And now register the permanent event handlers.
  dispatcher->RegisterPermanentEventHandler(ext_event_names::kOnWindowRemoved,
                                            RemoveWindow::EventHandler);
  dispatcher->RegisterPermanentEventHandler(ext_event_names::kOnWindowCreated,
                                            CreateWindowFunc::EventHandler);
}

bool WindowApiResult::UpdateWindowRect(HWND window,
                                       const DictionaryValue* window_props) {
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

  // Unspecified entries are set to -1 to let the executor know not to change
  // them.
  long left = -1;
  long top = -1;
  long width = -1;
  long height = -1;

  if (window_props) {
    int dim;
    if (window_props->HasKey(ext::kLeftKey)) {
      if (!window_props->GetInteger(ext::kLeftKey, &dim)) {
        PostError("bad request");
        return false;
      }
      left = dim;
    }

    if (window_props->HasKey(ext::kTopKey)) {
      if (!window_props->GetInteger(ext::kTopKey, &dim)) {
        PostError("bad request");
        return false;
      }
      top = dim;
    }

    if (window_props->HasKey(ext::kWidthKey)) {
      if (!window_props->GetInteger(ext::kWidthKey, &dim) || dim < 0) {
        PostError("bad request");
        return false;
      }
      width = dim;
    }

    if (window_props->HasKey(ext::kHeightKey)) {
      if (!window_props->GetInteger(ext::kHeightKey, &dim) || dim < 0) {
        PostError("bad request");
        return false;
      }
      height = dim;
    }
  }

  common_api::WindowInfo window_info;
  if (left != -1 || top != -1 || width  != -1 || height != -1) {
    ScopedComPtr<ICeeeWindowExecutor> executor;
    dispatcher->GetExecutor(window, IID_ICeeeWindowExecutor,
                            reinterpret_cast<void**>(executor.Receive()));
    if (executor == NULL) {
      LOG(WARNING) << "Failed to get an executor to update window.";
      PostError(api_module_constants::kInternalErrorError);
      return false;
    }

    HRESULT hr = executor->UpdateWindow(left, top, width, height, &window_info);
    if (FAILED(hr)) {
      LOG(ERROR) << "Couldn't update window: " << std::hex << window << ". " <<
          com::LogHr(hr);
      PostError(api_module_constants::kInternalErrorError);
      return false;
    }
    SetResultFromWindowInfo(window, window_info, false);
    return true;
  } else {
    return CreateWindowValue(window, false);
  }
}

void GetWindow::Execute(const ListValue& args, int request_id) {
  scoped_ptr<WindowApiResult> result(CreateApiResult(request_id));
  int window_id = 0;
  if (!args.GetInteger(0, &window_id)) {
    NOTREACHED() << "bad message";
    result->PostError(api_module_constants::kInvalidArgumentsError);
    return;
  }

  // CreateWindowValue validates the HWND.
  ApiDispatcher* dispatcher = GetDispatcher();
  DCHECK(dispatcher != NULL);
  HWND window = dispatcher->GetWindowHandleFromId(window_id);
  if (result->CreateWindowValue(window, false))
    result->PostResult();
}

void GetCurrentWindow::Execute(const ListValue& args, int request_id,
                               const DictionaryValue* associated_tab) {
  scoped_ptr<WindowApiResult> result(CreateApiResult(request_id));
  if (associated_tab == NULL) {
    // The associated tab may validly be NULL, for instance if the API call
    // originated from the background page. If this happens, simply return
    // the top-level window instead.
    if (result->CreateWindowValue(result->TopIeWindow(), false))
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
  // The window ID is just the window handle of the frame window, which is the
  // top-level ancestor of this window.
  HWND frame_window = window_utils::GetTopLevelParent(tab_window);
  if (frame_window == tab_window ||
      !result->IsTabWindowClass(tab_window) ||
      !result->IsIeFrameClass(frame_window)) {
    // If we couldn't get a valid parent frame window, then it must be because
    // the frame window (and the tab then) has been closed by now or it lives
    // under the hidden IE window.
    DCHECK(!::IsWindow(tab_window) || window_utils::IsWindowClass(frame_window,
        windows::kHiddenIeFrameWindowClass));
    int tab_id = dispatcher->GetTabIdFromHandle(tab_window);
    result->PostError(ExtensionErrorUtils::FormatErrorMessage(
        ext::kTabNotFoundError, base::IntToString(tab_id)));
    return;
  }

  if (result->CreateWindowValue(frame_window, false))
    result->PostResult();
}

void GetLastFocusedWindow::Execute(const ListValue& args, int request_id) {
  scoped_ptr<WindowApiResult> result(CreateApiResult(request_id));
  if (result->CreateWindowValue(result->TopIeWindow(), false))
    result->PostResult();
}

bool CreateWindowFunc::EventHandler(const std::string& input_args,
                                    std::string* converted_args,
                                    ApiDispatcher* dispatcher) {
  DCHECK(converted_args);
  // We don't need to modify anything in the arguments, we just want to delay.
  *converted_args = input_args;

  scoped_ptr<ListValue> args_list;
  int window_id = 0;
  if (!api_module_util::GetListAndDictIntValue(
      input_args, ext::kIdKey, &args_list, &window_id)) {
    NOTREACHED() << "Event arguments wasn't a dictionary with an ID in it. " <<
        input_args;
    return false;
  }

  // The hook may call us before the window is completely created, so we
  // must delay the execution until the first tab is completely created.
  // TODO(mad@chromium.org): Find a way to do this without blocking
  // all other ApiDispatching.
  HWND window = dispatcher->GetWindowHandleFromId(window_id);
  int waited_ms = 0;
  while (waited_ms < kMaxDelayMs &&
         ::IsWindow(window) &&
         !window_utils::FindDescendentWindow(
             window, windows::kIeTabWindowClass, false, NULL)) {
    ::SleepEx(kDelayMs, TRUE);  // TRUE = Alertable.
    waited_ms += kDelayMs;
  }
  // We don't DCHECK if the window died, but we must still return false
  // if it did, so that we don't broadcast the event back to Chrome.
  DCHECK(waited_ms < kMaxDelayMs || !::IsWindow(window));
  return waited_ms < kMaxDelayMs && ::IsWindow(window);
}

void CreateWindowFunc::Execute(const ListValue& args, int request_id) {
  scoped_ptr<WindowApiResult> result(CreateApiResult(request_id));
  scoped_ptr<DictionaryValue> input_dict;
  // The input dictionary is optional; if not provided, the args may be
  // either an empty list or a list with a single null value element.
  if (args.GetSize() > 0) {
    Value* first_arg = NULL;
    if (!args.Get(0, &first_arg) || first_arg == NULL) {
      NOTREACHED() << "bad request";
      result->PostError(api_module_constants::kInvalidArgumentsError);
      return;
    }
    if (first_arg->GetType() != Value::TYPE_NULL) {
      DictionaryValue* args_dict = NULL;
      if (!args.GetDictionary(0, &args_dict) || args_dict == NULL) {
        NOTREACHED() << "bad request";
        result->PostError(api_module_constants::kInvalidArgumentsError);
        return;
      }
      // Remember the arguments so that we can use them later.
      input_dict.reset(static_cast<DictionaryValue*>(args_dict->DeepCopy()));
    }
  }

  // Look for optional url.
  scoped_ptr<GURL> spec(new GURL(chrome::kAboutBlankURL));
  std::string url_input;
  if (input_dict.get() != NULL) {
    if (input_dict->HasKey(ext::kUrlKey)) {
      if (!input_dict->GetString(ext::kUrlKey, &url_input)) {
        NOTREACHED() << "bad request";
        result->PostError(api_module_constants::kInvalidArgumentsError);
        return;
      }

      spec.reset(new GURL(url_input));
      if (!spec->is_valid()) {
        result->PostError(ExtensionErrorUtils::FormatErrorMessage(
            ext::kInvalidUrlError, url_input).c_str());
        return;
      }
    }
  }

  // There are many ways to create new IE windows, but they don't all behave
  // the same depending on the IE version, OS or even settings. The most
  // reliable way we found was to CoCreate an instance of IE, but navigating
  // this instance doesn't behave exactly the same on all platforms.
  //
  // The main problem is with protected mode. If an instance of IE is launched
  // with protected mode on, and we navigate to a URL that doesn't need
  // protection (or vice versa), then the navigation will need to occur in
  // another browser, not the one that was launched in the inappropriate
  // protected mode.
  //
  // On IE7 if you CoCreate an instance of IE from a process running at an
  // elevated integrity level as our Broker is, you get an unprotected mode
  // IE, and if you navigate to a URL that needs protected mode, it will either
  // create a new IE with the proper protected mode, or re-use an existing one
  // if one is already opened.
  //
  // On IE8 the current process' integrity level is not taken into account
  // when CoCreating a new IE, it relies on the CLSID which is the regular one
  // (CLSID_InternetExplorer) for protected mode and a new one used for running
  // at medium integrity level (CLSID_InternetExplorerMedium). Also, if you
  // would then navigate to a URL that has a different protected mode than the
  // one used for the CoCreate, then, a new window will always be created, an
  // existing one will never be used.
  //
  // The other alternatives we looked at was to
  // 1) Explicitly take the IWebBrowser2 interface of an existing IE.
  //    But this can cause the navigate to create a new tab instead
  //    of a new window (even if we specified navOpenInNewWindow) if
  //    the tab settings specify that popups should be opened in a
  //    new tab as opposed to a new window.
  // 2) Use the IELaunchURL API (available in iepmapi.h).
  //    Except from the fact that it always creates a set of new
  //    IE processes in their own individual session this would
  //    behave exactly as we would like... But the price is too high.
  //    And it isn't available on XP...
  // 3) Still use CoCreateInstance but don't rely on the knowledge of the
  //    current OS or IE version but rely on notifications sent to the
  //    DWebBrowser2 events thingy. Experimenting with these was not trivial
  //    and in some case lead to a non-deterministic way of identifying if
  //    a new window had been created or not.

  // We need to know whether we will navigate to a URL that needs protected
  // mode enabled or not. On earlier versions of the OS (e.g., pre-Vista),
  // this call simply fails and thus act as if we didn't need protected mode.
  std::wstring url = UTF8ToWide(spec->spec());
  HRESULT protected_mode_url_hr = ::IEIsProtectedModeURL(url.c_str());

  // We default to CLSID_InternetExplorer and CLSCTX_ALL but we may need to
  // switch to CLSID_InternetExplorerMedium or add CLSCTX_ENABLE_CLOAKING later.
  DWORD class_context = CLSCTX_ALL;
  CLSID ie_clsid = CLSID_InternetExplorer;
  bool lowered_integrity_level = false;
  bool impersonating = false;
  if (protected_mode_url_hr != S_OK) {  // S_FALSE is returned for no.
    // When we don't need protected mode, we need to explicitly
    // request that IE8 starts the medium integrity version.
    if (ie_util::GetIeVersion() == ie_util::IEVERSION_IE8)
      ie_clsid = CLSID_InternetExplorerMedium;
  } else if (ie_util::GetIeVersion() == ie_util::IEVERSION_IE7) {
    // IE7 in protected mode needs to be started at lower integrity level
    // than the broker process. So we must enable cloaking and temporary bring
    // down the integrity level of this thread.
    class_context |= CLSCTX_ENABLE_CLOAKING;
    // We would have liked to use the CAccessToken class from ATL but it doesn't
    // support the integrity level impersonation, just the owner, primary group
    // and DACL. So we do it ourselves.
    if (!::ImpersonateSelf(SecurityImpersonation)) {
      DCHECK(false) << com::LogWe();
      return;
    }
    // Remember that we successfully impersonated, so we can RevertToSelf.
    impersonating = true;
    // This call fails on XP, so we don't look at the error, we just log it and
    // remember our success so we can revert it later. Specifying NULL for the
    // thread pointer means that we want to affect the current thread.
    HRESULT hr_lowered_integrity_level =
        process_utils_win::SetThreadIntegrityLevel(NULL, SDDL_ML_LOW);
    lowered_integrity_level = SUCCEEDED(hr_lowered_integrity_level);
    DLOG_IF(WARNING, !lowered_integrity_level) <<
        "SetThreadIntegrityLevelLow" << com::LogHr(hr_lowered_integrity_level);
  }

  // Now we can create a new web browser and be sure it will be the one that is
  // kept (as well as its window) once we navigate.
  CComPtr<IWebBrowser2> web_browser;
  HRESULT hr = web_browser.CoCreateInstance(ie_clsid, NULL, class_context);
  DCHECK(SUCCEEDED(hr)) << "Can't CoCreate IE! " << com::LogHr(hr);
  if (FAILED(hr)) {
    result->PostError(api_module_constants::kInternalErrorError);
    return;
  }

  // And now we can bring back the integrity level to where it was.
  if (lowered_integrity_level) {
    HRESULT hr_integrity_level =
        process_utils_win::ResetThreadIntegrityLevel(NULL);
    DCHECK(SUCCEEDED(hr_integrity_level)) << "Failed to bring back thread " <<
        "integrity level! " << com::LogHr(hr_integrity_level);
    LOG_IF(WARNING, FAILED(hr)) << "ResetThreadIntegrityLevel(NULL) " <<
        com::LogHr(hr_integrity_level);
  }

  // And stop impersonating.
  if (impersonating) {
    BOOL success = ::RevertToSelf();
    DCHECK(success) << "Failed to stop impersonating! " << com::LogWe();
    LOG_IF(WARNING, !success) << "RevertToSelf() " << com::LogWe();
  }

  // We need the HWND to create the window value to fill the info needed
  // by the callback, and also, to potentially resize and position it.
  HWND web_browserhwnd = NULL;
  hr = web_browser->get_HWND(reinterpret_cast<SHANDLE_PTR*>(&web_browserhwnd));
  DCHECK(SUCCEEDED(hr)) << "Can't get HWND!" << com::LogHr(hr);
  if (FAILED(hr)) {
    result->PostError(api_module_constants::kInternalErrorError);
    return;
  }

  if (input_dict.get() == NULL) {
    // No arguments, so no need to popupize, resize or reposition.
    if (!result->CreateWindowValue(web_browserhwnd, false)) {
      // CreateWindowValue will have posted the error if any.
      return;
    }
  } else {
    // Popupize if needed.
    std::string window_type;
    if (input_dict->GetString(ext::kWindowTypeKey, &window_type) &&
        window_type == ext::kWindowTypeValuePopup) {
      HRESULT hr = web_browser->put_AddressBar(VARIANT_FALSE);
      DCHECK(SUCCEEDED(hr)) << "Failed to hide address bar. " << com::LogHr(hr);
      hr = web_browser->put_StatusBar(VARIANT_FALSE);
      DCHECK(SUCCEEDED(hr)) << "Failed to hide status bar. " << com::LogHr(hr);
      hr = web_browser->put_ToolBar(FALSE);
      DCHECK(SUCCEEDED(hr)) << "Failed put_ToolBar. " << com::LogHr(hr);
    }
    // Reposition and Resize if needed.
    if (!result->UpdateWindowRect(web_browserhwnd, input_dict.get())) {
      // UpdateWindowRect will have posted the error if any.
      return;
    }
  }

  // Now we can Navigate to the requested url.
  hr = web_browser->Navigate(base::win::ScopedBstr(url.c_str()),
                             &CComVariant(),  // unused flags
                             &CComVariant(L"_top"),  // Target frame
                             &CComVariant(),  // Unused POST DATA
                             &CComVariant());  // Unused Headers
  DCHECK(SUCCEEDED(hr)) << "Can't Navigate IE to " << url << com::LogHr(hr);
  if (FAILED(hr)) {
    result->PostError(api_module_constants::kInternalErrorError);
    return;
  }

  // A CoCreated IE is not visible until we ask it to be.
  hr = web_browser->put_Visible(VARIANT_TRUE);
  DCHECK(SUCCEEDED(hr)) << "put_Visible: " << com::LogHr(hr);
  if (FAILED(hr)) {
    result->PostError(api_module_constants::kInternalErrorError);
    return;
  }
  result->PostResult();
}

void UpdateWindow::Execute(const ListValue& args, int request_id) {
  scoped_ptr<WindowApiResult> result(CreateApiResult(request_id));
  int window_id = 0;
  DictionaryValue* update_props;
  if (!args.GetInteger(0, &window_id) ||
      !args.GetDictionary(1, &update_props)) {
    NOTREACHED() << "bad message";
    result->PostError(api_module_constants::kInvalidArgumentsError);
    return;
  }

  ApiDispatcher* dispatcher = GetDispatcher();
  DCHECK(dispatcher != NULL);
  HWND window = dispatcher->GetWindowHandleFromId(window_id);
  if (!result->IsIeFrameClass(window)) {
    LOG(WARNING) << "Extension trying to access non-IE or dying window: " <<
        std::hex << window_id;
    result->PostError(ExtensionErrorUtils::FormatErrorMessage(
        ext::kWindowNotFoundError, base::IntToString(window_id)).c_str());
    return;
  }

  if (result->UpdateWindowRect(window, update_props))
    result->PostResult();
}

void RemoveWindow::Execute(const ListValue& args, int request_id) {
  scoped_ptr<WindowApiResult> result(CreateApiResult(request_id));
  int window_id = 0;
  if (!args.GetInteger(0, &window_id)) {
    NOTREACHED() << "bad message";
    result->PostError(api_module_constants::kInvalidArgumentsError);
    return;
  }

  ApiDispatcher* dispatcher = GetDispatcher();
  DCHECK(dispatcher != NULL);
  HWND window = dispatcher->GetWindowHandleFromId(window_id);
  if (!result->IsIeFrameClass(window)) {
    LOG(WARNING) << "Extension trying to access non-IE or dying window: " <<
        std::hex << window_id;
    result->PostError(ExtensionErrorUtils::FormatErrorMessage(
        ext::kWindowNotFoundError, base::IntToString(window_id)));
    return;
  }

  ScopedComPtr<ICeeeWindowExecutor> executor;
  dispatcher->GetExecutor(window, IID_ICeeeWindowExecutor,
                          reinterpret_cast<void**>(executor.Receive()));
  if (executor == NULL) {
    LOG(WARNING) << "Failed to get an executor to remove window.";
    result->PostError(api_module_constants::kInternalErrorError);
    return;
  }

  HRESULT hr = executor->RemoveWindow();
  if (FAILED(hr)) {
    LOG(ERROR) << "Executor failed to remove window: " << std::hex <<
        window_id << ". " << com::LogHr(hr);
    result->PostError("Internal Error trying to close window.");
    return;
  }

  // S_FALSE is returned when there are no tabs to close, e.g., the initial
  // tab has not finished loading by the time we are asked to close the window.
  if (hr == S_FALSE) {
    LOG(WARNING) << "Failed to get window manager to close the window, "
        "trying WM_CLOSE instead." << com::LogHr(hr);
    // We fall back to this to be slightly more robust, but this approach has
    // the drawback that it shows a pop-up dialog with a message like "do you
    // want to close N tabs" if there is more than one tab. And we may need
    // to try a few times because we have seen cases where SendMessage didn't
    // return 0 because the window couldn't process the message for some reason.
    int waited_ms = 0;
    while (waited_ms < kMaxDelayMs && ::SendMessage(window, WM_CLOSE, 0, 0)) {
      ::SleepEx(kDelayMs, TRUE);  // Alertable.
      waited_ms += kDelayMs;
    }

    DCHECK(waited_ms < kMaxDelayMs);
    if (waited_ms >= kMaxDelayMs) {
      result->PostError(api_module_constants::kInternalErrorError);
      return;
    }
  }

  // Now we must wait for the window removal to be completely done before
  // posting the response back to Chrome Frame.
  // And we remember the window identifier so that we can recognize the event.
  result->SetValue(ext::kWindowIdKey, Value::CreateIntegerValue(window_id));
  DCHECK(dispatcher != NULL);
  dispatcher->RegisterEphemeralEventHandler(ext_event_names::kOnWindowRemoved,
                                            RemoveWindow::ContinueExecution,
                                            result.release());
}

HRESULT RemoveWindow::ContinueExecution(
    const std::string& input_args,
    ApiDispatcher::InvocationResult* user_data,
    ApiDispatcher* dispatcher) {
  DCHECK(dispatcher != NULL);
  DCHECK(user_data != NULL);

  scoped_ptr<WindowApiResult> result(static_cast<WindowApiResult*>(user_data));

  scoped_ptr<ListValue> args_list;
  int window_id = 0;
  if (!api_module_util::GetListAndIntegerValue(input_args, &args_list,
                                               &window_id)) {
    NOTREACHED() << "Event arguments are not a list with an integer in it.";
    result->PostError(api_module_constants::kInternalErrorError);
    return E_INVALIDARG;
  }

  const Value* saved_window_value = result->GetValue(ext::kWindowIdKey);
  DCHECK(saved_window_value != NULL &&
         saved_window_value->IsType(Value::TYPE_INTEGER));
  int saved_window_id = 0;
  bool success = saved_window_value->GetAsInteger(&saved_window_id);
  DCHECK(success && saved_window_id != 0);
  if (saved_window_id == window_id) {
    // The windows.remove callback doesn't have any arguments.
    result->set_value(NULL);
    result->PostResult();
    return S_OK;
  } else {
    // release doesn't destroy result, we need to keep it for next try.
    result.release();
    return S_FALSE;  // S_FALSE keeps us in the queue.
  }
}

bool RemoveWindow::EventHandler(const std::string& input_args,
                                std::string* converted_args,
                                ApiDispatcher* dispatcher) {
  DCHECK(converted_args);
  // We don't need to modify anything in the arguments, we just want to delay.
  *converted_args = input_args;

  scoped_ptr<Value> input_args_val(base::JSONReader::Read(input_args, true));
  DCHECK(input_args_val.get() != NULL &&
         input_args_val->IsType(Value::TYPE_LIST));
  if (input_args_val.get() == NULL || !input_args_val->IsType(Value::TYPE_LIST))
    return false;

  int window_id = 0;
  bool success = static_cast<ListValue*>(input_args_val.get())->GetInteger(
      0, &window_id);
  DCHECK(success) << "Couldn't get an int window Id from input args.";
  if (!success)
    return false;
  // The hook may call us before the window has completely disappeared so we
  // must delay the execution until the window is completely gone.
  // TODO(mad@chromium.org): Find a way to do this without blocking
  // all other ApiDispatching.
  HWND window = dispatcher->GetWindowHandleFromId(window_id);
  int waited_ms = 0;
  while (waited_ms < kMaxDelayMs && ::IsWindow(window)) {
    ::SleepEx(kDelayMs, TRUE);  // Alertable.
    waited_ms += kDelayMs;
  }
  DCHECK(waited_ms < kMaxDelayMs);
  if (waited_ms < kMaxDelayMs) {
    return true;
  } else {
    return false;
  }
}

void GetAllWindows::Execute(const ListValue& args, int request_id) {
  scoped_ptr<IterativeWindowApiResult> result(CreateApiResult(request_id));
  bool populate_tabs = false;
  // The input dictionary is optional; if not provided, the args may be
  // either an empty list or a list with a single null value element.
  if (args.GetSize() > 0) {
    Value* first_arg = NULL;
    if (!args.Get(0, &first_arg) || first_arg == NULL) {
      NOTREACHED() << "bad request";
      result->PostError(api_module_constants::kInvalidArgumentsError);
    } else if (first_arg->GetType() != Value::TYPE_NULL) {
      DictionaryValue* dict = NULL;
      if (!args.GetDictionary(0, &dict) ||
          (dict->HasKey(ext::kPopulateKey) &&
           !dict->GetBoolean(ext::kPopulateKey, &populate_tabs))) {
        NOTREACHED() << "bad message";
        result->PostError(api_module_constants::kInvalidArgumentsError);
      }
    }

    if (result->AllFailed()) {
      result->FlushAllPosts();
      return;
    }
  }

  std::set<HWND> ie_frame_windows;
  window_utils::FindTopLevelWindows(windows::kIeFrameWindowClass,
                                    &ie_frame_windows);
  if (ie_frame_windows.empty()) {
    result->FlushAllPosts();
    return;
  }

  std::set<HWND>::const_iterator iter = ie_frame_windows.begin();
  for (; iter != ie_frame_windows.end(); ++iter) {
    if (result->CreateWindowValue(*iter, populate_tabs))
      result->PostResult();
  }

  DCHECK(!result->IsEmpty());
  if (result->IsEmpty())  // This is an error!
    result->PostError(api_module_constants::kInternalErrorError);

  result->FlushAllPosts();
}

}  // namespace window_api
