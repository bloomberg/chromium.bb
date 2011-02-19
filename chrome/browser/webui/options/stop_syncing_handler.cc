// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webui/options/stop_syncing_handler.h"

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

StopSyncingHandler::StopSyncingHandler() {
}

StopSyncingHandler::~StopSyncingHandler() {
}

void StopSyncingHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  localized_strings->SetString("stop_syncing_explanation",
      l10n_util::GetStringUTF16(IDS_SYNC_STOP_SYNCING_EXPLANATION_LABEL));
  localized_strings->SetString("stop_syncing_title",
      l10n_util::GetStringUTF16(IDS_SYNC_STOP_SYNCING_DIALOG_TITLE));
  localized_strings->SetString("stop_syncing_confirm",
      l10n_util::GetStringUTF16(IDS_SYNC_STOP_SYNCING_CONFIRM_BUTTON_LABEL));
}

void StopSyncingHandler::RegisterMessages() {
  DCHECK(web_ui_);
  web_ui_->RegisterMessageCallback("stopSyncing",
      NewCallback(this, &StopSyncingHandler::StopSyncing));
}

void StopSyncingHandler::StopSyncing(const ListValue* args){
  ProfileSyncService* service = web_ui_->GetProfile()->GetProfileSyncService();
  if (service != NULL && ProfileSyncService::IsSyncEnabled()) {
    service->DisableForUser();
    ProfileSyncService::SyncEvent(ProfileSyncService::STOP_FROM_OPTIONS);
  }
}
