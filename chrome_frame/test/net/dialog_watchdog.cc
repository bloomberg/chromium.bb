// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <oleacc.h>

#include "chrome_frame/test/net/dialog_watchdog.h"

#include "base/logging.h"
#include "base/scoped_comptr_win.h"
#include "base/string_util.h"

#include "chrome_frame/test/chrome_frame_test_utils.h"
#include "chrome_frame/function_stub.h"

namespace {
// Uses the IAccessible interface for the window to set the focus.
// This can be useful when you don't have control over the thread that
// owns the window.
// NOTE: this depends on oleacc.lib which the net tests already depend on
// but other unit tests don't depend on oleacc so we can't just add the method
// directly into chrome_frame_test_utils.cc (without adding a
// #pragma comment(lib, "oleacc.lib")).
bool SetFocusToAccessibleWindow(HWND hwnd) {
  bool ret = false;
  ScopedComPtr<IAccessible> acc;
  AccessibleObjectFromWindow(hwnd, OBJID_WINDOW, IID_IAccessible,
      reinterpret_cast<void**>(acc.Receive()));
  if (acc) {
    VARIANT self = { VT_I4 };
    self.lVal = CHILDID_SELF;
    ret = SUCCEEDED(acc->accSelect(SELFLAG_TAKEFOCUS, self));
  }
  return ret;
}

}  // namespace

SupplyProxyCredentials::SupplyProxyCredentials(const char* username,
                                               const char* password)
    : username_(username), password_(password) {
}

bool SupplyProxyCredentials::OnDialogDetected(HWND hwnd,
                                              const std::string& caption) {
  // IE's dialog caption (en-US).
  if (caption.compare("Windows Security") != 0)
    return false;

  DialogProps props = {0};
  ::EnumChildWindows(hwnd, EnumChildren, reinterpret_cast<LPARAM>(&props));
  DCHECK(::IsWindow(props.username_));
  DCHECK(::IsWindow(props.password_));

  // We can't use SetWindowText to set the username/password, so simulate
  // keyboard input instead.
  chrome_frame_test::ForceSetForegroundWindow(hwnd);
  CHECK(SetFocusToAccessibleWindow(props.username_));
  chrome_frame_test::SendString(username_.c_str());
  Sleep(100);

  chrome_frame_test::SendVirtualKey(VK_TAB, false);
  Sleep(100);
  chrome_frame_test::SendString(password_.c_str());

  Sleep(100);
  chrome_frame_test::SendVirtualKey(VK_RETURN, false);

  return true;
}

// static
BOOL SupplyProxyCredentials::EnumChildren(HWND hwnd, LPARAM param) {
  if (!::IsWindowVisible(hwnd))
    return TRUE;  // Ignore but continue to enumerate.

  DialogProps* props = reinterpret_cast<DialogProps*>(param);

  char class_name[MAX_PATH] = {0};
  ::GetClassNameA(hwnd, class_name, arraysize(class_name));
  if (lstrcmpiA(class_name, "Edit") == 0) {
    if (props->username_ == NULL || props->username_ == hwnd) {
      props->username_ = hwnd;
    } else if (props->password_ == NULL) {
      props->password_ = hwnd;
    }
  } else {
    EnumChildWindows(hwnd, EnumChildren, param);
  }

  return TRUE;
}

DialogWatchdog::DialogWatchdog() : hook_(NULL), hook_stub_(NULL) {
  Initialize();
}

DialogWatchdog::~DialogWatchdog() {
  Uninitialize();
}

bool DialogWatchdog::Initialize() {
  DCHECK(hook_ == NULL);
  DCHECK(hook_stub_ == NULL);
  hook_stub_ = FunctionStub::Create(reinterpret_cast<uintptr_t>(this),
                                    WinEventHook);
  hook_ = SetWinEventHook(EVENT_OBJECT_SHOW, EVENT_OBJECT_SHOW, NULL,
                          reinterpret_cast<WINEVENTPROC>(hook_stub_->code()), 0,
                          0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

  return hook_ != NULL;
}

void DialogWatchdog::Uninitialize() {
  if (hook_) {
    ::UnhookWinEvent(hook_);
    hook_ = NULL;
    FunctionStub::Destroy(hook_stub_);
    hook_stub_ = NULL;
  }
}

// static
void DialogWatchdog::WinEventHook(DialogWatchdog* me, HWINEVENTHOOK hook,
                                  DWORD event, HWND hwnd, LONG object_id,
                                  LONG child_id, DWORD event_thread_id,
                                  DWORD event_time) {
  // Check for a dialog class ("#32770") and notify observers if we find one.
  char class_name[MAX_PATH] = {0};
  ::GetClassNameA(hwnd, class_name, arraysize(class_name));
  if (lstrcmpA(class_name, "#32770") == 0) {
    int len = ::GetWindowTextLength(hwnd);
    std::string text;
    ::GetWindowTextA(hwnd, WriteInto(&text, len + 1), len + 1);
    me->OnDialogFound(hwnd, text);
  }
}

void DialogWatchdog::OnDialogFound(HWND hwnd, const std::string& caption) {
  std::vector<DialogWatchdogObserver*>::iterator it = observers_.begin();
  while (it != observers_.end()) {
    if ((*it)->OnDialogDetected(hwnd, caption))
      break;
    it++;
  }
}
