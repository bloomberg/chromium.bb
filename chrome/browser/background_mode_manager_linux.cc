// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unistd.h>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/nix/xdg_util.h"
#include "base/task.h"
#include "chrome/browser/background_mode_manager.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/common/auto_start_linux.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

class DisableLaunchOnStartupTask : public Task {
 public:
  virtual void Run();
};

class EnableLaunchOnStartupTask : public Task {
 public:
  virtual void Run();
};

}  // namespace

void BackgroundModeManager::EnableLaunchOnStartup(bool should_launch) {
  // This functionality is only defined for default profile, currently.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kUserDataDir))
    return;
  if (should_launch) {
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                            new EnableLaunchOnStartupTask());
  } else {
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                            new DisableLaunchOnStartupTask());
  }
}

void DisableLaunchOnStartupTask::Run() {
  scoped_ptr<base::Environment> environment(base::Environment::Create());
  if (!AutoStart::Remove(ShellIntegration::GetDesktopName(environment.get()))) {
    NOTREACHED() << "Failed to deregister launch on login.";
  }
}

// TODO(rickcam): Bug 56280: Share implementation with ShellIntegration
void EnableLaunchOnStartupTask::Run() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  scoped_ptr<base::Environment> environment(base::Environment::Create());
  scoped_ptr<chrome::VersionInfo> version_info(new chrome::VersionInfo());

  std::string wrapper_script;
  if (!environment->GetVar("CHROME_WRAPPER", &wrapper_script)) {
    LOG(WARNING)
        << "Failed to register launch on login.  CHROME_WRAPPER not set.";
    return;
  }
  std::string command_line =
      wrapper_script + " --no-startup-window";
  if (!AutoStart::AddApplication(
          ShellIntegration::GetDesktopName(environment.get()),
          version_info->Name(),
          command_line,
          false)) {
    NOTREACHED() << "Failed to register launch on login.";
  }
}

string16 BackgroundModeManager::GetPreferencesMenuLabel() {
  string16 result = gtk_util::GetStockPreferencesMenuLabel();
  if (!result.empty())
    return result;
  return l10n_util::GetStringUTF16(IDS_PREFERENCES);
}
