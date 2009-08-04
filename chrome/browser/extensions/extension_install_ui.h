// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_UI_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_UI_H_

#include "base/file_path.h"
#include "base/gfx/native_widget_types.h"
#include "base/ref_counted.h"
#include "chrome/browser/extensions/crx_installer.h"

class ExtensionsService;
class MessageLoop;
class Profile;
class SandboxedExtensionUnpacker;

// Displays all the UI around extension installation.
class ExtensionInstallUI : public CrxInstallerClient {
 public:
  // NOTE: The implementation of this is platform-specific.
  static void ShowExtensionInstallPrompt(Profile* profile,
                                         CrxInstaller* installer,
                                         Extension* extension,
                                         SkBitmap* install_icon);

  ExtensionInstallUI(Profile* profile);

 private:
  // CrxInstallerClient
  virtual void ConfirmInstall(CrxInstaller* installer, Extension* extension,
                              SkBitmap* icon);
  virtual void OnInstallSuccess(Extension* extension);
  virtual void OnInstallFailure(const std::string& error);
  virtual void OnOverinstallAttempted(Extension* extension);

  void ShowThemeInfoBar(Extension* extension);

  Profile* profile_;
  MessageLoop* ui_loop_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_UI_H_
