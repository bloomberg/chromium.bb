// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sync_file_system_internals/sync_file_system_internals_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/sync_file_system/sync_file_system_api_helpers.h"
#include "chrome/browser/google_apis/time_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync_file_system/logger.h"
#include "chrome/browser/sync_file_system/sync_file_system_service.h"
#include "chrome/browser/sync_file_system/sync_file_system_service_factory.h"
#include "chrome/browser/sync_file_system/sync_service_state.h"
#include "chrome/common/extensions/api/sync_file_system.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_ui.h"

using google_apis::EventLogger;
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
  web_ui()->RegisterMessageCallback(
      "getLog",
      base::Bind(&SyncFileSystemInternalsHandler::GetLog,
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

void SyncFileSystemInternalsHandler::GetLog(
    const base::ListValue* args) {
  const std::vector<EventLogger::Event> log =
      sync_file_system::util::GetLogHistory();

  // Collate events which haven't been sent to WebUI yet.
  base::ListValue list;
  for (std::vector<EventLogger::Event>::const_iterator log_entry = log.begin();
       log_entry != log.end();
       ++log_entry) {
    if (log_entry->id <= last_log_id_sent_)
      continue;

    base::DictionaryValue* dict = new DictionaryValue;
    dict->SetString("time",
        google_apis::util::FormatTimeAsStringLocaltime(log_entry->when));
    dict->SetString("logEvent", log_entry->what);
    list.Append(dict);
    last_log_id_sent_ = log_entry->id;
  }
  if (list.empty())
    return;

  web_ui()->CallJavascriptFunction("syncService.onGetLog", list);
}

}  // namespace syncfs_internals
