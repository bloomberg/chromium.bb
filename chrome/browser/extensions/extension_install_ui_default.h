// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_UI_DEFAULT_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_UI_DEFAULT_H_

#include "chrome/browser/extensions/extension_install_ui.h"

class InfoBarDelegate;
class Profile;

namespace content {
class WebContents;
}

class ExtensionInstallUIDefault : public ExtensionInstallUI {
 public:
  explicit ExtensionInstallUIDefault(Profile* profile);
  virtual ~ExtensionInstallUIDefault();

  // ExtensionInstallUI implementation:
  virtual void OnInstallSuccess(const extensions::Extension* extension,
                                SkBitmap* icon) OVERRIDE;
  virtual void OnInstallFailure(
      const extensions::CrxInstallerError& error) OVERRIDE;
  virtual void SetSkipPostInstallUI(bool skip_ui) OVERRIDE;
  virtual void SetUseAppInstalledBubble(bool use_bubble) OVERRIDE;

 private:
  // Whether or not to show the default UI after completing the installation.
  bool skip_post_install_ui_;

  // Used to undo theme installation.
  std::string previous_theme_id_;
  bool previous_using_native_theme_;

  // Whether to show an installed bubble on app install, or use the default
  // action of opening a new tab page.
  bool use_app_installed_bubble_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ExtensionInstallUIDefault);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_UI_DEFAULT_H_
