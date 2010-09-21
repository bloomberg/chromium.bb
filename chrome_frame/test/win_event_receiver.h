// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_TEST_WIN_EVENT_RECEIVER_H_
#define CHROME_FRAME_TEST_WIN_EVENT_RECEIVER_H_

#include <windows.h>

#include <string>
#include <vector>
#include <utility>

#include "base/linked_ptr.h"
#include "base/object_watcher.h"

struct FunctionStub;

// Listens to WinEvents from the WinEventReceiver.
class WinEventListener {
 public:
  virtual ~WinEventListener() {}
  // Called when an event has been received. |hwnd| is the window that generated
  // the event, or null if no window is associated with the event.
  virtual void OnEventReceived(DWORD event, HWND hwnd, LONG object_id,
                               LONG child_id) = 0;
};

// Receives WinEvents and forwards them to its listener. The event types the
// listener wants to receive can be specified.
class WinEventReceiver {
 public:
  WinEventReceiver();
  ~WinEventReceiver();

  // Sets the sole listener of this receiver. The listener will receive all
  // WinEvents of the given event type. Any previous listener will be
  // replaced. |listener| should not be NULL.
  void SetListenerForEvent(WinEventListener* listener, DWORD event);

  // Same as above, but sets a range of events to listen for.
  void SetListenerForEvents(WinEventListener* listener, DWORD event_min,
                            DWORD event_max);

  // Stops receiving events and forwarding them to the listener. It is
  // permitted to call this even if the receiver has already been stopped.
  void StopReceivingEvents();

 private:
  bool InitializeHook(DWORD event_min, DWORD event_max);

  static void CALLBACK WinEventHook(WinEventReceiver* me, HWINEVENTHOOK hook,
      DWORD event, HWND hwnd, LONG object_id, LONG child_id,
      DWORD event_thread_id, DWORD event_time);

  WinEventListener* listener_;
  HWINEVENTHOOK hook_;
  FunctionStub* hook_stub_;
};

// Receives notifications when a window is opened or closed.
class WindowObserver {
 public:
  virtual ~WindowObserver() {}
  virtual void OnWindowOpen(HWND hwnd) = 0;
  virtual void OnWindowClose(HWND hwnd) = 0;
};

// Notifies observers when windows whose captions match specified patterns
// open or close. When a window opens, its caption is compared to the patterns
// associated with each observer. Observers registered with matching patterns
// are notified of the window's opening and will be notified when the same
// window is closed (including if the owning process terminates without closing
// the window).
//
// Changes to a window's caption while it is open do not affect the set of
// observers to be notified when it closes.
//
// Observers are not notified of the closing of windows that were already open
// when they were registered.
//
// Observers may call AddObserver and/or RemoveObserver during notifications.
//
// Each instance of this class must only be accessed from a single thread, and
// that thread must be running a message loop.
class WindowWatchdog : public WinEventListener {
 public:
  WindowWatchdog();
  // Register |observer| to be notified when windows matching |caption_pattern|
  // are opened or closed. A single observer may be registered multiple times.
  // If a single window caption matches multiple registrations of a single
  // observer, the observer will be notified once per matching registration.
  void AddObserver(WindowObserver* observer,
                   const std::string& caption_pattern);

  // Remove all registrations of |observer|. The |observer| will not be notified
  // during or after this call.
  void RemoveObserver(WindowObserver* observer);

 private:
  class ProcessExitObserver;

  // The Delegate object is actually a ProcessExitObserver, but declaring
  // it as such would require fully declaring the ProcessExitObserver class
  // here in order for linked_ptr to access its destructor.
  typedef std::pair<HWND, linked_ptr<base::ObjectWatcher::Delegate> >
    OpenWindowEntry;
  typedef std::vector<OpenWindowEntry> OpenWindowList;

  struct ObserverEntry {
    WindowObserver* observer;
    std::string caption_pattern;
    OpenWindowList open_windows;
  };

  typedef std::vector<ObserverEntry> ObserverEntryList;

  // WinEventListener implementation.
  virtual void OnEventReceived(
    DWORD event, HWND hwnd, LONG object_id, LONG child_id);

  static std::string GetWindowCaption(HWND hwnd);

  void HandleOnOpen(HWND hwnd);
  void HandleOnClose(HWND hwnd);
  void OnHwndProcessExited(HWND hwnd);

  ObserverEntryList observers_;
  WinEventReceiver win_event_receiver_;

  DISALLOW_COPY_AND_ASSIGN(WindowWatchdog);
};



#endif  // CHROME_FRAME_TEST_WIN_EVENT_RECEIVER_H_
