// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROCESS_SINGLETON_MODAL_DIALOG_LOCK_H_
#define CHROME_BROWSER_PROCESS_SINGLETON_MODAL_DIALOG_LOCK_H_

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "chrome/browser/process_singleton.h"
#include "ui/gfx/native_widget_types.h"

namespace base {
class CommandLine;
class FilePath;
}

// Provides a ProcessSingleton::NotificationCallback that prevents
// command-line handling when a modal dialog is active during startup. The
// client must ensure that SetActiveModalDialog is called appropriately when
// such dialogs are displayed or dismissed.
//
// While a dialog is active, the ProcessSingleton notification
// callback will handle but ignore notifications:
// 1. Neither this process nor the invoking process will handle the command
//    line.
// 2. The active dialog is brought to the foreground and/or the taskbar icon
//    flashed (using ::SetForegroundWindow on Windows).
//
// Otherwise, the notification is forwarded to a wrapped NotificationCallback.
class ProcessSingletonModalDialogLock {
 public:
  typedef base::Callback<void(gfx::NativeWindow)> SetForegroundWindowHandler;
  explicit ProcessSingletonModalDialogLock(
      const ProcessSingleton::NotificationCallback& original_callback);

  ProcessSingletonModalDialogLock(
      const ProcessSingleton::NotificationCallback& original_callback,
      const SetForegroundWindowHandler& set_foreground_window_handler);

  ~ProcessSingletonModalDialogLock();

  // Receives a handle to the active modal dialog, or NULL if the active dialog
  // is dismissed.
  void SetActiveModalDialog(gfx::NativeWindow active_dialog);

  // Returns the ProcessSingleton::NotificationCallback.
  // The callback is only valid during the lifetime of the
  // ProcessSingletonModalDialogLock instance.
  ProcessSingleton::NotificationCallback AsNotificationCallback();

 private:
  bool NotificationCallbackImpl(const base::CommandLine& command_line,
                                const base::FilePath& current_directory);

  gfx::NativeWindow active_dialog_;
  ProcessSingleton::NotificationCallback original_callback_;
  SetForegroundWindowHandler set_foreground_window_handler_;

  DISALLOW_COPY_AND_ASSIGN(ProcessSingletonModalDialogLock);
};

#endif  // CHROME_BROWSER_PROCESS_SINGLETON_MODAL_DIALOG_LOCK_H_
