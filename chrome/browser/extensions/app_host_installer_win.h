// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_APP_HOST_INSTALLER_WIN_H_
#define CHROME_BROWSER_EXTENSIONS_APP_HOST_INSTALLER_WIN_H_

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "base/win/object_watcher.h"
#include "base/win/scoped_handle.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {

class Extension;

// Called on the original calling thread after InstallAppHostIfNecessary()
// is complete.  A boolean flag is passed, which is false iff the installation
// was required but failed.
typedef base::Callback<void(bool)> OnAppHostInstallationCompleteCallback;

// Determines if the App Host is installed and, if not, installs it.
class AppHostInstaller {
 public:
  // Asynchronously checks the system state and installs the App Host if not
  // present. |completion_callback| will be notified on the caller's thread
  // when the installation process completes, and is passed a boolean to
  // indicate success or failure of installation.
  // No timeout is used - thus |completion_callback| may be held for an
  // arbitrarily long time, including past browser shutdown.
  static void EnsureAppHostInstalled(
      const OnAppHostInstallationCompleteCallback& completion_callback);

  static void SetInstallWithLauncher(bool install_with_launcher);

  static bool GetInstallWithLauncher();

 private:
  // Constructs an AppHostInstaller, which will call |completion_callback|
  // on the specified thread upon installation completion.
  AppHostInstaller(
      const OnAppHostInstallationCompleteCallback& completion_callback,
      content::BrowserThread::ID caller_thread_id);

  ~AppHostInstaller();

  // Checks the system state on the FILE thread. Will call
  // InstallAppHostOnFileThread() if App Host is not installed.
  void EnsureAppHostInstalledInternal();

  // Runs on the FILE thread. Installs App Host by asynchronously triggering
  // App Host installation. Will call FinishOnCallerThread() upon completion
  // (successful or otherwise).
  void InstallAppHostOnFileThread();

  // Passes |success| to |completion_callback| on the original caller thread.
  // Deletes the AppHostInstaller.
  void FinishOnCallerThread(bool success);

  OnAppHostInstallationCompleteCallback completion_callback_;
  content::BrowserThread::ID caller_thread_id_;
  base::win::ScopedHandle process_;
  scoped_ptr<base::win::ObjectWatcher::Delegate> delegate_;
  base::win::ObjectWatcher watcher_;

  static bool install_with_launcher_;

  DISALLOW_COPY_AND_ASSIGN(AppHostInstaller);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_APP_HOST_INSTALLER_WIN_H_
