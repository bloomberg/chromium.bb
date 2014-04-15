// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shell_integration.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/policy/policy_path_parser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"

#if defined(OS_CHROMEOS)
#include "chromeos/chromeos_switches.h"
#endif

#if !defined(OS_WIN)
#include "chrome/common/chrome_version_info.h"
#include "grit/chromium_strings.h"
#include "ui/base/l10n/l10n_util.h"
#endif

using content::BrowserThread;

ShellIntegration::DefaultWebClientSetPermission
    ShellIntegration::CanSetAsDefaultProtocolClient() {
  // Allowed as long as the browser can become the operating system default
  // browser.
  return CanSetAsDefaultBrowser();
}

static const struct ShellIntegration::AppModeInfo* gAppModeInfo = NULL;

// static
void ShellIntegration::SetAppModeInfo(const struct AppModeInfo* info) {
  gAppModeInfo = info;
}

// static
const struct ShellIntegration::AppModeInfo* ShellIntegration::AppModeInfo() {
  return gAppModeInfo;
}

// static
bool ShellIntegration::IsRunningInAppMode() {
  return gAppModeInfo != NULL;
}

// static
CommandLine ShellIntegration::CommandLineArgsForLauncher(
    const GURL& url,
    const std::string& extension_app_id,
    const base::FilePath& profile_path) {
  base::ThreadRestrictions::AssertIOAllowed();
  CommandLine new_cmd_line(CommandLine::NO_PROGRAM);

  AppendProfileArgs(
      extension_app_id.empty() ? base::FilePath() : profile_path,
      &new_cmd_line);

  // If |extension_app_id| is present, we use the kAppId switch rather than
  // the kApp switch (the launch url will be read from the extension app
  // during launch.
  if (!extension_app_id.empty()) {
    new_cmd_line.AppendSwitchASCII(switches::kAppId, extension_app_id);
  } else {
    // Use '--app=url' instead of just 'url' to launch the browser with minimal
    // chrome.
    // Note: Do not change this flag!  Old Gears shortcuts will break if you do!
    new_cmd_line.AppendSwitchASCII(switches::kApp, url.spec());
  }
  return new_cmd_line;
}

// static
void ShellIntegration::AppendProfileArgs(
    const base::FilePath& profile_path,
    CommandLine* command_line) {
  DCHECK(command_line);
  const CommandLine& cmd_line = *CommandLine::ForCurrentProcess();

  // Use the same UserDataDir for new launches that we currently have set.
  base::FilePath user_data_dir =
      cmd_line.GetSwitchValuePath(switches::kUserDataDir);
#if defined(OS_MACOSX) || defined(OS_WIN)
  policy::path_parser::CheckUserDataDirPolicy(&user_data_dir);
#endif
  if (!user_data_dir.empty()) {
    // Make sure user_data_dir is an absolute path.
    user_data_dir = base::MakeAbsoluteFilePath(user_data_dir);
    if (!user_data_dir.empty() && base::PathExists(user_data_dir))
      command_line->AppendSwitchPath(switches::kUserDataDir, user_data_dir);
  }

#if defined(OS_CHROMEOS)
  base::FilePath profile = cmd_line.GetSwitchValuePath(
      chromeos::switches::kLoginProfile);
  if (!profile.empty())
    command_line->AppendSwitchPath(chromeos::switches::kLoginProfile, profile);
#else
  if (!profile_path.empty())
    command_line->AppendSwitchPath(switches::kProfileDirectory,
                                   profile_path.BaseName());
#endif
}

#if !defined(OS_WIN)

base::string16 ShellIntegration::GetAppShortcutsSubdirName() {
  if (chrome::VersionInfo::GetChannel() == chrome::VersionInfo::CHANNEL_CANARY)
    return l10n_util::GetStringUTF16(IDS_APP_SHORTCUTS_SUBDIR_NAME_CANARY);
  return l10n_util::GetStringUTF16(IDS_APP_SHORTCUTS_SUBDIR_NAME);
}

// static
bool ShellIntegration::SetAsDefaultBrowserInteractive() {
  return false;
}

// static
bool ShellIntegration::SetAsDefaultProtocolClientInteractive(
    const std::string& protocol) {
  return false;
}
#endif

bool ShellIntegration::DefaultWebClientObserver::IsOwnedByWorker() {
  return false;
}

bool ShellIntegration::DefaultWebClientObserver::
    IsInteractiveSetDefaultPermitted() {
  return false;
}

///////////////////////////////////////////////////////////////////////////////
// ShellIntegration::DefaultWebClientWorker
//

ShellIntegration::DefaultWebClientWorker::DefaultWebClientWorker(
    DefaultWebClientObserver* observer)
    : observer_(observer) {
}

void ShellIntegration::DefaultWebClientWorker::StartCheckIsDefault() {
  if (observer_) {
    observer_->SetDefaultWebClientUIState(STATE_PROCESSING);
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(
            &DefaultWebClientWorker::ExecuteCheckIsDefault, this));
  }
}

