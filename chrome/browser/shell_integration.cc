// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shell_integration.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"

ShellIntegration::ShortcutInfo::ShortcutInfo()
    : create_on_desktop(false),
      create_in_applications_menu(false),
      create_in_quick_launch_bar(false) {
}

ShellIntegration::ShortcutInfo::~ShortcutInfo() {}

std::string ShellIntegration::GetCommandLineArgumentsCommon(
    const GURL& url,
    const string16& extension_app_id) {
  const CommandLine cmd = *CommandLine::ForCurrentProcess();
  std::wstring arguments_w;

  // Use the same UserDataDir for new launches that we currently have set.
  FilePath user_data_dir = cmd.GetSwitchValuePath(switches::kUserDataDir);
  if (!user_data_dir.value().empty()) {
    // Make sure user_data_dir is an absolute path.
    if (file_util::AbsolutePath(&user_data_dir) &&
        file_util::PathExists(user_data_dir)) {
      // TODO: This is wrong in pathological quoting scenarios; we shouldn't be
      // passing around command lines as strings at all.
      arguments_w += std::wstring(L"--") + ASCIIToWide(switches::kUserDataDir) +
                     L"=\"" + user_data_dir.ToWStringHack() + L"\" ";
    }
  }

#if defined(OS_CHROMEOS)
  FilePath profile = cmd.GetSwitchValuePath(switches::kLoginProfile);
  if (!profile.empty()) {
    arguments_w += std::wstring(L"--") + ASCIIToWide(switches::kLoginProfile) +
        L"=\"" + profile.ToWStringHack() + L"\" ";
  }
#endif

  // If |extension_app_id| is present, we use the kAppId switch rather than
  // the kApp switch (the launch url will be read from the extension app
  // during launch.
  if (!extension_app_id.empty()) {
    arguments_w += std::wstring(L"--") + ASCIIToWide(switches::kAppId) +
        L"=\"" + ASCIIToWide(UTF16ToASCII(extension_app_id)) + L"\"";
  } else {
    // Use '--app=url' instead of just 'url' to launch the browser with minimal
    // chrome.
    // Note: Do not change this flag!  Old Gears shortcuts will break if you do!
    std::string url_string = url.spec();
    ReplaceSubstringsAfterOffset(&url_string, 0, "\\", "%5C");
    ReplaceSubstringsAfterOffset(&url_string, 0, "\"", "%22");
    ReplaceSubstringsAfterOffset(&url_string, 0, ";",  "%3B");
    ReplaceSubstringsAfterOffset(&url_string, 0, "$",  "%24");
#if defined(OS_WIN)  // Windows shortcuts can't escape % so we use \x instead.
    ReplaceSubstringsAfterOffset(&url_string, 0, "%",  "\\x");
#endif
    std::wstring url_w = UTF8ToWide(url_string);
    arguments_w += std::wstring(L"--") + ASCIIToWide(switches::kApp) +
        L"=\"" + url_w + L"\"";
  }
  return WideToUTF8(arguments_w);
}

///////////////////////////////////////////////////////////////////////////////
// ShellIntegration::DefaultBrowserWorker
//

ShellIntegration::DefaultBrowserWorker::DefaultBrowserWorker(
    DefaultBrowserObserver* observer)
    : observer_(observer) {
}

void ShellIntegration::DefaultBrowserWorker::StartCheckDefaultBrowser() {
  observer_->SetDefaultBrowserUIState(STATE_PROCESSING);
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(
          this, &DefaultBrowserWorker::ExecuteCheckDefaultBrowser));
}

void ShellIntegration::DefaultBrowserWorker::StartSetAsDefaultBrowser() {
  observer_->SetDefaultBrowserUIState(STATE_PROCESSING);
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(
          this, &DefaultBrowserWorker::ExecuteSetAsDefaultBrowser));
}

void ShellIntegration::DefaultBrowserWorker::ObserverDestroyed() {
  // Our associated view has gone away, so we shouldn't call back to it if
  // our worker thread returns after the view is dead.
  observer_ = NULL;
}

///////////////////////////////////////////////////////////////////////////////
// DefaultBrowserWorker, private:

void ShellIntegration::DefaultBrowserWorker::ExecuteCheckDefaultBrowser() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DefaultBrowserState state = ShellIntegration::IsDefaultBrowser();
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(
          this, &DefaultBrowserWorker::CompleteCheckDefaultBrowser, state));
}

void ShellIntegration::DefaultBrowserWorker::CompleteCheckDefaultBrowser(
    DefaultBrowserState state) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  UpdateUI(state);
}

void ShellIntegration::DefaultBrowserWorker::ExecuteSetAsDefaultBrowser() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  ShellIntegration::SetAsDefaultBrowser();
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(
          this, &DefaultBrowserWorker::CompleteSetAsDefaultBrowser));
}

void ShellIntegration::DefaultBrowserWorker::CompleteSetAsDefaultBrowser() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (observer_) {
    // Set as default completed, check again to make sure it stuck...
    StartCheckDefaultBrowser();
  }
}

void ShellIntegration::DefaultBrowserWorker::UpdateUI(
    DefaultBrowserState state) {
  if (observer_) {
    switch (state) {
      case NOT_DEFAULT_BROWSER:
        observer_->SetDefaultBrowserUIState(STATE_NOT_DEFAULT);
        break;
      case IS_DEFAULT_BROWSER:
        observer_->SetDefaultBrowserUIState(STATE_IS_DEFAULT);
        break;
      case UNKNOWN_DEFAULT_BROWSER:
        observer_->SetDefaultBrowserUIState(STATE_UNKNOWN);
        break;
      default:
        break;
    }
  }
}
