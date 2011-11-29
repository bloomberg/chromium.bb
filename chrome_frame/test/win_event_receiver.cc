// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/test/win_event_receiver.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/win/object_watcher.h"
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

// Notifies WindowWatchdog when the process owning a given window exits.
//
// If the process terminates before its handle may be obtained, this class will
// still properly notifyy the WindowWatchdog.
//
// Notification is always delivered via a message loop task in the message loop
// that is active when the instance is constructed.
class WindowWatchdog::ProcessExitObserver
    : public base::win::ObjectWatcher::Delegate {
 public:
  // Initiates the process watch. Will always return without notifying the
  // watchdog.
  ProcessExitObserver(WindowWatchdog* window_watchdog, HWND hwnd);
  virtual ~ProcessExitObserver();

  // base::ObjectWatcher::Delegate implementation
  virtual void OnObjectSignaled(HANDLE process_handle);

 private:
  WindowWatchdog* window_watchdog_;
  HANDLE process_handle_;
  HWND hwnd_;

  base::WeakPtrFactory<ProcessExitObserver> weak_factory_;
  base::win::ObjectWatcher object_watcher_;

  DISALLOW_COPY_AND_ASSIGN(ProcessExitObserver);
};

WindowWatchdog::ProcessExitObserver::ProcessExitObserver(
    WindowWatchdog* window_watchdog, HWND hwnd)
    : window_watchdog_(window_watchdog),
      process_handle_(NULL),
      hwnd_(hwnd),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  DWORD pid = 0;
  ::GetWindowThreadProcessId(hwnd, &pid);
  if (pid != 0) {
    process_handle_ = ::OpenProcess(SYNCHRONIZE, FALSE, pid);
  }

  if (process_handle_ != NULL) {
    object_watcher_.StartWatching(process_handle_, this);
  } else {
    // Process is gone, so the window must be gone too. Notify our observer!
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&ProcessExitObserver::OnObjectSignaled,
                              weak_factory_.GetWeakPtr(), HANDLE(NULL)));
  }
}

WindowWatchdog::ProcessExitObserver::~ProcessExitObserver() {
  if (process_handle_ != NULL) {
    ::CloseHandle(process_handle_);
  }
}

void WindowWatchdog::ProcessExitObserver::OnObjectSignaled(
    HANDLE process_handle) {
  window_watchdog_->OnHwndProcessExited(hwnd_);
}

WindowWatchdog::WindowWatchdog() {}

void WindowWatchdog::AddObserver(WindowObserver* observer,
                                 const std::string& caption_pattern,
                                 const std::string& class_name_pattern) {
  if (observers_.empty()) {
    // SetListenerForEvents takes an event_min and event_max.
    // EVENT_OBJECT_DESTROY, EVENT_OBJECT_SHOW, and EVENT_OBJECT_HIDE are
    // consecutive, in that order; hence we supply only DESTROY and HIDE to
    // denote exactly the required set.
    win_event_receiver_.SetListenerForEvents(
        this, EVENT_OBJECT_DESTROY, EVENT_OBJECT_HIDE);
  }

  ObserverEntry new_entry = {
      observer,
      caption_pattern,
      class_name_pattern,
      OpenWindowList() };

  observers_.push_back(new_entry);
}

void WindowWatchdog::RemoveObserver(WindowObserver* observer) {
  for (ObserverEntryList::iterator i = observers_.begin();
       i != observers_.end(); ) {
    i = (observer == i->observer) ? observers_.erase(i) : ++i;
  }

  if (observers_.empty())
    win_event_receiver_.StopReceivingEvents();
}

std::string WindowWatchdog::GetWindowCaption(HWND hwnd) {
  std::string caption;
  int len = ::GetWindowTextLength(hwnd) + 1;
  if (len > 1)
    ::GetWindowTextA(hwnd, WriteInto(&caption, len), len);
  return caption;
}

bool WindowWatchdog::MatchingWindow(const ObserverEntry& entry,
                                    const std::string& caption,
                                    const std::string& class_name) {
  bool should_match_caption = !entry.caption_pattern.empty();
  bool should_match_class = !entry.class_name_pattern.empty();

  if (should_match_caption &&
      MatchPattern(caption, entry.caption_pattern) &&
      !should_match_class) {
    return true;
  }
  if (should_match_class &&
      MatchPattern(class_name, entry.class_name_pattern)) {
    return true;
  }
  return false;
}

void WindowWatchdog::HandleOnOpen(HWND hwnd) {
  std::string caption = GetWindowCaption(hwnd);
  char class_name[MAX_PATH] = {0};
  GetClassNameA(hwnd, class_name, arraysize(class_name));

  // Instantiated only if there is at least one interested observer. Each
  // interested observer will maintain a reference to this object, such that it
  // is deleted when the last observer disappears.
  linked_ptr<ProcessExitObserver> process_exit_observer;

  // Identify the interested observers and mark them as watching this HWND for
  // close.
  ObserverEntryList interested_observers;
  for (ObserverEntryList::iterator entry_iter = observers_.begin();
       entry_iter != observers_.end(); ++entry_iter) {
    if (MatchingWindow(*entry_iter, caption, class_name)) {
      if (process_exit_observer == NULL) {
        process_exit_observer.reset(new ProcessExitObserver(this, hwnd));
      }

      entry_iter->open_windows.push_back(
          OpenWindowEntry(hwnd, process_exit_observer));

      interested_observers.push_back(*entry_iter);
    }
  }

  // Notify the interested observers in a separate pass in case AddObserver or
  // RemoveObserver is called as a side-effect of the notification.
  for (ObserverEntryList::iterator entry_iter = interested_observers.begin();
       entry_iter != interested_observers.end(); ++entry_iter) {
    entry_iter->observer->OnWindowOpen(hwnd);
  }
}

void WindowWatchdog::HandleOnClose(HWND hwnd) {
  // Identify the interested observers, reaping OpenWindow entries as
  // appropriate
  ObserverEntryList interested_observers;
  for (ObserverEntryList::iterator entry_iter = observers_.begin();
       entry_iter != observers_.end(); ++entry_iter) {
    size_t num_open_windows = entry_iter->open_windows.size();

    OpenWindowList::iterator window_iter = entry_iter->open_windows.begin();
    while (window_iter != entry_iter->open_windows.end()) {
      if (hwnd == window_iter->first) {
        window_iter = entry_iter->open_windows.erase(window_iter);
      } else {
        ++window_iter;
      }
    }

    if (num_open_windows != entry_iter->open_windows.size()) {
      interested_observers.push_back(*entry_iter);
    }
  }

  // Notify the interested observers in a separate pass in case AddObserver or
  // RemoveObserver is called as a side-effect of the notification.
  for (ObserverEntryList::iterator entry_iter = interested_observers.begin();
    entry_iter != interested_observers.end(); ++entry_iter) {
    entry_iter->observer->OnWindowClose(hwnd);
  }
}

void WindowWatchdog::OnEventReceived(
    DWORD event, HWND hwnd, LONG object_id, LONG child_id) {
  // We need to look for top level windows and a natural check is for
  // WS_CHILD. Instead, checking for WS_CAPTION allows us to filter
  // out other stray popups
  if (event == EVENT_OBJECT_SHOW) {
    HandleOnOpen(hwnd);
  } else {
    DCHECK(event == EVENT_OBJECT_DESTROY || event == EVENT_OBJECT_HIDE);
    HandleOnClose(hwnd);
  }
}

void WindowWatchdog::OnHwndProcessExited(HWND hwnd) {
  HandleOnClose(hwnd);
}
