// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_PROCESS_SINGLETON_H_
#define CHROME_BROWSER_CHROME_PROCESS_SINGLETON_H_

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "chrome/browser/process_singleton.h"
#include "chrome/browser/process_singleton_modal_dialog_lock.h"
#include "chrome/browser/process_singleton_startup_lock.h"

// Composes a basic ProcessSingleton with ProcessSingletonStartupLock and
// ProcessSingletonModalDialogLock.
//
// Notifications from ProcessSingleton will first close a modal dialog if
// active. Otherwise, until |Unlock()| is called, they will be queued up. Once
// unlocked, notifications will be passed to the client-supplied
// NotificationCallback.
//
// The client must ensure that SetModalDialogNotificationHandler is called
// appropriately when dialogs are displayed or dismissed during startup. If a
// dialog is active, it is closed (via the provided handler) and then the
// notification is processed as normal.
class ChromeProcessSingleton {
 public:
  ChromeProcessSingleton(
      const base::FilePath& user_data_dir,
      const ProcessSingleton::NotificationCallback& notification_callback);

  ~ChromeProcessSingleton();

  // Notify another process, if available. Otherwise sets ourselves as the
  // singleton instance. Returns PROCESS_NONE if we became the singleton
  // instance. Callers are guaranteed to either have notified an existing
  // process or have grabbed the singleton (unless the profile is locked by an
  // unreachable process).
  ProcessSingleton::NotifyResult NotifyOtherProcessOrCreate();

  // Clear any lock state during shutdown.
  void Cleanup();

  // Receives a callback to be run to close the active modal dialog, or an empty
  // closure if the active dialog is dismissed.
  void SetModalDialogNotificationHandler(base::Closure notification_handler);

  // Executes previously queued command-line invocations and allows future
  // invocations to be executed immediately.
  // This only has an effect the first time it is called.
  void Unlock();

 private:
  // We compose these two locks with the client-supplied notification callback.
  // First |modal_dialog_lock_| will discard any notifications that arrive while
  // a modal dialog is active. Otherwise, it will pass the notification to
  // |startup_lock_|, which will queue notifications until |Unlock()| is called.
  // Notifications passing through both locks are finally delivered to our
  // client.
  ProcessSingletonStartupLock startup_lock_;
  ProcessSingletonModalDialogLock modal_dialog_lock_;

  // The basic ProcessSingleton
  ProcessSingleton process_singleton_;

  DISALLOW_COPY_AND_ASSIGN(ChromeProcessSingleton);
};

#endif  // CHROME_BROWSER_CHROME_PROCESS_SINGLETON_H_
