// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sync_promo_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/sync_setup_flow.h"
#include "chrome/common/url_constants.h"
#include "content/browser/tab_contents/tab_contents.h"

SyncPromoHandler::SyncPromoHandler(ProfileManager* profile_manager)
    : SyncSetupHandler(profile_manager) {
}

SyncPromoHandler::~SyncPromoHandler() {
}

WebUIMessageHandler* SyncPromoHandler::Attach(WebUI* web_ui) {
  DCHECK(web_ui);
  return SyncSetupHandler::Attach(web_ui);
}

void SyncPromoHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("InitializeSyncPromo",
      base::Bind(&SyncPromoHandler::HandleInitializeSyncPromo,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("CloseSyncPromo",
      base::Bind(&SyncPromoHandler::HandleCloseSyncPromo,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("showAdvancedSyncSettings",
      base::Bind(&SyncPromoHandler::HandleShowAdvancedSyncSettings,
                 base::Unretained(this)));
  SyncSetupHandler::RegisterMessages();
}

void SyncPromoHandler::ShowConfigure(const base::DictionaryValue& args) {
  bool usePassphrase = false;
  args.GetBoolean("usePassphrase", &usePassphrase);

  if (usePassphrase) {
    // If a passphrase is required then we must show the configure pane.
    SyncSetupHandler::ShowConfigure(args);
  } else {
    // If no passphrase is required then skip the configure pane and sync
    // everything by default. This makes the first run experience simpler.
    // Note, there's an advanced link in the sync promo that takes users
    // to Settings where the configure pane is not skipped.
    SyncConfiguration configuration;
    configuration.sync_everything = true;
    DCHECK(flow());
    flow()->OnUserConfigured(configuration);
  }
}

void SyncPromoHandler::ShowSetupUI() {
  ProfileSyncService* service =
      Profile::FromWebUI(web_ui_)->GetProfileSyncService();
  service->get_wizard().Step(SyncSetupWizard::GetLoginState());
}

void SyncPromoHandler::HandleInitializeSyncPromo(const base::ListValue* args) {
  OpenSyncSetup();
}

void SyncPromoHandler::HandleCloseSyncPromo(const base::ListValue* args) {
  CloseSyncSetup();
  web_ui_->tab_contents()->OpenURL(GURL(chrome::kChromeUINewTabURL),
                                   GURL(), CURRENT_TAB,
                                   PageTransition::LINK);
}

void SyncPromoHandler::HandleShowAdvancedSyncSettings(
    const base::ListValue* args) {
  CloseSyncSetup();
  std::string url(chrome::kChromeUISettingsURL);
  url += chrome::kSyncSetupSubPage;
  web_ui_->tab_contents()->OpenURL(GURL(url), GURL(), CURRENT_TAB,
                                   PageTransition::LINK);
}
