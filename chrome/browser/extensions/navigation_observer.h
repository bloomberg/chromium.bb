// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_NAVIGATION_OBSERVER_H_
#define CHROME_BROWSER_EXTENSIONS_NAVIGATION_OBSERVER_H_

#include <set>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Profile;

namespace content {
class NavigationController;
}

namespace extensions {

// The NavigationObserver listens to navigation notifications. If the user
// navigates into an extension that has been disabled due to a permission
// increase, it prompts the user to accept the new permissions and re-enables
// the extension.
class NavigationObserver : public ExtensionInstallPrompt::Delegate,
                           public content::NotificationObserver {
 public:
  explicit NavigationObserver(Profile* profile);
  virtual ~NavigationObserver();

  // content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;
 private:
  // Registers for the NOTIFICATION_NAV_ENTRY_COMMITTED notification.
  void RegisterForNotifications();

  // Checks if |nav_controller| has entered an extension's web extent. If it
  // has and the extension is disabled due to a permissions increase, this
  // prompts the user to accept the new permissions and enables the extension.
  void PromptToEnableExtensionIfNecessary(
      content::NavigationController* nav_controller);

  // ExtensionInstallPrompt::Delegate callbacks used for the permissions prompt.
  virtual void InstallUIProceed() OVERRIDE;
  virtual void InstallUIAbort(bool user_initiated) OVERRIDE;

  content::NotificationRegistrar registrar_;

  Profile* profile_;

  // The UI used to confirm enabling extensions.
  scoped_ptr<ExtensionInstallPrompt> extension_install_prompt_;

  // The data we keep track of when prompting to enable extensions.
  std::string in_progress_prompt_extension_id_;
  content::NavigationController* in_progress_prompt_navigation_controller_;

  // The extension ids we've already prompted the user about.
  std::set<std::string> prompted_extensions_;

  DISALLOW_COPY_AND_ASSIGN(NavigationObserver);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_NAVIGATION_OBSERVER_H_
