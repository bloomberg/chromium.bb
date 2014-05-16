// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/shell/browser/default_shell_browser_main_delegate.h"

#include "apps/shell/browser/shell_extension_system.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"

namespace apps {

DefaultShellBrowserMainDelegate::DefaultShellBrowserMainDelegate() {
}

DefaultShellBrowserMainDelegate::~DefaultShellBrowserMainDelegate() {
}

void DefaultShellBrowserMainDelegate::Start(
    content::BrowserContext* browser_context) {
  const std::string kAppSwitch = "app";
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kAppSwitch)) {
    base::FilePath app_dir(command_line->GetSwitchValueNative(kAppSwitch));
    base::FilePath app_absolute_dir = base::MakeAbsoluteFilePath(app_dir);

    extensions::ShellExtensionSystem* extension_system =
        static_cast<extensions::ShellExtensionSystem*>(
            extensions::ExtensionSystem::Get(browser_context));
    extension_system->LoadAndLaunchApp(app_absolute_dir);
  } else {
    LOG(ERROR) << "--" << kAppSwitch << " unset; boredom is in your future";
  }
}

void DefaultShellBrowserMainDelegate::Shutdown() {
}

}  // namespace apps
