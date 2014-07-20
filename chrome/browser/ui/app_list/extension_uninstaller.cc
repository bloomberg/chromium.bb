// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/extension_uninstaller.h"

#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension.h"

ExtensionUninstaller::ExtensionUninstaller(
    Profile* profile,
    const std::string& extension_id,
    AppListControllerDelegate* controller)
    : profile_(profile),
      app_id_(extension_id),
      controller_(controller) {
}

ExtensionUninstaller::~ExtensionUninstaller() {
}

void ExtensionUninstaller::Run() {
  const extensions::Extension* extension =
      extensions::ExtensionSystem::Get(profile_)->extension_service()->
          GetInstalledExtension(app_id_);
  if (!extension) {
    CleanUp();
    return;
  }
  controller_->OnShowChildDialog();
  dialog_.reset(
      extensions::ExtensionUninstallDialog::Create(profile_, NULL, this));
  dialog_->ConfirmUninstall(extension);
}

void ExtensionUninstaller::ExtensionUninstallAccepted() {
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  const extensions::Extension* extension =
      service->GetInstalledExtension(app_id_);
  if (extension) {
    service->UninstallExtension(
        app_id_, extensions::UNINSTALL_REASON_USER_INITIATED, NULL);
  }
  controller_->OnCloseChildDialog();
  CleanUp();
}

void ExtensionUninstaller::ExtensionUninstallCanceled() {
  controller_->OnCloseChildDialog();
  CleanUp();
}

void ExtensionUninstaller::CleanUp() {
  delete this;
}
