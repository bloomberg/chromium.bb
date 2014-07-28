// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_UI_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_UI_H_

#include <string>

#include "base/basictypes.h"

class Browser;
class ExtensionInstallPrompt;
class Profile;
class SkBitmap;

namespace extensions {
class CrxInstallerError;
class Extension;
class ExtensionWebstorePrivateApiTest;
}

// Interface that should be implemented for each platform to display all the UI
// around extension installation.
class ExtensionInstallUI {
 public:
  static ExtensionInstallUI* Create(Profile* profile);

  virtual ~ExtensionInstallUI();

  // Called when an extension was installed.
  virtual void OnInstallSuccess(const extensions::Extension* extension,
                                const SkBitmap* icon) = 0;

  // Called when an extension failed to install.
  virtual void OnInstallFailure(const extensions::CrxInstallerError& error) = 0;


  // TODO(asargent) Normally we navigate to the new tab page when an app is
  // installed, but we're experimenting with instead showing a bubble when
  // an app is installed which points to the new tab button. This may become
  // the default behavior in the future.
  virtual void SetUseAppInstalledBubble(bool use_bubble) = 0;

  // Whether or not to show the default UI after completing the installation.
  void set_skip_post_install_ui(bool skip_ui) {
    skip_post_install_ui_ = skip_ui;
  }

  // Opens apps UI and animates the app icon for the app with id |app_id|.
  static void OpenAppInstalledUI(Profile* profile, const std::string& app_id);

#if defined(UNIT_TEST)
  static void set_disable_failure_ui_for_tests() {
    disable_failure_ui_for_tests_ = true;
  }
#endif

  // Creates an ExtensionInstallPrompt from |browser|.
  // Caller assumes ownership.
  static ExtensionInstallPrompt* CreateInstallPromptWithBrowser(
      Browser* browser);

  // Creates an ExtensionInstallPrompt from |profile|.
  // Caller assumes ownership. This method is deprecated and should not be used
  // in new code.
  static ExtensionInstallPrompt* CreateInstallPromptWithProfile(
      Profile* profile);

  Profile* profile() { return profile_; }

 protected:
  explicit ExtensionInstallUI(Profile* profile);

  static bool disable_failure_ui_for_tests() {
    return disable_failure_ui_for_tests_;
  }

  bool skip_post_install_ui() const { return skip_post_install_ui_; }

 private:
  static bool disable_failure_ui_for_tests_;

  Profile* profile_;

  // Whether or not to show the default UI after completing the installation.
  bool skip_post_install_ui_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInstallUI);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_UI_H_
