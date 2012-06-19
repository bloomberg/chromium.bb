// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shell_integration.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

ShellIntegration::DefaultWebClientSetPermission
    ShellIntegration::CanSetAsDefaultProtocolClient() {
  // Allowed as long as the browser can become the operating system default
  // browser.
  return CanSetAsDefaultBrowser();
}

ShellIntegration::ShortcutInfo::ShortcutInfo()
    : is_platform_app(false),
      create_on_desktop(false),
      create_in_applications_menu(false),
      create_in_quick_launch_bar(false) {
}

ShellIntegration::ShortcutInfo::~ShortcutInfo() {}

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
    bool is_platform_app) {
  const CommandLine& cmd_line = *CommandLine::ForCurrentProcess();
  CommandLine new_cmd_line(CommandLine::NO_PROGRAM);

  // Use the same UserDataDir for new launches that we currently have set.
  FilePath user_data_dir = cmd_line.GetSwitchValuePath(switches::kUserDataDir);
  if (!user_data_dir.empty()) {
    // Make sure user_data_dir is an absolute path.
    if (file_util::AbsolutePath(&user_data_dir) &&
        file_util::PathExists(user_data_dir)) {
      new_cmd_line.AppendSwitchPath(switches::kUserDataDir, user_data_dir);
    }
  }

#if defined(OS_CHROMEOS)
  FilePath profile = cmd_line.GetSwitchValuePath(switches::kLoginProfile);
  if (!profile.empty())
    new_cmd_line.AppendSwitchPath(switches::kLoginProfile, profile);
#endif

  // If |extension_app_id| is present, we use the kAppId switch rather than
  // the kApp switch (the launch url will be read from the extension app
  // during launch.
  if (!extension_app_id.empty()) {
    new_cmd_line.AppendSwitchASCII(switches::kAppId, extension_app_id);
    if (is_platform_app)
      new_cmd_line.AppendSwitch(switches::kEnableExperimentalExtensionApis);
  } else {
    // Use '--app=url' instead of just 'url' to launch the browser with minimal
    // chrome.
    // Note: Do not change this flag!  Old Gears shortcuts will break if you do!
    new_cmd_line.AppendSwitchASCII(switches::kApp, url.spec());
  }
  return new_cmd_line;
}

#if !defined(OS_WIN)
// static
bool ShellIntegration::SetAsDefaultBrowserInteractive() {
  return false;
}
#endif

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
  if (observer_) {
    observer_->SetDefaultWebClientUIState(STATE_PROCESSING);
  }
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(
          &DefaultWebClientWorker::ExecuteSetAsDefault, this));
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

void ShellIntegration::DefaultWebClientWorker::ExecuteSetAsDefault() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  SetAsDefault(observer_ && observer_->IsInteractiveSetDefaultPermitted());
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &DefaultWebClientWorker::CompleteSetAsDefault, this));
}

void ShellIntegration::DefaultWebClientWorker::CompleteSetAsDefault() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Set as default completed, check again to make sure it stuck...
  StartCheckIsDefault();
}

void ShellIntegration::DefaultWebClientWorker::UpdateUI(
    DefaultWebClientState state) {
  if (observer_) {
    switch (state) {
      case NOT_DEFAULT_WEB_CLIENT:
        observer_->SetDefaultWebClientUIState(STATE_NOT_DEFAULT);
        break;
      case IS_DEFAULT_WEB_CLIENT:
        observer_->SetDefaultWebClientUIState(STATE_IS_DEFAULT);
        break;
      case UNKNOWN_DEFAULT_WEB_CLIENT:
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
  return ShellIntegration::IsDefaultBrowser();
}

void ShellIntegration::DefaultBrowserWorker::SetAsDefault(
    bool interactive_permitted) {
  switch (ShellIntegration::CanSetAsDefaultBrowser()) {
    case ShellIntegration::SET_DEFAULT_UNATTENDED:
      ShellIntegration::SetAsDefaultBrowser();
      break;
    case ShellIntegration::SET_DEFAULT_INTERACTIVE:
      if (interactive_permitted)
        ShellIntegration::SetAsDefaultBrowserInteractive();
      break;
    default:
      NOTREACHED();
  }
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

void ShellIntegration::DefaultProtocolClientWorker::SetAsDefault(
    bool interactive_permitted) {
  ShellIntegration::SetAsDefaultProtocolClient(protocol_);
}
