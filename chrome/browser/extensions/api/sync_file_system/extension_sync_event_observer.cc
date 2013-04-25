// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/sync_file_system/extension_sync_event_observer.h"

#include "chrome/browser/extensions/api/sync_file_system/sync_file_system_api_helpers.h"
#include "chrome/browser/extensions/event_names.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/sync_file_system/sync_event_observer.h"
#include "chrome/browser/sync_file_system/sync_file_system_service.h"
#include "chrome/common/extensions/api/sync_file_system.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/syncable/syncable_file_system_util.h"

using sync_file_system::SyncEventObserver;

namespace extensions {

ExtensionSyncEventObserver::ExtensionSyncEventObserver(
    Profile* profile)
    : profile_(profile),
      sync_service_(NULL) {}

void ExtensionSyncEventObserver::InitializeForService(
    sync_file_system::SyncFileSystemService* sync_service,
    const std::string& service_name) {
  DCHECK(sync_service);
  if (sync_service_ != NULL) {
    DCHECK_EQ(sync_service_, sync_service);
    return;
  }
  sync_service_ = sync_service;
  service_name_ = service_name;
  sync_service_->AddSyncEventObserver(this);
}

ExtensionSyncEventObserver::~ExtensionSyncEventObserver() {}

void ExtensionSyncEventObserver::Shutdown() {
  if (sync_service_ != NULL)
    sync_service_->RemoveSyncEventObserver(this);
}

std::string ExtensionSyncEventObserver::GetExtensionId(
    const GURL& app_origin) {
  const Extension* app = ExtensionSystem::Get(profile_)->extension_service()->
      GetInstalledApp(app_origin);
  if (!app) {
    // The app is uninstalled or disabled.
    return std::string();
  }
  return app->id();
}

void ExtensionSyncEventObserver::OnSyncStateUpdated(
    const GURL& app_origin,
    sync_file_system::SyncServiceState state,
    const std::string& description) {
  // Convert state and description into SyncState Object.
  api::sync_file_system::ServiceInfo service_info;
  service_info.state = SyncServiceStateToExtensionEnum(state);
  service_info.description = description;
  scoped_ptr<base::ListValue> params(
      api::sync_file_system::OnServiceStatusChanged::Create(service_info));

  BroadcastOrDispatchEvent(app_origin,
                           event_names::kOnServiceStatusChanged,
                           params.Pass());
}

void ExtensionSyncEventObserver::OnFileSynced(
    const fileapi::FileSystemURL& url,
    sync_file_system::SyncFileStatus status,
    sync_file_system::SyncAction action,
    sync_file_system::SyncDirection direction) {
  scoped_ptr<base::ListValue> params(new ListValue());

  // For now we always assume events come only for files (not directories).
  params->Append(CreateDictionaryValueForFileSystemEntry(
      url, sync_file_system::SYNC_FILE_TYPE_FILE));

  // Status, SyncAction and any optional notes to go here.
  api::sync_file_system::FileStatus status_enum =
      SyncFileStatusToExtensionEnum(status);
  api::sync_file_system::SyncAction action_enum =
      SyncActionToExtensionEnum(action);
  api::sync_file_system::SyncDirection direction_enum =
      SyncDirectionToExtensionEnum(direction);
  params->AppendString(api::sync_file_system::ToString(status_enum));
  params->AppendString(api::sync_file_system::ToString(action_enum));
  params->AppendString(api::sync_file_system::ToString(direction_enum));

  BroadcastOrDispatchEvent(url.origin(),
                           event_names::kOnFileStatusChanged,
                           params.Pass());
}

void ExtensionSyncEventObserver::BroadcastOrDispatchEvent(
    const GURL& app_origin,
    const std::string& event_name,
    scoped_ptr<base::ListValue> values) {
  // Check to see whether the event should be broadcasted to all listening
  // extensions or sent to a specific extension ID.
  bool broadcast_mode = app_origin.is_empty();
  EventRouter* event_router = ExtensionSystem::Get(profile_)->event_router();
  DCHECK(event_router);

  scoped_ptr<Event> event(new Event(event_name, values.Pass()));
  event->restrict_to_profile = profile_;

  // No app_origin, broadcast to all listening extensions for this event name.
  if (broadcast_mode) {
    event_router->BroadcastEvent(event.Pass());
    return;
  }

  // Dispatch to single extension ID.
  const std::string extension_id = GetExtensionId(app_origin);
  if (extension_id.empty())
    return;
  event_router->DispatchEventToExtension(extension_id, event.Pass());
}

}  // namespace extensions
