// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/test/window_watchdog.h"

#include "base/logging.h"
#include "base/string_util.h"

#include "chrome_frame/function_stub.h"


WindowWatchdog::WindowWatchdog() : hook_(NULL), hook_stub_(NULL) {
}

WindowWatchdog::~WindowWatchdog() {
  UninitializeHook();
}

void WindowWatchdog::AddObserver(WindowObserver* observer,
                                 const std::string& window_class) {
  WindowObserverEntry new_entry = { observer, window_class };
  observers_.push_back(new_entry);

  if (!hook_)
    InitializeHook();
}

void WindowWatchdog::RemoveObserver(WindowObserver* observer) {
  for (ObserverMap::iterator i = observers_.begin(); i != observers_.end();) {
    i = (observer = i->observer) ? observers_.erase(i) : ++i;
  }

  if (observers_.empty())
    UninitializeHook();
}

bool WindowWatchdog::InitializeHook() {
  DCHECK(hook_ == NULL);
  DCHECK(hook_stub_ == NULL);
  hook_stub_ = FunctionStub::Create(reinterpret_cast<uintptr_t>(this),
                                    WinEventHook);
  hook_ = SetWinEventHook(EVENT_OBJECT_SHOW, EVENT_OBJECT_SHOW, NULL,
                          reinterpret_cast<WINEVENTPROC>(hook_stub_->code()), 0,
                          0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

  return hook_ != NULL;
}

void WindowWatchdog::UninitializeHook() {
  if (hook_) {
    ::UnhookWinEvent(hook_);
    hook_ = NULL;
    FunctionStub::Destroy(hook_stub_);
    hook_stub_ = NULL;
  }
}

// static
void WindowWatchdog::WinEventHook(WindowWatchdog* me, HWINEVENTHOOK hook,
                                  DWORD event, HWND hwnd, LONG object_id,
                                  LONG child_id, DWORD event_thread_id,
                                  DWORD event_time) {
  // We need to look for top level windows and a natural check is for
  // WS_CHILD. Instead, checking for WS_CAPTION allows us to filter
  // out other stray popups
  if (!(WS_CAPTION & GetWindowLong(hwnd, GWL_STYLE)))
    return;

  char class_name[MAX_PATH] = {0};
  ::GetClassNameA(hwnd, class_name, arraysize(class_name));

  ObserverMap interested_observers;
  for (ObserverMap::iterator i = me->observers_.begin();
      i != me->observers_.end(); i++) {
    if (0 == lstrcmpA(i->window_class.c_str(), class_name)) {
      interested_observers.push_back(*i);
    }
  }

  std::string caption;
  int len = ::GetWindowTextLength(hwnd) + 1;
  ::GetWindowTextA(hwnd, WriteInto(&caption, len), len);

  for (ObserverMap::iterator i = interested_observers.begin();
      i != interested_observers.end(); i++) {
    i->observer->OnWindowDetected(hwnd, caption);
  }
}
