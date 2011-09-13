// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_NAVIGATION_OBSERVER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_NAVIGATION_OBSERVER_H_
#pragma once

#include <set>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "content/common/notification_registrar.h"

class Profile;
class NavigationController;

// The ExtensionNavigationObserver listens to navigation notifications. If the
// user navigates into an extension that has been disabled due to a permission
// increase, it prompts the user to accept the new permissions and re-enables
// the extension.
class ExtensionNavigationObserver : public ExtensionInstallUI::Delegate,
                                    public NotificationObserver {
 public:
  explicit ExtensionNavigationObserver(Profile* profile);
  virtual ~ExtensionNavigationObserver();

  // NotificationObserver
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;
 private:
  // Registers for the NOTIFICATION_NAV_ENTRY_COMMITTED notification.
  void RegisterForNotifications();

  // Checks if |nav_controller| has entered an extension's web extent. If it
  // has and the extension is disabled due to a permissions increase, this
  // prompts the user to accept the new permissions and enables the extension.
  void PromptToEnableExtensionIfNecessary(NavigationController* nav_controller);

  // ExtensionInstallUI::Delegate callbacks used for the permissions prompt.
  virtual void InstallUIProceed() OVERRIDE;
  virtual void InstallUIAbort(bool user_initiated) OVERRIDE;

  NotificationRegistrar registrar_;

  Profile* profile_;

  // The UI used to confirm enabling extensions.
  scoped_ptr<ExtensionInstallUI> extension_install_ui_;

  // The data we keep track of when prompting to enable extensions.
  std::string in_progress_prompt_extension_id_;
  NavigationController* in_progress_prompt_navigation_controller_;

  // The extension ids we've already prompted the user about.
  std::set<std::string> prompted_extensions_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_NAVIGATION_OBSERVER_H_
