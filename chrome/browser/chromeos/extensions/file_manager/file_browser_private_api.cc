// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/file_browser_private_api.h"

#include "chrome/browser/chromeos/extensions/file_manager/event_router.h"
#include "chrome/browser/chromeos/extensions/file_manager/file_browser_private_api_factory.h"
#include "chrome/browser/chromeos/extensions/file_manager/private_api_dialog.h"
#include "chrome/browser/chromeos/extensions/file_manager/private_api_drive.h"
#include "chrome/browser/chromeos/extensions/file_manager/private_api_file_system.h"
#include "chrome/browser/chromeos/extensions/file_manager/private_api_misc.h"
#include "chrome/browser/chromeos/extensions/file_manager/private_api_mount.h"
#include "chrome/browser/chromeos/extensions/file_manager/private_api_strings.h"
#include "chrome/browser/chromeos/extensions/file_manager/private_api_tasks.h"
#include "chrome/browser/chromeos/extensions/file_manager/private_api_util.h"
#include "chrome/browser/extensions/extension_function_registry.h"

namespace file_manager {

FileBrowserPrivateAPI::FileBrowserPrivateAPI(Profile* profile)
    : event_router_(new EventRouter(profile)) {
  ExtensionFunctionRegistry* registry =
      ExtensionFunctionRegistry::GetInstance();
  // Tasks related functions.
  registry->RegisterFunction<ExecuteTaskFunction>();
  registry->RegisterFunction<GetFileTasksFunction>();
  registry->RegisterFunction<SetDefaultTaskFunction>();

  // Drive related functions.
  registry->RegisterFunction<GetDriveEntryPropertiesFunction>();
  registry->RegisterFunction<PinDriveFileFunction>();
  registry->RegisterFunction<GetDriveFilesFunction>();
  registry->RegisterFunction<CancelFileTransfersFunction>();
  registry->RegisterFunction<SearchDriveFunction>();
  registry->RegisterFunction<SearchDriveMetadataFunction>();
  registry->RegisterFunction<ClearDriveCacheFunction>();
  registry->RegisterFunction<GetDriveConnectionStateFunction>();
  registry->RegisterFunction<RequestAccessTokenFunction>();
  registry->RegisterFunction<GetShareUrlFunction>();

  // Select file dialog related functions.
  registry->RegisterFunction<CancelFileDialogFunction>();
  registry->RegisterFunction<SelectFileFunction>();
  registry->RegisterFunction<SelectFilesFunction>();

  // Mount points related functions.
  registry->RegisterFunction<AddMountFunction>();
  registry->RegisterFunction<RemoveMountFunction>();
  registry->RegisterFunction<GetMountPointsFunction>();

  // Hundreds of strings for the file manager.
  registry->RegisterFunction<GetStringsFunction>();

  // File system related functions.
  registry->RegisterFunction<RequestFileSystemFunction>();
  registry->RegisterFunction<AddFileWatchFunction>();
  registry->RegisterFunction<RemoveFileWatchFunction>();
  registry->RegisterFunction<SetLastModifiedFunction>();
  registry->RegisterFunction<GetSizeStatsFunction>();
  registry->RegisterFunction<GetVolumeMetadataFunction>();
  registry->RegisterFunction<ValidatePathNameLengthFunction>();
  registry->RegisterFunction<FormatDeviceFunction>();

  // Miscellaneous functions.
  registry->RegisterFunction<LogoutUserFunction>();
  registry->RegisterFunction<GetPreferencesFunction>();
  registry->RegisterFunction<SetPreferencesFunction>();
  registry->RegisterFunction<ZipSelectionFunction>();
  registry->RegisterFunction<ZoomFunction>();
  event_router_->ObserveFileSystemEvents();
}

FileBrowserPrivateAPI::~FileBrowserPrivateAPI() {
}

void FileBrowserPrivateAPI::Shutdown() {
  event_router_->Shutdown();
}

// static
FileBrowserPrivateAPI* FileBrowserPrivateAPI::Get(Profile* profile) {
  return FileBrowserPrivateAPIFactory::GetForProfile(profile);
}

}  // namespace file_manager
