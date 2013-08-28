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
  registry->RegisterFunction<extensions::ExecuteTaskFunction>();
  registry->RegisterFunction<extensions::GetFileTasksFunction>();
  registry->RegisterFunction<extensions::SetDefaultTaskFunction>();

  // Drive related functions.
  registry->RegisterFunction<extensions::GetDriveEntryPropertiesFunction>();
  registry->RegisterFunction<extensions::PinDriveFileFunction>();
  registry->RegisterFunction<extensions::GetDriveFilesFunction>();
  registry->RegisterFunction<extensions::CancelFileTransfersFunction>();
  registry->RegisterFunction<extensions::SearchDriveFunction>();
  registry->RegisterFunction<extensions::SearchDriveMetadataFunction>();
  registry->RegisterFunction<extensions::ClearDriveCacheFunction>();
  registry->RegisterFunction<extensions::GetDriveConnectionStateFunction>();
  registry->RegisterFunction<extensions::RequestAccessTokenFunction>();
  registry->RegisterFunction<extensions::GetShareUrlFunction>();

  // Select file dialog related functions.
  registry->RegisterFunction<extensions::CancelFileDialogFunction>();
  registry->RegisterFunction<extensions::SelectFileFunction>();
  registry->RegisterFunction<extensions::SelectFilesFunction>();

  // Mount points related functions.
  registry->RegisterFunction<extensions::AddMountFunction>();
  registry->RegisterFunction<extensions::RemoveMountFunction>();
  registry->RegisterFunction<extensions::GetMountPointsFunction>();

  // Hundreds of strings for the file manager.
  registry->RegisterFunction<extensions::GetStringsFunction>();

  // File system related functions.
  registry->RegisterFunction<extensions::RequestFileSystemFunction>();
  registry->RegisterFunction<extensions::AddFileWatchFunction>();
  registry->RegisterFunction<extensions::RemoveFileWatchFunction>();
  registry->RegisterFunction<extensions::SetLastModifiedFunction>();
  registry->RegisterFunction<extensions::GetSizeStatsFunction>();
  registry->RegisterFunction<extensions::GetVolumeMetadataFunction>();
  registry->RegisterFunction<extensions::ValidatePathNameLengthFunction>();
  registry->RegisterFunction<extensions::FormatDeviceFunction>();

  // Miscellaneous functions.
  registry->RegisterFunction<extensions::LogoutUserFunction>();
  registry->RegisterFunction<extensions::GetPreferencesFunction>();
  registry->RegisterFunction<extensions::SetPreferencesFunction>();
  registry->RegisterFunction<extensions::ZipSelectionFunction>();
  registry->RegisterFunction<extensions::ZoomFunction>();
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
