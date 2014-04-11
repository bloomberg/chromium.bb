// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app.h"

#include "base/environment.h"
#include "base/logging.h"
#include "chrome/browser/shell_integration_linux.h"
#include "content/public/browser/browser_thread.h"

namespace web_app {

namespace internals {

bool CreatePlatformShortcuts(
    const base::FilePath& web_app_path,
    const ShellIntegration::ShortcutInfo& shortcut_info,
    const extensions::FileHandlersInfo& file_handlers_info,
    const ShellIntegration::ShortcutLocations& creation_locations,
    ShortcutCreationReason /*creation_reason*/) {
#if !defined(OS_CHROMEOS)
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  return ShellIntegrationLinux::CreateDesktopShortcut(
      shortcut_info, creation_locations);
#else
  return false;
#endif
}

void DeletePlatformShortcuts(
    const base::FilePath& web_app_path,
    const ShellIntegration::ShortcutInfo& shortcut_info) {
#if !defined(OS_CHROMEOS)
  ShellIntegrationLinux::DeleteDesktopShortcuts(shortcut_info.profile_path,
      shortcut_info.extension_id);
#endif
}

void UpdatePlatformShortcuts(
    const base::FilePath& web_app_path,
    const base::string16& /*old_app_title*/,
    const ShellIntegration::ShortcutInfo& shortcut_info,
    const extensions::FileHandlersInfo& file_handlers_info) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));

  scoped_ptr<base::Environment> env(base::Environment::Create());

  // Find out whether shortcuts are already installed.
  ShellIntegration::ShortcutLocations creation_locations =
      ShellIntegrationLinux::GetExistingShortcutLocations(
          env.get(), shortcut_info.profile_path, shortcut_info.extension_id);
  // Always create a hidden shortcut in applications if a visible one is not
  // being created. This allows the operating system to identify the app, but
  // not show it in the menu.
  creation_locations.hidden = true;

  CreatePlatformShortcuts(web_app_path,
                          shortcut_info,
                          file_handlers_info,
                          creation_locations,
                          SHORTCUT_CREATION_BY_USER);
}

void DeleteAllShortcutsForProfile(const base::FilePath& profile_path) {
#if !defined(OS_CHROMEOS)
  ShellIntegrationLinux::DeleteAllDesktopShortcuts(profile_path);
#endif
}

}  // namespace internals

}  // namespace web_app
