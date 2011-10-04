// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sync_promo_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/common/url_constants.h"
#include "content/browser/tab_contents/tab_contents.h"

SyncPromoHandler::SyncPromoHandler() {
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
  SyncSetupHandler::RegisterMessages();
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
