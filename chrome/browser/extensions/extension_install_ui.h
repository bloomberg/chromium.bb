// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_UI_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_UI_H_

#include "base/file_path.h"
#include "base/gfx/native_widget_types.h"
#include "base/ref_counted.h"

#include <string>

class Extension;
class ExtensionsService;
class MessageLoop;
class Profile;
class SandboxedExtensionUnpacker;
class SkBitmap;

// Displays all the UI around extension installation.
class ExtensionInstallUI {
 public:
  class Delegate {
   public:
    // We call this method after ConfirmInstall() to signal that the
    // installation should continue.
    virtual void ContinueInstall() = 0;

    // We call this method after ConfirmInstall() to signal that the
    // installation should stop.
    virtual void AbortInstall() = 0;
  };

  // NOTE: The implementation of this is platform-specific.
  static void ShowExtensionInstallPrompt(Profile* profile,
                                         Delegate* delegate,
                                         Extension* extension,
                                         SkBitmap* install_icon,
                                         const std::wstring& warning_text);

  ExtensionInstallUI(Profile* profile);

  // This is called by the installer to verify whether the installation should
  // proceed.
  //
  // We *MUST* eventually call either ContinueInstall() or AbortInstall()
  // on |delegate|.
  void ConfirmInstall(Delegate* delegate, Extension* extension,
                      SkBitmap* icon);

  // Installation was successful.
  void OnInstallSuccess(Extension* extension);

  // Intallation failed.
  void OnInstallFailure(const std::string& error);

  // The install was rejected because the same extension/version is already
  // installed.
  void OnOverinstallAttempted(Extension* extension);

 private:
  void ShowThemeInfoBar(Extension* new_theme);

  Profile* profile_;
  MessageLoop* ui_loop_;
  std::string previous_theme_id_;  // Used to undo theme installation.
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_UI_H_
