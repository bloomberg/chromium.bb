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
  registry->RegisterFunction<
    extensions::FileBrowserPrivateExecuteTaskFunction>();
  registry->RegisterFunction<
    extensions::FileBrowserPrivateGetFileTasksFunction>();
  registry->RegisterFunction<
    extensions::FileBrowserPrivateSetDefaultTaskFunction>();

  // Drive related functions.
  registry->RegisterFunction<
    extensions::FileBrowserPrivateGetDriveEntryPropertiesFunction>();
  registry->RegisterFunction<
    extensions::FileBrowserPrivatePinDriveFileFunction>();
  registry->RegisterFunction<
    extensions::FileBrowserPrivateGetDriveFilesFunction>();
  registry->RegisterFunction<
    extensions::FileBrowserPrivateCancelFileTransfersFunction>();
  registry->RegisterFunction<
    extensions::FileBrowserPrivateSearchDriveFunction>();
  registry->RegisterFunction<
    extensions::FileBrowserPrivateSearchDriveMetadataFunction>();
  registry->RegisterFunction<
    extensions::FileBrowserPrivateClearDriveCacheFunction>();
  registry->RegisterFunction<
    extensions::FileBrowserPrivateGetDriveConnectionStateFunction>();
  registry->RegisterFunction<
    extensions::FileBrowserPrivateRequestAccessTokenFunction>();
  registry->RegisterFunction<
    extensions::FileBrowserPrivateGetShareUrlFunction>();

  // Select file dialog related functions.
  registry->RegisterFunction<
    extensions::FileBrowserPrivateCancelDialogFunction>();
  registry->RegisterFunction<
    extensions::FileBrowserPrivateSelectFileFunction>();
  registry->RegisterFunction<
    extensions::FileBrowserPrivateSelectFilesFunction>();

  // Mount points related functions.
  registry->RegisterFunction<extensions::FileBrowserPrivateAddMountFunction>();
  registry->RegisterFunction<
    extensions::FileBrowserPrivateRemoveMountFunction>();
  registry->RegisterFunction<
    extensions::FileBrowserPrivateGetMountPointsFunction>();

  // Hundreds of strings for the file manager.
  registry->RegisterFunction<
    extensions::FileBrowserPrivateGetStringsFunction>();

  // File system related functions.
  registry->RegisterFunction<
    extensions::FileBrowserPrivateRequestFileSystemFunction>();
  registry->RegisterFunction<
    extensions::FileBrowserPrivateAddFileWatchFunction>();
  registry->RegisterFunction<
    extensions::FileBrowserPrivateRemoveFileWatchFunction>();
  registry->RegisterFunction<
    extensions::FileBrowserPrivateSetLastModifiedFunction>();
  registry->RegisterFunction<
    extensions::FileBrowserPrivateGetSizeStatsFunction>();
  registry->RegisterFunction<
    extensions::FileBrowserPrivateGetVolumeMetadataFunction>();
  registry->RegisterFunction<
    extensions::FileBrowserPrivateValidatePathNameLengthFunction>();
  registry->RegisterFunction<
    extensions::FileBrowserPrivateFormatDeviceFunction>();

  // Miscellaneous functions.
  registry->RegisterFunction<
    extensions::FileBrowserPrivateLogoutUserFunction>();
  registry->RegisterFunction<
    extensions::FileBrowserPrivateGetPreferencesFunction>();
  registry->RegisterFunction<
    extensions::FileBrowserPrivateSetPreferencesFunction>();
  registry->RegisterFunction<
    extensions::FileBrowserPrivateInstallWebstoreItemFunction>();
  registry->RegisterFunction<
    extensions::FileBrowserPrivateZipSelectionFunction>();
  registry->RegisterFunction<extensions::FileBrowserPrivateZoomFunction>();

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
