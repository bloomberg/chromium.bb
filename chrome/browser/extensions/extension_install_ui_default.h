// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_UI_DEFAULT_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_UI_DEFAULT_H_
#pragma once

#include "chrome/browser/extensions/extension_install_ui.h"

class InfoBarDelegate;
class Profile;
class TabContents;

class ExtensionInstallUIDefault : public ExtensionInstallUI {
 public:
  explicit ExtensionInstallUIDefault(Browser* browser);
  virtual ~ExtensionInstallUIDefault();

  // ExtensionInstallUI implementation:
  virtual void OnInstallSuccess(const extensions::Extension* extension,
                                SkBitmap* icon) OVERRIDE;
  virtual void OnInstallFailure(const string16& error) OVERRIDE;
  virtual void SetSkipPostInstallUI(bool skip_ui) OVERRIDE;
  virtual void SetUseAppInstalledBubble(bool use_bubble) OVERRIDE;

 private:
  // Shows an infobar for a newly-installed theme.  previous_theme_id should be
  // empty if the previous theme was the system/default theme.
  static void ShowThemeInfoBar(const std::string& previous_theme_id,
                               bool previous_using_native_theme,
                               const extensions::Extension* new_theme,
                               Profile* profile);

  // Returns the delegate to control the browser's info bar. This is
  // within its own function due to its platform-specific nature.
  static InfoBarDelegate* GetNewThemeInstalledInfoBarDelegate(
      TabContents* tab_contents,
      const extensions::Extension* new_theme,
      const std::string& previous_theme_id,
      bool previous_using_native_theme);

  Browser* browser_;

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
