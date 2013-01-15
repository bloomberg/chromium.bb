// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_APP_SHORTCUT_MANAGER_H_
#define CHROME_BROWSER_EXTENSIONS_APP_SHORTCUT_MANAGER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Profile;

namespace extensions {

// This class manages the installation of shortcuts for platform apps.
class AppShortcutManager : public content::NotificationObserver {
 public:
  explicit AppShortcutManager(Profile* profile);

  virtual ~AppShortcutManager();

  // content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  void UpdateApplicationShortcuts(const Extension* extension);

  // Implement ImageLoadingTracker::Observer. |tracker_| is used to
  // load the application's icon, which is done when we start creating an
  // application's shortcuts. This method receives the icon, and completes
  // the process of installing the shortcuts.
  void OnImageLoaded(const gfx::Image& image);

#if defined(OS_WIN)
  void OnAppHostInstallationComplete(scoped_refptr<Extension> extension,
                                     bool app_host_install_success);
#endif

  void DeleteApplicationShortcuts(const Extension* extension);

  content::NotificationRegistrar registrar_;
  Profile* profile_;

  // Fields used when installing application shortcuts.
  ShellIntegration::ShortcutInfo shortcut_info_;

  base::WeakPtrFactory<AppShortcutManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AppShortcutManager);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_APP_SHORTCUT_MANAGER_H_