void ShellIntegration::DefaultWebClientWorker::StartSetAsDefault() {
  bool interactive_permitted = false;
  if (observer_) {
    observer_->SetDefaultWebClientUIState(STATE_PROCESSING);
    interactive_permitted = observer_->IsInteractiveSetDefaultPermitted();
  }
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&DefaultWebClientWorker::ExecuteSetAsDefault, this,
                 interactive_permitted));
}

void ShellIntegration::DefaultWebClientWorker::ObserverDestroyed() {
  // Our associated view has gone away, so we shouldn't call back to it if
  // our worker thread returns after the view is dead.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observer_ = NULL;
}

///////////////////////////////////////////////////////////////////////////////
// DefaultWebClientWorker, private:

void ShellIntegration::DefaultWebClientWorker::ExecuteCheckIsDefault() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DefaultWebClientState state = CheckIsDefault();
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &DefaultWebClientWorker::CompleteCheckIsDefault, this, state));
}

void ShellIntegration::DefaultWebClientWorker::CompleteCheckIsDefault(
    DefaultWebClientState state) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  UpdateUI(state);
  // The worker has finished everything it needs to do, so free the observer
  // if we own it.
  if (observer_ && observer_->IsOwnedByWorker()) {
    delete observer_;
    observer_ = NULL;
  }
}

void ShellIntegration::DefaultWebClientWorker::ExecuteSetAsDefault(
    bool interactive_permitted) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  bool result = SetAsDefault(interactive_permitted);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&DefaultWebClientWorker::CompleteSetAsDefault, this, result));
}

void ShellIntegration::DefaultWebClientWorker::CompleteSetAsDefault(
    bool succeeded) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // First tell the observer what the SetAsDefault call has returned.
  if (observer_)
    observer_->OnSetAsDefaultConcluded(succeeded);
  // Set as default completed, check again to make sure it stuck...
  StartCheckIsDefault();
}

void ShellIntegration::DefaultWebClientWorker::UpdateUI(
    DefaultWebClientState state) {
  if (observer_) {
    switch (state) {
      case NOT_DEFAULT:
        observer_->SetDefaultWebClientUIState(STATE_NOT_DEFAULT);
        break;
      case IS_DEFAULT:
        observer_->SetDefaultWebClientUIState(STATE_IS_DEFAULT);
        break;
      case UNKNOWN_DEFAULT:
        observer_->SetDefaultWebClientUIState(STATE_UNKNOWN);
        break;
      default:
        break;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// ShellIntegration::DefaultBrowserWorker
//

ShellIntegration::DefaultBrowserWorker::DefaultBrowserWorker(
    DefaultWebClientObserver* observer)
    : DefaultWebClientWorker(observer) {
}

///////////////////////////////////////////////////////////////////////////////
// DefaultBrowserWorker, private:

ShellIntegration::DefaultWebClientState
ShellIntegration::DefaultBrowserWorker::CheckIsDefault() {
  return ShellIntegration::GetDefaultBrowser();
}

bool ShellIntegration::DefaultBrowserWorker::SetAsDefault(
    bool interactive_permitted) {
  bool result = false;
  switch (ShellIntegration::CanSetAsDefaultBrowser()) {
    case ShellIntegration::SET_DEFAULT_UNATTENDED:
      result = ShellIntegration::SetAsDefaultBrowser();
      break;
    case ShellIntegration::SET_DEFAULT_INTERACTIVE:
      if (interactive_permitted)
        result = ShellIntegration::SetAsDefaultBrowserInteractive();
      break;
    default:
      NOTREACHED();
  }

  return result;
}

///////////////////////////////////////////////////////////////////////////////
// ShellIntegration::DefaultProtocolClientWorker
//

ShellIntegration::DefaultProtocolClientWorker::DefaultProtocolClientWorker(
    DefaultWebClientObserver* observer, const std::string& protocol)
    : DefaultWebClientWorker(observer),
      protocol_(protocol) {
}

///////////////////////////////////////////////////////////////////////////////
// DefaultProtocolClientWorker, private:

ShellIntegration::DefaultWebClientState
ShellIntegration::DefaultProtocolClientWorker::CheckIsDefault() {
  return ShellIntegration::IsDefaultProtocolClient(protocol_);
}

bool ShellIntegration::DefaultProtocolClientWorker::SetAsDefault(
    bool interactive_permitted) {
  bool result = false;
  switch (ShellIntegration::CanSetAsDefaultProtocolClient()) {
    case ShellIntegration::SET_DEFAULT_NOT_ALLOWED:
      result = false;
      break;
    case ShellIntegration::SET_DEFAULT_UNATTENDED:
      result = ShellIntegration::SetAsDefaultProtocolClient(protocol_);
      break;
    case ShellIntegration::SET_DEFAULT_INTERACTIVE:
      if (interactive_permitted) {
        result = ShellIntegration::SetAsDefaultProtocolClientInteractive(
            protocol_);
      }
      break;
  }

  return result;
}
