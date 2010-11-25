// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Funnel of Chrome Extension Events to be sent to Chrome.

#include "ceee/ie/broker/window_events_funnel.h"

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/values.h"
#include "ceee/common/com_utils.h"
#include "ceee/common/windows_constants.h"
#include "ceee/common/window_utils.h"
#include "ceee/ie/broker/chrome_postman.h"
#include "ceee/ie/common/ie_util.h"
#include "chrome/browser/extensions/extension_event_names.h"
#include "chrome/browser/extensions/extension_tabs_module_constants.h"

namespace ext_event_names = extension_event_names;
namespace keys = extension_tabs_module_constants;

namespace {
// The Shell Hook uses a registered message to communicate with registered
// windows.
const UINT kShellHookMessage = ::RegisterWindowMessage(L"SHELLHOOK");

// The HWND, Class and WindowProc for the Registered Shell Hook Window.
HWND shell_hook_window = NULL;
const wchar_t kShellHookWindowClassName[] = L"CeeeShellHookWindow";
LRESULT CALLBACK WindowEventsFunnelWindowProc(HWND hwnd, UINT message,
                                              WPARAM wparam, LPARAM lparam) {
  if (message == kShellHookMessage) {
    switch (wparam) {
//       case HSHELL_WINDOWCREATED: {
//         if (window_utils::IsWindowClass(reinterpret_cast<HWND>(lparam),
//                                         windows::kIeFrameWindowClass)) {
//           WindowEventsFunnel window_events_funnel;
//           window_events_funnel.OnCreated(lparam);
//         }
//         break;
//       }
//       case HSHELL_WINDOWDESTROYED: {
//         if (window_utils::IsWindowClass(reinterpret_cast<HWND>(lparam),
//                                         windows::kIeFrameWindowClass)) {
//           WindowEventsFunnel window_events_funnel;
//           window_events_funnel.OnRemoved(lparam);
//         }
//         break;
//       }
      case HSHELL_WINDOWACTIVATED: {
        if (window_utils::IsWindowClass(reinterpret_cast<HWND>(lparam),
                                        windows::kIeFrameWindowClass)) {
          WindowEventsFunnel window_events_funnel;
          window_events_funnel.OnFocusChanged(lparam);
        }
        break;
      }
    }
  }
  return ::DefWindowProc(hwnd, message, wparam, lparam);
}
}

void WindowEventsFunnel::Initialize() {
  if (shell_hook_window)
    return;

  WNDCLASSEX shell_window_hook_class = {0};
  shell_window_hook_class.cbSize = sizeof(WNDCLASSEX);
  shell_window_hook_class.lpfnWndProc = WindowEventsFunnelWindowProc;
  shell_window_hook_class.hInstance = GetModuleHandle(NULL);
  shell_window_hook_class.lpszClassName = kShellHookWindowClassName;

  ATOM class_registration = ::RegisterClassEx(&shell_window_hook_class);
  DCHECK(class_registration != NULL) <<
      "Couldn't register Shell Hook Window class!" << com::LogWe();
  if (!class_registration)
    return;

  shell_hook_window = ::CreateWindow(
      reinterpret_cast<wchar_t*>(class_registration),
      L"", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, NULL, NULL);
  DCHECK(shell_hook_window != NULL) << "Couldn't Create Shell Hook Window!" <<
      com::LogWe();
  if (shell_hook_window) {
    BOOL success = ::RegisterShellHookWindow(shell_hook_window);
    DCHECK(success) << "Couldn't register shell hook window!" << com::LogWe();
  }
}

void WindowEventsFunnel::Terminate() {
  if (shell_hook_window) {
    BOOL success = ::DeregisterShellHookWindow(shell_hook_window);
    DCHECK(success) << "Couldn't unregister shell hook window!" << com::LogWe();
    success = ::DestroyWindow(shell_hook_window);
    shell_hook_window = NULL;
    DCHECK(success) << "Couldn't destroy shell hook window!" << com::LogWe();
  }
}

HRESULT WindowEventsFunnel::SendEvent(const char* event_name,
                                      const Value& event_args) {
  // Event arguments are always stored in a list.
  std::string event_args_str;
  if (event_args.IsType(Value::TYPE_LIST)) {
    base::JSONWriter::Write(&event_args, false, &event_args_str);
  } else {
    ListValue list;
    list.Append(event_args.DeepCopy());
    base::JSONWriter::Write(&list, false, &event_args_str);
  }

  DCHECK(ChromePostman::GetInstance() != NULL);
  ChromePostman::GetInstance()->FireEvent(event_name, event_args_str.c_str());
  return S_OK;
}

HRESULT WindowEventsFunnel::OnCreated(HWND window) {
  RECT window_rect;
  if (!::GetWindowRect(window, &window_rect)) {
    DWORD we = ::GetLastError();
    DCHECK(false) << "GetWindowRect failed " << com::LogWe(we);
    return HRESULT_FROM_WIN32(we);
  }

  scoped_ptr<DictionaryValue> dict(new DictionaryValue());
  dict->SetInteger(keys::kIdKey, reinterpret_cast<int>(window));
  dict->SetBoolean(keys::kFocusedKey,
      (window == window_utils::GetTopLevelParent(::GetForegroundWindow())));
  dict->SetInteger(keys::kLeftKey, window_rect.left);
  dict->SetInteger(keys::kTopKey, window_rect.top);
  dict->SetInteger(keys::kWidthKey, window_rect.right - window_rect.left);
  dict->SetInteger(keys::kHeightKey, window_rect.bottom - window_rect.top);
  dict->SetBoolean(keys::kIncognitoKey, ie_util::GetIEIsInPrivateBrowsing());
  // TODO(mad@chromium.org): for now, always setting to "normal" since
  // we don't yet have a way to tell if the window is a popup or not.
  dict->SetString(keys::kWindowTypeKey, keys::kWindowTypeValueNormal);

  return SendEvent(ext_event_names::kOnWindowCreated, *dict.get());
}

HRESULT WindowEventsFunnel::OnFocusChanged(int window_id) {
  scoped_ptr<Value> args(Value::CreateIntegerValue(window_id));
  return SendEvent(ext_event_names::kOnWindowFocusedChanged, *args.get());
}

HRESULT WindowEventsFunnel::OnRemoved(HWND window) {
  scoped_ptr<Value> args(Value::CreateIntegerValue(
      reinterpret_cast<int>(window)));
  return SendEvent(ext_event_names::kOnWindowRemoved, *args.get());
}
