// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/process_singleton_modal_dialog_lock.h"

#if defined(OS_WIN)
#include <Windows.h>
#endif

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"

namespace {

#if !defined(OS_WIN) || defined(USE_AURA)
void DoSetForegroundWindow(gfx::NativeWindow /* target_window */) {
  NOTREACHED();
}
#else
void DoSetForegroundWindow(HWND target_window) {
  ::SetForegroundWindow(target_window);
}
#endif

}  // namespace

ProcessSingletonModalDialogLock::ProcessSingletonModalDialogLock(
    const ProcessSingleton::NotificationCallback& original_callback)
    : active_dialog_(NULL),
      original_callback_(original_callback),
      set_foreground_window_handler_(base::Bind(&DoSetForegroundWindow)) {}

ProcessSingletonModalDialogLock::ProcessSingletonModalDialogLock(
    const ProcessSingleton::NotificationCallback& original_callback,
    const SetForegroundWindowHandler& set_foreground_window_handler)
    : active_dialog_(NULL),
      original_callback_(original_callback),
      set_foreground_window_handler_(set_foreground_window_handler) {}

ProcessSingletonModalDialogLock::~ProcessSingletonModalDialogLock() {}

void ProcessSingletonModalDialogLock::SetActiveModalDialog(
    gfx::NativeWindow active_dialog) {
  active_dialog_ = active_dialog;
}

ProcessSingleton::NotificationCallback
ProcessSingletonModalDialogLock::AsNotificationCallback() {
  return base::Bind(&ProcessSingletonModalDialogLock::NotificationCallbackImpl,
                    base::Unretained(this));
}

bool ProcessSingletonModalDialogLock::NotificationCallbackImpl(
      const CommandLine& command_line,
      const base::FilePath& current_directory) {
  if (active_dialog_ != NULL) {
    set_foreground_window_handler_.Run(active_dialog_);
    return true;
  } else {
    return original_callback_.Run(command_line, current_directory);
  }
}
