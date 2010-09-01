// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/test/win_event_receiver.h"

#include "base/logging.h"
#include "base/string_util.h"

#include "chrome_frame/function_stub.h"

// WinEventReceiver methods
WinEventReceiver::WinEventReceiver()
    : listener_(NULL),
      hook_(NULL),
      hook_stub_(NULL) {
}

WinEventReceiver::~WinEventReceiver() {
  StopReceivingEvents();
}

void WinEventReceiver::SetListenerForEvent(WinEventListener* listener,
                                           DWORD event) {
  SetListenerForEvents(listener, event, event);
}

void WinEventReceiver::SetListenerForEvents(WinEventListener* listener,
                                            DWORD event_min, DWORD event_max) {
  DCHECK(listener != NULL);
  StopReceivingEvents();

  listener_ = listener;

  InitializeHook(event_min, event_max);
}

void WinEventReceiver::StopReceivingEvents() {
  if (hook_) {
    ::UnhookWinEvent(hook_);
    hook_ = NULL;
    FunctionStub::Destroy(hook_stub_);
    hook_stub_ = NULL;
  }
}

bool WinEventReceiver::InitializeHook(DWORD event_min, DWORD event_max) {
  DCHECK(hook_ == NULL);
  DCHECK(hook_stub_ == NULL);
  hook_stub_ = FunctionStub::Create(reinterpret_cast<uintptr_t>(this),
                                    WinEventHook);
  // Don't use WINEVENT_SKIPOWNPROCESS here because we fake generate an event
  // in the mock IE event sink (IA2_EVENT_DOCUMENT_LOAD_COMPLETE) that we want
  // to catch.
  hook_ = SetWinEventHook(event_min, event_max, NULL,
                          reinterpret_cast<WINEVENTPROC>(hook_stub_->code()), 0,
                          0, WINEVENT_OUTOFCONTEXT);
  DLOG_IF(ERROR, hook_ == NULL) << "Unable to SetWinEvent hook";
  return hook_ != NULL;
}

// static
void WinEventReceiver::WinEventHook(WinEventReceiver* me, HWINEVENTHOOK hook,
                                    DWORD event, HWND hwnd, LONG object_id,
                                    LONG child_id, DWORD event_thread_id,
                                    DWORD event_time) {
  DCHECK(me->listener_ != NULL);
  me->listener_->OnEventReceived(event, hwnd, object_id, child_id);
}

// WindowWatchdog methods
void WindowWatchdog::AddObserver(WindowObserver* observer,
                                 const std::string& window_class) {
  if (observers_.empty())
    win_event_receiver_.SetListenerForEvent(this, EVENT_OBJECT_SHOW);

  WindowObserverEntry new_entry = { observer, window_class };
  observers_.push_back(new_entry);
}

void WindowWatchdog::RemoveObserver(WindowObserver* observer) {
  for (ObserverMap::iterator i = observers_.begin(); i != observers_.end();) {
    i = (observer == i->observer) ? observers_.erase(i) : ++i;
  }

  if (observers_.empty())
    win_event_receiver_.StopReceivingEvents();
}

void WindowWatchdog::OnEventReceived(DWORD event, HWND hwnd, LONG object_id,
                                     LONG child_id) {
  // We need to look for top level windows and a natural check is for
  // WS_CHILD. Instead, checking for WS_CAPTION allows us to filter
  // out other stray popups
  if (!(WS_CAPTION & GetWindowLong(hwnd, GWL_STYLE)))
    return;

  char class_name[MAX_PATH] = {0};
  ::GetClassNameA(hwnd, class_name, arraysize(class_name));

  ObserverMap interested_observers;
  for (ObserverMap::iterator i = observers_.begin();
      i != observers_.end(); i++) {
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
