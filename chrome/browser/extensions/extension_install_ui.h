// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_UI_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_UI_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/string16.h"
#include "chrome/browser/extensions/crx_installer_error.h"

class Browser;
class ExtensionInstallPrompt;
class Profile;
class SkBitmap;

namespace content {
class WebContents;
}

namespace extensions {
class Extension;
class ExtensionWebstorePrivateApiTest;
}  // namespace extensions

// Interface that should be implemented for each platform to display all the UI
// around extension installation.
class ExtensionInstallUI {
 public:
  static ExtensionInstallUI* Create(Profile* profile);

  virtual ~ExtensionInstallUI();

  // Called when an extension was installed.
  virtual void OnInstallSuccess(const extensions::Extension* extension,
                                SkBitmap* icon) = 0;
  // Called when an extension failed to install.
  virtual void OnInstallFailure(const extensions::CrxInstallerError& error) = 0;

  // Whether or not to show the default UI after completing the installation.
  virtual void SetSkipPostInstallUI(bool skip_ui) = 0;

  // TODO(asargent) Normally we navigate to the new tab page when an app is
  // installed, but we're experimenting with instead showing a bubble when
  // an app is installed which points to the new tab button. This may become
  // the default behavior in the future.
  virtual void SetUseAppInstalledBubble(bool use_bubble) = 0;

  // Opens apps UI and animates the app icon for the app with id |app_id|.
  static void OpenAppInstalledUI(Profile* profile, const std::string& app_id);

  // Disables showing UI (ErrorBox, etc.) for install failures. To be used only
  // in tests.
  static void DisableFailureUIForTests();

  // Creates an ExtensionInstallPrompt from |browser|.
  // Caller assumes ownership.
  static ExtensionInstallPrompt* CreateInstallPromptWithBrowser(
      Browser* browser);

  // Creates an ExtensionInstallPrompt from |profile|.
  // Caller assumes ownership. This method is deperecated
  // and should not be used in new code.
  static ExtensionInstallPrompt* CreateInstallPromptWithProfile(
      Profile* profile);

  Profile* profile() { return profile_; }

 protected:
  ExtensionInstallUI();

  Profile* profile_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_UI_H_
