// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/options_sync_setup_handler.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"

void OptionsSyncSetupHandler::ShowSetupUI() {
  ProfileSyncService* service =
      web_ui_->GetProfile()->GetProfileSyncService();
  DCHECK(service);

  // If the wizard is already visible, focus it.
  if (service->get_wizard().IsVisible()) {
    web_ui_->CallJavascriptFunction("OptionsPage.closeOverlay");
    service->get_wizard().Focus();
    return;
  }

  // The user is trying to manually load a syncSetup URL.  We should bring up
  // either a login or a configure flow based on the state of sync.
  if (service->HasSyncSetupCompleted()) {
    if (service->IsPassphraseRequiredForDecryption()) {
      service->get_wizard().Step(SyncSetupWizard::ENTER_PASSPHRASE);
    } else {
      service->get_wizard().Step(SyncSetupWizard::CONFIGURE);
    }
  } else {
    service->get_wizard().Step(SyncSetupWizard::GetLoginState());
  }

  // Show the Sync Setup page.
  scoped_ptr<Value> page(Value::CreateStringValue("syncSetup"));
  web_ui_->CallJavascriptFunction("OptionsPage.navigateToPage", *page);
}
