// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/sync_file_system/extension_sync_event_observer.h"

#include "chrome/browser/extensions/event_names.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/sync_file_system/sync_event_observer.h"
#include "chrome/common/extensions/api/sync_file_system.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/syncable/sync_operation_type.h"

using sync_file_system::SyncEventObserver;

namespace extensions {

namespace {

api::sync_file_system::SyncStateStatus SyncServiceStateEnumToExtensionEnum(
    SyncEventObserver::SyncServiceState state) {
  switch (state) {
    case SyncEventObserver::SYNC_SERVICE_RUNNING:
      return api::sync_file_system::SYNC_FILE_SYSTEM_SYNC_STATE_STATUS_RUNNING;
    case SyncEventObserver::SYNC_SERVICE_AUTHENTICATION_REQUIRED:
      return api::sync_file_system::
          SYNC_FILE_SYSTEM_SYNC_STATE_STATUS_AUTHENTICATION_REQUIRED;
    case SyncEventObserver::SYNC_SERVICE_TEMPORARY_UNAVAILABLE:
      return api::sync_file_system::
          SYNC_FILE_SYSTEM_SYNC_STATE_STATUS_TEMPORARY_UNAVAILABLE;
    case SyncEventObserver::SYNC_SERVICE_DISABLED:
      return api::sync_file_system::SYNC_FILE_SYSTEM_SYNC_STATE_STATUS_DISABLED;
  }
  NOTREACHED();
  return api::sync_file_system::SYNC_FILE_SYSTEM_SYNC_STATE_STATUS_NONE;
}

api::sync_file_system::SyncOperationType SyncOperationTypeToExtensionEnum(
    fileapi::SyncOperationType operation_type) {
  switch (operation_type) {
    case fileapi::SYNC_OPERATION_NONE:
      return api::sync_file_system::
          SYNC_FILE_SYSTEM_SYNC_OPERATION_TYPE_NONE;
    case fileapi::SYNC_OPERATION_ADD:
      return api::sync_file_system::
          SYNC_FILE_SYSTEM_SYNC_OPERATION_TYPE_ADDED;
    case fileapi::SYNC_OPERATION_UPDATE:
      return api::sync_file_system::
          SYNC_FILE_SYSTEM_SYNC_OPERATION_TYPE_UPDATED;
    case fileapi::SYNC_OPERATION_DELETE:
      return api::sync_file_system::
          SYNC_FILE_SYSTEM_SYNC_OPERATION_TYPE_DELETED;
  }
  NOTREACHED();
  return api::sync_file_system::SYNC_FILE_SYSTEM_SYNC_OPERATION_TYPE_NONE;
}

}  // namespace

ExtensionSyncEventObserver::ExtensionSyncEventObserver(
    Profile* profile,
    const std::string& service_name)
    : profile_(profile),
      service_name_(service_name) {}

ExtensionSyncEventObserver::~ExtensionSyncEventObserver() {}

const std::string& ExtensionSyncEventObserver::GetExtensionId(
    const GURL& app_origin) {
  const Extension* app = ExtensionSystem::Get(profile_)->extension_service()->
      GetInstalledApp(app_origin);
  DCHECK(app);
  return app->id();
}

void ExtensionSyncEventObserver::OnSyncStateUpdated(
    const GURL& app_origin,
    sync_file_system::SyncEventObserver::SyncServiceState state,
    const std::string& description) {
  // TODO(calvinlo): Check extension in set of extension_ids that are listening
  // for events. If extension_id is not in the set, then ignore the event.
  const std::string extension_id = GetExtensionId(app_origin);

  // Convert state and description into SyncState Object
  api::sync_file_system::SyncState sync_state;
  sync_state.service_name = service_name_;
  sync_state.state = SyncServiceStateEnumToExtensionEnum(state);
  sync_state.description = description;
  scoped_ptr<base::ListValue> params(
      api::sync_file_system::OnSyncStateChanged::Create(sync_state));

  // Dispatch the event to the extension
  ExtensionSystem::Get(profile_)->event_router()->DispatchEventToExtension(
      extension_id, event_names::kOnSyncStateChanged, params.Pass(),
      profile_, GURL());
}

void ExtensionSyncEventObserver::OnFileSynced(
    const fileapi::FileSystemURL& url,
    fileapi::SyncOperationType operation) {
  const std::string extension_id = GetExtensionId(url.origin());

  // TODO(calvinlo):Convert filePath from string to Webkit FileEntry.
  const api::sync_file_system::SyncOperationType sync_operation_type =
      SyncOperationTypeToExtensionEnum(operation);
  const std::string filePath = url.path().AsUTF8Unsafe();
  scoped_ptr<base::ListValue> params(
      api::sync_file_system::OnFileSynced::Create(filePath,
                                                  sync_operation_type));

  // Dispatch the event to the extension
  ExtensionSystem::Get(profile_)->event_router()->DispatchEventToExtension(
      extension_id, event_names::kOnFileSynced, params.Pass(),
      profile_, GURL());
}

}  // namespace extensions
