// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/application_lifetime.h"

#include "base/bind.h"
#include "base/prefs/pref_service.h"
#include "base/win/metro.h"
#include "base/win/windows_version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/first_run/upgrade_util.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/pref_names.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chrome/browser/metro_utils/metro_chrome_win.h"
#include "chrome/browser/metro_viewer/chrome_metro_viewer_process_host_aurawin.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/installer/util/util_constants.h"
#include "content/public/browser/web_contents.h"
#endif

namespace chrome {

#if !defined(USE_AURA)
void HandleAppExitingForPlatform() {
  views::Widget::CloseAllSecondaryWidgets();
}
#endif

// Following set of functions, which are used to switch chrome mode after
// restart are used for in places where either user explicitly wants to switch
// mode or some functionality is not available in either mode and we ask user
// to switch mode.
// Here mode refers to Windows 8 modes such as Metro (also called immersive)
// and desktop mode (Classic or traditional).

// Mode switch based on current mode which is devised from current process.
void AttemptRestartWithModeSwitch() {
#if defined(USE_AURA)
  // This function should be called only from non aura code path.
  // In aura/ash windows world browser process is always non metro.
  NOTREACHED();
#else
  // The kRestartSwitchMode preference does not exists for Windows 7 and older
  // operating systems so there is no need for OS version check.
  PrefService* prefs = g_browser_process->local_state();
  if (base::win::IsMetroProcess()) {
    prefs->SetString(prefs::kRelaunchMode,
                     upgrade_util::kRelaunchModeDesktop);
  } else {
    prefs->SetString(prefs::kRelaunchMode,
                     upgrade_util::kRelaunchModeMetro);
  }
  AttemptRestart();
#endif
}

#if defined(USE_AURA)
void ActivateDesktopHelper(AshExecutionStatus ash_execution_status) {
  scoped_ptr<base::Environment> env(base::Environment::Create());
  std::string version_str;

  // Get the version variable and remove it from the environment.
  if (!env->GetVar(chrome::kChromeVersionEnvVar, &version_str))
    version_str.clear();

  base::FilePath exe_path;
  if (!PathService::Get(base::FILE_EXE, &exe_path))
    return;

  base::FilePath path(exe_path.DirName());

  // The relauncher is ordinarily in the version directory.  When running in a
  // build tree however (where CHROME_VERSION is not set in the environment)
  // look for it in Chrome's directory.
  if (!version_str.empty())
    path = path.AppendASCII(version_str);

  path = path.Append(installer::kDelegateExecuteExe);

  // Actually launching the process needs to happen in the metro viewer,
  // otherwise it won't automatically transition to desktop.  So we have
  // to send an IPC to the viewer to do the ShellExecute.
  ChromeMetroViewerProcessHost::HandleActivateDesktop(
      path, ash_execution_status == ASH_TERMINATE);
}
#endif

void AttemptRestartToDesktopMode() {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetString(prefs::kRelaunchMode,
                   upgrade_util::kRelaunchModeDesktop);

  AttemptRestart();
}

void AttemptRestartToMetroMode() {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetString(prefs::kRelaunchMode,
                   upgrade_util::kRelaunchModeMetro);
  AttemptRestart();
}

}  // namespace chrome
