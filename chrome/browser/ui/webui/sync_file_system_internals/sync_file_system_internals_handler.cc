// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sync_file_system_internals/sync_file_system_internals_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/values.h"
#include "chrome/browser/drive/drive_notification_manager.h"
#include "chrome/browser/drive/drive_notification_manager_factory.h"
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

using drive::EventLogger;
using sync_file_system::SyncFileSystemServiceFactory;
using sync_file_system::SyncServiceState;

namespace syncfs_internals {

SyncFileSystemInternalsHandler::SyncFileSystemInternalsHandler(Profile* profile)
    : profile_(profile) {
  sync_file_system::SyncFileSystemService* sync_service =
      SyncFileSystemServiceFactory::GetForProfile(profile);
  DCHECK(sync_service);
  sync_service->AddSyncEventObserver(this);
}

SyncFileSystemInternalsHandler::~SyncFileSystemInternalsHandler() {
  sync_file_system::SyncFileSystemService* sync_service =
      SyncFileSystemServiceFactory::GetForProfile(profile_);
  if (sync_service != NULL)
    sync_service->RemoveSyncEventObserver(this);
}

void SyncFileSystemInternalsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getServiceStatus",
      base::Bind(&SyncFileSystemInternalsHandler::GetServiceStatus,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getLog",
      base::Bind(&SyncFileSystemInternalsHandler::GetLog,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getNotificationSource",
      base::Bind(&SyncFileSystemInternalsHandler::GetNotificationSource,
                 base::Unretained(this)));
}

void SyncFileSystemInternalsHandler::OnSyncStateUpdated(
    const GURL& app_origin,
    sync_file_system::SyncServiceState state,
    const std::string& description) {
  std::string state_string = extensions::api::sync_file_system::ToString(
        extensions::SyncServiceStateToExtensionEnum(state));
  if (!description.empty())
    state_string += " (" + description + ")";

  // TODO(calvinlo): OnSyncStateUpdated should be updated to also provide the
  // notification mechanism (XMPP or Polling).
  web_ui()->CallJavascriptFunction("SyncService.onGetServiceStatus",
                                   base::StringValue(state_string));
}

void SyncFileSystemInternalsHandler::OnFileSynced(
    const fileapi::FileSystemURL& url,
    sync_file_system::SyncFileStatus status,
    sync_file_system::SyncAction action,
    sync_file_system::SyncDirection direction) {}

void SyncFileSystemInternalsHandler::GetServiceStatus(
    const base::ListValue* args) {
  SyncServiceState state_enum = SyncFileSystemServiceFactory::GetForProfile(
      profile_)->GetSyncServiceState();
  std::string state_string = extensions::api::sync_file_system::ToString(
      extensions::SyncServiceStateToExtensionEnum(state_enum));
  web_ui()->CallJavascriptFunction("SyncService.onGetServiceStatus",
                                   base::StringValue(state_string));
}

void SyncFileSystemInternalsHandler::GetNotificationSource(
    const base::ListValue* args) {
  drive::DriveNotificationManager* drive_notification_manager =
      drive::DriveNotificationManagerFactory::GetForBrowserContext(profile_);
  bool xmpp_enabled = drive_notification_manager->push_notification_enabled();
  std::string notification_source = xmpp_enabled ? "XMPP" : "Polling";
  web_ui()->CallJavascriptFunction("SyncService.onGetNotificationSource",
                                   base::StringValue(notification_source));
}

void SyncFileSystemInternalsHandler::GetLog(
    const base::ListValue* args) {
  const std::vector<EventLogger::Event> log =
      sync_file_system::util::GetLogHistory();

  int last_log_id_sent;
  if (!args->GetInteger(0, &last_log_id_sent))
    last_log_id_sent = -1;

  // Collate events which haven't been sent to WebUI yet.
  base::ListValue list;
  for (std::vector<EventLogger::Event>::const_iterator log_entry = log.begin();
       log_entry != log.end();
       ++log_entry) {
    if (log_entry->id <= last_log_id_sent)
      continue;

    base::DictionaryValue* dict = new DictionaryValue;
    dict->SetInteger("id", log_entry->id);
    dict->SetString("time",
        google_apis::util::FormatTimeAsStringLocaltime(log_entry->when));
    dict->SetString("logEvent", log_entry->what);
    list.Append(dict);
    last_log_id_sent = log_entry->id;
  }
  if (list.empty())
    return;

  web_ui()->CallJavascriptFunction("SyncService.onGetLog", list);
}

}  // namespace syncfs_internals
