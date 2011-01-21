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

static const FilePath::CharType kAutostart[] = "autostart";
static const FilePath::CharType kConfig[] = ".config";
static const char kXdgConfigHome[] = "XDG_CONFIG_HOME";

FilePath GetAutostartDirectory(base::Environment* environment) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  FilePath result =
      base::nix::GetXDGDirectory(environment, kXdgConfigHome, kConfig);
  result = result.Append(kAutostart);
  return result;
}

FilePath GetAutostartFilename(base::Environment* environment) {
  FilePath directory = GetAutostartDirectory(environment);
  return directory.Append(ShellIntegration::GetDesktopName(environment));
}

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
  if (!file_util::Delete(GetAutostartFilename(environment.get()), false)) {
    NOTREACHED() << "Failed to deregister launch on login.";
  }
}

// TODO(rickcam): Bug 56280: Share implementation with ShellIntegration
void EnableLaunchOnStartupTask::Run() {
  scoped_ptr<base::Environment> environment(base::Environment::Create());
  scoped_ptr<chrome::VersionInfo> version_info(new chrome::VersionInfo());
  FilePath autostart_directory = GetAutostartDirectory(environment.get());
  FilePath autostart_file = GetAutostartFilename(environment.get());
  if (!file_util::DirectoryExists(autostart_directory) &&
      !file_util::CreateDirectory(autostart_directory)) {
    NOTREACHED()
      << "Failed to register launch on login.  No autostart directory.";
    return;
  }
  std::string wrapper_script;
  if (!environment->GetVar("CHROME_WRAPPER", &wrapper_script)) {
    LOG(WARNING)
        << "Failed to register launch on login.  CHROME_WRAPPER not set.";
    return;
  }
  std::string autostart_file_contents =
      "[Desktop Entry]\n"
      "Type=Application\n"
      "Terminal=false\n"
      "Exec=" + wrapper_script +
      " --enable-background-mode --no-startup-window\n"
      "Name=" + version_info->Name() + "\n";
  std::string::size_type content_length = autostart_file_contents.length();
  if (file_util::WriteFile(autostart_file, autostart_file_contents.c_str(),
                           content_length) !=
      static_cast<int>(content_length)) {
    NOTREACHED() << "Failed to register launch on login.  Failed to write "
                 << autostart_file.value();
    file_util::Delete(GetAutostartFilename(environment.get()), false);
  }
}

string16 BackgroundModeManager::GetPreferencesMenuLabel() {
  string16 result = gtk_util::GetStockPreferencesMenuLabel();
  if (!result.empty())
    return result;
  return l10n_util::GetStringUTF16(IDS_PREFERENCES);
}
