// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_enable_flow.h"

#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/ui/extensions/extension_enable_flow_delegate.h"

using extensions::Extension;

ExtensionEnableFlow::ExtensionEnableFlow(Profile* profile,
                                         const std::string& extension_id,
                                         ExtensionEnableFlowDelegate* delegate)
    : profile_(profile),
      extension_id_(extension_id),
      delegate_(delegate) {
}

ExtensionEnableFlow::~ExtensionEnableFlow() {
}

void ExtensionEnableFlow::Start() {
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  const Extension* extension = service->GetExtensionById(extension_id_, true);
  if (!extension) {
    extension = service->GetTerminatedExtension(extension_id_);
    // It's possible (though unlikely) the app could have been uninstalled since
    // the user clicked on it.
    if (!extension)
      return;
    // If the app was terminated, reload it first. (This reallocates the
    // Extension object.)
    service->ReloadExtension(extension_id_);
    extension = service->GetExtensionById(extension_id_, true);
  }

  extensions::ExtensionPrefs* extension_prefs = service->extension_prefs();
  if (!extension_prefs->DidExtensionEscalatePermissions(extension_id_)) {
    // Enable the extension immediately if its privileges weren't escalated.
    // This is a no-op if the extension was previously terminated.
    service->EnableExtension(extension_id_);

    delegate_->ExtensionEnableFlowFinished();  // |delegate_| may delete us.
    return;
  }

  if (!prompt_)
    prompt_.reset(delegate_->CreateExtensionInstallPrompt());
  prompt_->ConfirmReEnable(this, extension);
}

void ExtensionEnableFlow::InstallUIProceed() {
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();

  // The extension can be uninstalled in another window while the UI was
  // showing. Do nothing in that case.
  const Extension* extension = service->GetExtensionById(extension_id_, true);
  if (!extension)
    return;

  service->GrantPermissionsAndEnableExtension(extension,
                                              prompt_->record_oauth2_grant());
  delegate_->ExtensionEnableFlowFinished();  // |delegate_| may delete us.
}

void ExtensionEnableFlow::InstallUIAbort(bool user_initiated) {
  delegate_->ExtensionEnableFlowAborted(user_initiated);
  // |delegate_| may delete us.
}
