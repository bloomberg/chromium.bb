// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/interstitials/chrome_controller_client.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/process/launch.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/interstitials/chrome_metrics_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/safe_browsing_db/safe_browsing_prefs.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/intent_helper.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/chrome_pages.h"
#endif

#if defined(OS_WIN)
#include "base/base_paths_win.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/win/windows_version.h"
#endif

using content::Referrer;

namespace {

#if !defined(OS_CHROMEOS)
void LaunchDateAndTimeSettingsOnFileThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::FILE);
// The code for each OS is completely separate, in order to avoid bugs like
// https://crbug.com/430877 . ChromeOS is handled on the UI thread.
#if defined(OS_ANDROID)
  chrome::android::OpenDateAndTimeSettings();

#elif defined(OS_LINUX)
  struct ClockCommand {
    const char* const pathname;
    const char* const argument;
  };
  static const ClockCommand kClockCommands[] = {
      // Unity
      {"/usr/bin/unity-control-center", "datetime"},
      // GNOME
      //
      // NOTE: On old Ubuntu, naming control panels doesn't work, so it
      // opens the overview. This will have to be good enough.
      {"/usr/bin/gnome-control-center", "datetime"},
      {"/usr/local/bin/gnome-control-center", "datetime"},
      {"/opt/bin/gnome-control-center", "datetime"},
      // KDE
      {"/usr/bin/kcmshell4", "clock"},
      {"/usr/local/bin/kcmshell4", "clock"},
      {"/opt/bin/kcmshell4", "clock"},
  };

  base::CommandLine command(base::FilePath(""));
  for (const ClockCommand& cmd : kClockCommands) {
    base::FilePath pathname(cmd.pathname);
    if (base::PathExists(pathname)) {
      command.SetProgram(pathname);
      command.AppendArg(cmd.argument);
      break;
    }
  }
  if (command.GetProgram().empty()) {
    // Alas, there is nothing we can do.
    return;
  }

  base::LaunchOptions options;
  options.wait = false;
  options.allow_new_privs = true;
  base::LaunchProcess(command, options);

#elif defined(OS_MACOSX)
  base::CommandLine command(base::FilePath("/usr/bin/open"));
  command.AppendArg("/System/Library/PreferencePanes/DateAndTime.prefPane");

  base::LaunchOptions options;
  options.wait = false;
  base::LaunchProcess(command, options);

#elif defined(OS_WIN)
  base::FilePath path;
  PathService::Get(base::DIR_SYSTEM, &path);
  static const base::char16 kControlPanelExe[] = L"control.exe";
  path = path.Append(base::string16(kControlPanelExe));
  base::CommandLine command(path);
  command.AppendArg(std::string("/name"));
  command.AppendArg(std::string("Microsoft.DateAndTime"));

  base::LaunchOptions options;
  options.wait = false;
  base::LaunchProcess(command, options);

#else
  NOTREACHED();

#endif
  // Don't add code here! (See the comment at the beginning of the function.)
}
#endif

}  // namespace

ChromeControllerClient::ChromeControllerClient(
    content::WebContents* web_contents,
    std::unique_ptr<security_interstitials::MetricsHelper> metrics_helper)
    : SecurityInterstitialControllerClient(
        web_contents, std::move(metrics_helper),
        Profile::FromBrowserContext(
            web_contents->GetBrowserContext())->GetPrefs(),
        g_browser_process->GetApplicationLocale(),
        GURL(chrome::kChromeUINewTabURL)) {}

ChromeControllerClient::~ChromeControllerClient() {}

bool ChromeControllerClient::CanLaunchDateAndTimeSettings() {
#if defined(OS_ANDROID) || defined(OS_CHROMEOS) || defined(OS_LINUX) || \
    defined(OS_MACOSX) || defined(OS_WIN)
  return true;
#else
  return false;
#endif
}

void ChromeControllerClient::LaunchDateAndTimeSettings() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

#if defined(OS_CHROMEOS)
  chrome::ShowSettingsSubPageForProfile(ProfileManager::GetActiveUserProfile(),
                                        chrome::kDateTimeSubPage);
#else
  content::BrowserThread::PostTask(
      content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&LaunchDateAndTimeSettingsOnFileThread));
#endif
}
