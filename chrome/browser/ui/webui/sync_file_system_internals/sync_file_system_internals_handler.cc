// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sync_file_system_internals/sync_file_system_internals_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/sync_file_system/sync_file_system_api_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync_file_system/sync_file_system_service.h"
#include "chrome/browser/sync_file_system/sync_file_system_service_factory.h"
#include "chrome/browser/sync_file_system/sync_service_state.h"
#include "chrome/common/extensions/api/sync_file_system.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_ui.h"

using sync_file_system::SyncFileSystemServiceFactory;
using sync_file_system::SyncServiceState;

namespace syncfs_internals {

SyncFileSystemInternalsHandler::SyncFileSystemInternalsHandler(Profile* profile)
    : profile_(profile) {}

SyncFileSystemInternalsHandler::~SyncFileSystemInternalsHandler() {}

void SyncFileSystemInternalsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getServiceStatus",
      base::Bind(&SyncFileSystemInternalsHandler::GetServiceStatus,
                 base::Unretained(this)));
}

void SyncFileSystemInternalsHandler::GetServiceStatus(
    const base::ListValue* args) {
  SyncServiceState state_enum = SyncFileSystemServiceFactory::GetForProfile(
      profile_)->GetSyncServiceState();
  std::string state_string = extensions::api::sync_file_system::ToString(
      extensions::SyncServiceStateToExtensionEnum(state_enum));
  web_ui()->CallJavascriptFunction("syncService.getServiceStatusResult",
                                   base::StringValue(state_string));
}

}  // namespace syncfs_internals
