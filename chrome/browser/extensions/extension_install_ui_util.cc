// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_install_ui_util.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/extension_install_ui.h"

namespace extensions {
namespace install_ui {

void ShowPostInstallUIForApproval(Profile* profile,
                                  const WebstoreInstaller::Approval& approval,
                                  const Extension* extension) {
  scoped_ptr<ExtensionInstallUI> install_ui(
      ExtensionInstallUI::Create(profile));
  install_ui->SetUseAppInstalledBubble(approval.use_app_installed_bubble);
  install_ui->set_skip_post_install_ui(approval.skip_post_install_ui);
  install_ui->OnInstallSuccess(extension, approval.installing_icon.bitmap());
}

}  // namespace install_ui
}  // namespace extensions
