// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_TEST_NET_DIALOG_WATCHDOG_H_
#define CHROME_FRAME_TEST_NET_DIALOG_WATCHDOG_H_

#include <windows.h>

#include <string>
#include <vector>

struct FunctionStub;

class DialogWatchdogObserver {  // NOLINT
 public:
  // returns true if this observer handled the dialog.
  virtual bool OnDialogDetected(HWND hwnd, const std::string& caption) = 0;
};

class SupplyProxyCredentials : public DialogWatchdogObserver {
 public:
  SupplyProxyCredentials(const char* username, const char* password);

 protected:
  struct DialogProps {
    HWND username_;
    HWND password_;
  };

  virtual bool OnDialogDetected(HWND hwnd, const std::string& caption);
  static BOOL CALLBACK EnumChildren(HWND hwnd, LPARAM param);

 protected:
  std::string username_;
  std::string password_;
};

class DialogWatchdog {
 public:
  DialogWatchdog();
  ~DialogWatchdog();

  inline void AddObserver(DialogWatchdogObserver* observer) {
    observers_.push_back(observer);
  }

  bool Initialize();
  void Uninitialize();

 protected:
  static void CALLBACK WinEventHook(DialogWatchdog* me, HWINEVENTHOOK hook,
      DWORD event, HWND hwnd, LONG object_id, LONG child_id,
      DWORD event_thread_id, DWORD event_time);

  void OnDialogFound(HWND hwnd, const std::string& caption);

 protected:
  HWINEVENTHOOK hook_;
  std::vector<DialogWatchdogObserver*> observers_;
  FunctionStub* hook_stub_;
};

#endif  // CHROME_FRAME_TEST_NET_DIALOG_WATCHDOG_H_
