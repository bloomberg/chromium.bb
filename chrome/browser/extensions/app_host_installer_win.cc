// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// AppHostInstaller checks the presence of app_host.exe, and launches
// the installer if missing. The check must be performed on the FILE thread.
// The installation is also launched on the FILE thread as an asynchronous
// process. Once installation completes, QuickEnableWatcher is notified.
// AppHostInstaller::FinishOnCallerThread() is called in the end,
// which notifies the caller via a completion callback on the original
// calling thread, and then destroys the AppHostInstaller instance.

#include "chrome/browser/extensions/app_host_installer_win.h"

#include <windows.h>
#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/win/registry.h"
#include "chrome/installer/launcher_support/chrome_launcher_support.h"
#include "chrome/installer/util/util_constants.h"

namespace {

using extensions::OnAppHostInstallationCompleteCallback;

// TODO(huangs) Refactor the constants: http://crbug.com/148538
const wchar_t kGoogleRegClientsKey[] = L"Software\\Google\\Update\\Clients";

// Copied from chrome_appid.cc.
const wchar_t kBinariesAppGuid[] = L"{4DC8B4CA-1BDA-483e-B5FA-D3C12E15B62D}";

// Copied from google_update_constants.cc
const wchar_t kRegCommandLineField[] = L"CommandLine";
const wchar_t kRegCommandsKey[] = L"Commands";

// Copied from util_constants.cc.
const wchar_t kCmdQuickEnableApplicationHost[] =
    L"quick-enable-application-host";

// QuickEnableDelegate handles the completion event of App Host installation
// via the quick-enable-application host command.  At construction, the
// class is given |callback_| that takes a bool parameter.
// Upon completion, |callback_| is invoked, and is passed a boolean to
// indicate success or failure of installation.
class QuickEnableDelegate : public base::win::ObjectWatcher::Delegate {
 public:
  explicit QuickEnableDelegate(
      const OnAppHostInstallationCompleteCallback& callback);

  ~QuickEnableDelegate();

  // base::win::ObjectWatcher::Delegate implementation.
  virtual void OnObjectSignaled(HANDLE object) OVERRIDE;

 private:
  OnAppHostInstallationCompleteCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(QuickEnableDelegate);
};

QuickEnableDelegate::QuickEnableDelegate(
    const OnAppHostInstallationCompleteCallback& callback)
    : callback_(callback) {}

QuickEnableDelegate::~QuickEnableDelegate() {}

void QuickEnableDelegate::OnObjectSignaled(HANDLE object) {
  // Reset callback_ to free up references. But do it now because it's possible
  // that callback_.Run() will cause this object to be deleted.
  OnAppHostInstallationCompleteCallback callback(callback_);
  callback_.Reset();

  int exit_code = 0;
  base::TerminationStatus status(
      base::GetTerminationStatus(object, &exit_code));
  if (status == base::TERMINATION_STATUS_NORMAL_TERMINATION) {
    callback.Run(true);
  } else {
    LOG(ERROR) << "App Launcher install failed, status = " << status
               << ", exit code = " << exit_code;
    callback.Run(false);
  }

  // At this point 'this' may be deleted. Don't do anything else here.
}

// Reads the path to app_host.exe from the value "UninstallString" within the
// App Host's "ClientState" registry key.  Returns an empty string if the path
// does not exist or cannot be read.
string16 GetQuickEnableAppHostCommand(bool system_level) {
  HKEY root_key = system_level ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  string16 subkey(kGoogleRegClientsKey);
  subkey.append(1, L'\\').append(kBinariesAppGuid)
      .append(1, L'\\').append(kRegCommandsKey)
      .append(1, L'\\').append(kCmdQuickEnableApplicationHost);
  base::win::RegKey reg_key;
  string16 cmd;
  if (reg_key.Open(root_key, subkey.c_str(),
                   KEY_QUERY_VALUE) == ERROR_SUCCESS) {
    // If read is unsuccessful, |cmd| remains empty.
    reg_key.ReadValue(kRegCommandLineField, &cmd);
  }
  return cmd;
}

// Launches the Google Update command to quick-enable App Host.
// Returns true if the command is launched.
bool LaunchQuickEnableAppHost(base::win::ScopedHandle* process) {
  DCHECK(!process->IsValid());
  bool success = false;

  string16 cmd_str(GetQuickEnableAppHostCommand(true));
  if (cmd_str.empty())  // Try user-level if absent from system-level.
    cmd_str = GetQuickEnableAppHostCommand(false);
  if (!cmd_str.empty()) {
    VLOG(1) << "Quick-enabling application host: " << cmd_str;
    if (!base::LaunchProcess(cmd_str, base::LaunchOptions(),
                             process->Receive())) {
      LOG(ERROR) << "Failed to quick-enable application host.";
    }
    success = process->IsValid();
  }
  return success;
}

}  // namespace

