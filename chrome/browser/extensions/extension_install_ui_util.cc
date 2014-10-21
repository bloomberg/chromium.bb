// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_install_ui_util.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/extensions/extension_install_ui_factory.h"
#include "extensions/browser/install/extension_install_ui.h"

namespace extensions {
namespace install_ui {

void ShowPostInstallUIForApproval(content::BrowserContext* context,
                                  const WebstoreInstaller::Approval& approval,
                                  const Extension* extension) {
  scoped_ptr<ExtensionInstallUI> install_ui(
      extensions::CreateExtensionInstallUI(context));
  install_ui->SetUseAppInstalledBubble(approval.use_app_installed_bubble);
  install_ui->SetSkipPostInstallUI(approval.skip_post_install_ui);
  install_ui->OnInstallSuccess(extension, approval.installing_icon.bitmap());
}

}  // namespace install_ui
}  // namespace extensions
