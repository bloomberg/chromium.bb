// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_TEST_WINDOW_WATCHDOG_H_
#define CHROME_FRAME_TEST_WINDOW_WATCHDOG_H_

#include <windows.h>

#include <string>
#include <vector>

struct FunctionStub;

// Interface implemented by WindowWatchdog users. An observer can register
// for notifications on multiple window classes.
class WindowObserver {  // NOLINT
 public:
  virtual void OnWindowDetected(HWND hwnd, const std::string& caption) = 0;

 protected:
  virtual ~WindowObserver() {}
};

// Watch a for window to be shown with the given window class name.
// If found, call the observer interested in it.
class WindowWatchdog {
 public:
  WindowWatchdog();
  ~WindowWatchdog();

  // Register for notifications for |window_class|. An observer can register
  // for multiple notifications
  void AddObserver(WindowObserver* observer, const std::string& window_class);

  // Remove all entries for |observer|
  void RemoveObserver(WindowObserver* observer);

 protected:
  bool InitializeHook();
  void UninitializeHook();

  static void CALLBACK WinEventHook(WindowWatchdog* me, HWINEVENTHOOK hook,
      DWORD event, HWND hwnd, LONG object_id, LONG child_id,
      DWORD event_thread_id, DWORD event_time);

  void OnDialogFound(HWND hwnd, const std::string& caption);

 protected:
  struct WindowObserverEntry {
    WindowObserver* observer;
    std::string window_class;
  };

  typedef std::vector<WindowObserverEntry> ObserverMap;

  HWINEVENTHOOK hook_;
  ObserverMap observers_;
  FunctionStub* hook_stub_;
};


#endif  // CHROME_FRAME_TEST_WINDOW_WATCHDOG_H_
