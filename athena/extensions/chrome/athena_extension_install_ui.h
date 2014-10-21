// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_EXTENSIONS_CHROME_ATHENA_EXTENSION_INSTALL_UI_H_
#define ATHENA_EXTENSIONS_CHROME_ATHENA_EXTENSION_INSTALL_UI_H_

#include "extensions/browser/install/extension_install_ui.h"

namespace content {
class BrowserContext;
}

namespace athena {

class AthenaExtensionInstallUI : public extensions::ExtensionInstallUI {
 public:
  AthenaExtensionInstallUI();
  virtual ~AthenaExtensionInstallUI();

  // ExtensionInstallUI:
  virtual void OnInstallSuccess(const extensions::Extension* extension,
                                const SkBitmap* icon) override;
  virtual void OnInstallFailure(
      const extensions::CrxInstallerError& error) override;
  virtual void SetUseAppInstalledBubble(bool use_bubble) override;
  virtual void OpenAppInstalledUI(const std::string& app_id) override;
  virtual void SetSkipPostInstallUI(bool skip_ui) override;
  virtual gfx::NativeWindow GetDefaultInstallDialogParent() override;

 private:
  // Whether or not to show the default UI after completing the installation.
  bool skip_post_install_ui_;

  DISALLOW_COPY_AND_ASSIGN(AthenaExtensionInstallUI);
};

}  //  namespace athena

#endif  // ATHENA_EXTENSIONS_CHROME_ATHENA_EXTENSION_INSTALL_UI_H_
