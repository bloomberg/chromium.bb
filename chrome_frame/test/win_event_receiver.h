// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_TEST_WIN_EVENT_RECEIVER_H_
#define CHROME_FRAME_TEST_WIN_EVENT_RECEIVER_H_

#include <windows.h>

#include <string>
#include <vector>

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

// Observes window show events. Used with WindowWatchdog.
class WindowObserver {
 public:
  virtual ~WindowObserver() {}
  // Called when a window has been shown.
  virtual void OnWindowDetected(HWND hwnd, const std::string& caption) = 0;
};

// Watch a for window to be shown with the given window class name.
// If found, call the observer interested in it.
class WindowWatchdog : public WinEventListener {
 public:
  // Register for notifications for |window_class|. An observer can register
  // for multiple notifications.
  void AddObserver(WindowObserver* observer, const std::string& window_class);

  // Remove all entries for |observer|.
  void RemoveObserver(WindowObserver* observer);

 private:
  struct WindowObserverEntry {
    WindowObserver* observer;
    std::string window_class;
  };

  typedef std::vector<WindowObserverEntry> ObserverMap;

  // Overriden from WinEventListener.
  virtual void OnEventReceived(DWORD event, HWND hwnd, LONG object_id,
                               LONG child_id);

  ObserverMap observers_;
  WinEventReceiver win_event_receiver_;
};

#endif  // CHROME_FRAME_TEST_WIN_EVENT_RECEIVER_H_
