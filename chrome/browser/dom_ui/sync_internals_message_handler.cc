// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/sync_internals_message_handler.h"

#include "base/callback.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/sync_ui_util.h"

SyncInternalsMessageHandler::SyncInternalsMessageHandler(Profile* profile)
    : profile_(profile) {
  DCHECK(profile_);
  ProfileSyncService* service = profile_->GetProfileSyncService();
  if (service) {
    service->AddObserver(this);
  }
  // TODO(akalin): Listen for when the service gets created/destroyed.
}

SyncInternalsMessageHandler::~SyncInternalsMessageHandler() {
  ProfileSyncService* service = profile_->GetProfileSyncService();
  if (service) {
    service->RemoveObserver(this);
  }
}

void SyncInternalsMessageHandler::OnStateChanged() {
  dom_ui_->CallJavascriptFunction(L"onSyncServiceStateChanged");
}

void SyncInternalsMessageHandler::RegisterMessages() {
  dom_ui_->RegisterMessageCallback(
      "getAboutInfo",
      NewCallback(
          this, &SyncInternalsMessageHandler::HandleGetAboutInfo));
}

void SyncInternalsMessageHandler::HandleGetAboutInfo(const ListValue* args) {
  ProfileSyncService* service = profile_->GetProfileSyncService();
  DictionaryValue about_info;
  sync_ui_util::ConstructAboutInformation(service, &about_info);

  dom_ui_->CallJavascriptFunction(L"onGetAboutInfoFinished", about_info);
}