namespace extensions {

using content::BrowserThread;

// static
bool AppHostInstaller::install_with_launcher_ = false;

// static
void AppHostInstaller::EnsureAppHostInstalled(
    const OnAppHostInstallationCompleteCallback& completion_callback) {
  BrowserThread::ID caller_thread_id;
  if (!BrowserThread::GetCurrentThreadIdentifier(&caller_thread_id)) {
    NOTREACHED();
    return;
  }

  // AppHostInstaler will delete itself
  (new AppHostInstaller(completion_callback, caller_thread_id))->
      EnsureAppHostInstalledInternal();
}

void AppHostInstaller::SetInstallWithLauncher(
    bool install_with_launcher) {
  install_with_launcher_ = install_with_launcher;
}

bool AppHostInstaller::GetInstallWithLauncher() {
  return install_with_launcher_;
}

AppHostInstaller::AppHostInstaller(
    const OnAppHostInstallationCompleteCallback& completion_callback,
    BrowserThread::ID caller_thread_id)
    : completion_callback_(completion_callback),
      caller_thread_id_(caller_thread_id) {}

AppHostInstaller::~AppHostInstaller() {}

void AppHostInstaller::EnsureAppHostInstalledInternal() {
  if (!BrowserThread::CurrentlyOn(BrowserThread::FILE)) {
    // Redo on FILE thread.
    if (!BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
            base::Bind(&AppHostInstaller::EnsureAppHostInstalledInternal,
                       base::Unretained(this)))) {
      FinishOnCallerThread(false);
    }
    return;
  }

  if ((install_with_launcher_ &&
       chrome_launcher_support::IsAppLauncherPresent()) ||
      (!install_with_launcher_ &&
       chrome_launcher_support::IsAppHostPresent())) {
    FinishOnCallerThread(true);
  } else {
    InstallAppHostOnFileThread();
  }
}

void AppHostInstaller::InstallAppHostOnFileThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(!process_.IsValid());
  if (!LaunchQuickEnableAppHost(&process_)) {
    FinishOnCallerThread(false);
  } else {
    DCHECK(process_.IsValid());
    DCHECK(!delegate_.get());
    watcher_.StopWatching();
    delegate_.reset(new QuickEnableDelegate(
                        base::Bind(&AppHostInstaller::FinishOnCallerThread,
                                   base::Unretained(this))));
    watcher_.StartWatching(process_, delegate_.get());
  }
}

void AppHostInstaller::FinishOnCallerThread(bool success) {
  if (!BrowserThread::CurrentlyOn(caller_thread_id_)) {
    // Redo on caller thread.
    if (!BrowserThread::PostTask(caller_thread_id_, FROM_HERE,
            base::Bind(&AppHostInstaller::FinishOnCallerThread,
                       base::Unretained(this),
                       success))) {
      // This could happen in Shutdown....
      delete this;
    }
    return;
  }

  completion_callback_.Run(success);
  delete this;
}

}  // namespace extensions
