// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/sync_file_system/extension_sync_event_observer.h"

#include "chrome/browser/extensions/event_names.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/sync_file_system/sync_event_observer.h"
#include "chrome/browser/sync_file_system/sync_file_system_service.h"
#include "chrome/common/extensions/api/sync_file_system.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/syncable/sync_operation_result.h"

using sync_file_system::SyncEventObserver;

namespace extensions {

namespace {

api::sync_file_system::SyncStateStatus SyncServiceStateEnumToExtensionEnum(
    SyncEventObserver::SyncServiceState state) {
  switch (state) {
    case SyncEventObserver::SYNC_SERVICE_RUNNING:
      return api::sync_file_system::SYNC_STATE_STATUS_RUNNING;
    case SyncEventObserver::SYNC_SERVICE_AUTHENTICATION_REQUIRED:
      return api::sync_file_system::
          SYNC_STATE_STATUS_AUTHENTICATION_REQUIRED;
    case SyncEventObserver::SYNC_SERVICE_TEMPORARY_UNAVAILABLE:
      return api::sync_file_system::
          SYNC_STATE_STATUS_TEMPORARY_UNAVAILABLE;
    case SyncEventObserver::SYNC_SERVICE_DISABLED:
      return api::sync_file_system::SYNC_STATE_STATUS_DISABLED;
  }
  NOTREACHED();
  return api::sync_file_system::SYNC_STATE_STATUS_NONE;
}

api::sync_file_system::SyncOperationResult SyncOperationResultToExtensionEnum(
    fileapi::SyncOperationResult operation_result) {
  switch (operation_result) {
    case fileapi::SYNC_OPERATION_NONE:
      return api::sync_file_system::
          SYNC_OPERATION_RESULT_NONE;
    case fileapi::SYNC_OPERATION_ADDED:
      return api::sync_file_system::
          SYNC_OPERATION_RESULT_ADDED;
    case fileapi::SYNC_OPERATION_UPDATED:
      return api::sync_file_system::
          SYNC_OPERATION_RESULT_UPDATED;
    case fileapi::SYNC_OPERATION_DELETED:
      return api::sync_file_system::
          SYNC_OPERATION_RESULT_DELETED;
    case fileapi::SYNC_OPERATION_CONFLICTED:
      return api::sync_file_system::
          SYNC_OPERATION_RESULT_CONFLICTED;
  }
  NOTREACHED();
  return api::sync_file_system::SYNC_OPERATION_RESULT_NONE;
}

}  // namespace

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
  // Convert state and description into SyncState Object.
  api::sync_file_system::SyncState sync_state;
  sync_state.service_name = service_name_;
  sync_state.state = SyncServiceStateEnumToExtensionEnum(state);
  sync_state.description = description;
  scoped_ptr<base::ListValue> params(
      api::sync_file_system::OnSyncStateChanged::Create(sync_state));

  BroadcastOrDispatchEvent(app_origin,
                           event_names::kOnSyncStateChanged,
                           params.Pass());
}

void ExtensionSyncEventObserver::OnFileSynced(
    const fileapi::FileSystemURL& url,
    fileapi::SyncOperationResult result) {
  // TODO(calvinlo):Convert filePath from string to Webkit FileEntry.
  const api::sync_file_system::SyncOperationResult sync_operation_result =
      SyncOperationResultToExtensionEnum(result);
  const std::string filePath = url.path().AsUTF8Unsafe();
  scoped_ptr<base::ListValue> params(
      api::sync_file_system::OnFileSynced::Create(filePath,
                                                  sync_operation_result));

  BroadcastOrDispatchEvent(url.origin(),
                           event_names::kOnFileSynced,
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
  event_router->DispatchEventToExtension(extension_id, event.Pass());
}

}  // namespace extensions
