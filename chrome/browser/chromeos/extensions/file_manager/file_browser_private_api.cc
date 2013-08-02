// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/file_browser_private_api.h"

#include <map>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/drive/logging.h"
#include "chrome/browser/chromeos/extensions/file_manager/file_browser_private_api_factory.h"
#include "chrome/browser/chromeos/extensions/file_manager/file_manager_event_router.h"
#include "chrome/browser/chromeos/extensions/file_manager/private_api_dialog.h"
#include "chrome/browser/chromeos/extensions/file_manager/private_api_drive.h"
#include "chrome/browser/chromeos/extensions/file_manager/private_api_file_system.h"
#include "chrome/browser/chromeos/extensions/file_manager/private_api_mount.h"
#include "chrome/browser/chromeos/extensions/file_manager/private_api_strings.h"
#include "chrome/browser/chromeos/extensions/file_manager/private_api_tasks.h"
#include "chrome/browser/chromeos/extensions/file_manager/private_api_util.h"
#include "chrome/browser/chromeos/extensions/file_manager/zip_file_creator.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/extensions/extension_function_registry.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/page_zoom.h"
#include "url/gurl.h"

using extensions::ZipFileCreator;

namespace file_manager {

FileBrowserPrivateAPI::FileBrowserPrivateAPI(Profile* profile)
    : event_router_(new FileManagerEventRouter(profile)) {
  ExtensionFunctionRegistry* registry =
      ExtensionFunctionRegistry::GetInstance();
  // Tasks related functions.
  registry->RegisterFunction<ExecuteTaskFunction>();
  registry->RegisterFunction<GetFileTasksFunction>();
  registry->RegisterFunction<SetDefaultTaskFunction>();
  registry->RegisterFunction<ViewFilesFunction>();

  // Drive related functions.
  registry->RegisterFunction<GetDriveEntryPropertiesFunction>();
  registry->RegisterFunction<PinDriveFileFunction>();
  registry->RegisterFunction<GetDriveFilesFunction>();
  registry->RegisterFunction<CancelFileTransfersFunction>();
  registry->RegisterFunction<TransferFileFunction>();
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

  // Mount points releated functions.
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

  // Misc functions.
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

LogoutUserFunction::LogoutUserFunction() {
}

LogoutUserFunction::~LogoutUserFunction() {
}

bool LogoutUserFunction::RunImpl() {
  chrome::AttemptUserExit();
  return true;
}

GetPreferencesFunction::GetPreferencesFunction() {
}

GetPreferencesFunction::~GetPreferencesFunction() {
}

bool GetPreferencesFunction::RunImpl() {
  scoped_ptr<DictionaryValue> value(new DictionaryValue());

  const PrefService* service = profile_->GetPrefs();

  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::GetForProfile(profile_);
  bool drive_enabled = (integration_service != NULL);

  value->SetBoolean("driveEnabled", drive_enabled);

  value->SetBoolean("cellularDisabled",
                    service->GetBoolean(prefs::kDisableDriveOverCellular));

  value->SetBoolean("hostedFilesDisabled",
                    service->GetBoolean(prefs::kDisableDriveHostedFiles));

  value->SetBoolean("use24hourClock",
                    service->GetBoolean(prefs::kUse24HourClock));

  {
    bool allow = true;
    if (!chromeos::CrosSettings::Get()->GetBoolean(
            chromeos::kAllowRedeemChromeOsRegistrationOffers, &allow)) {
      allow = true;
    }
    value->SetBoolean("allowRedeemOffers", allow);
  }

  SetResult(value.release());

  drive::util::Log(logging::LOG_INFO, "%s succeeded.", name().c_str());
  return true;
}

SetPreferencesFunction::SetPreferencesFunction() {
}

SetPreferencesFunction::~SetPreferencesFunction() {
}

bool SetPreferencesFunction::RunImpl() {
  base::DictionaryValue* value = NULL;

  if (!args_->GetDictionary(0, &value) || !value)
    return false;

  PrefService* service = profile_->GetPrefs();

  bool tmp;

  if (value->GetBoolean("cellularDisabled", &tmp))
    service->SetBoolean(prefs::kDisableDriveOverCellular, tmp);

  if (value->GetBoolean("hostedFilesDisabled", &tmp))
    service->SetBoolean(prefs::kDisableDriveHostedFiles, tmp);

  drive::util::Log(logging::LOG_INFO, "%s succeeded.", name().c_str());
  return true;
}

ZipSelectionFunction::ZipSelectionFunction() {
}

ZipSelectionFunction::~ZipSelectionFunction() {
}

bool ZipSelectionFunction::RunImpl() {
  if (args_->GetSize() < 3) {
    return false;
  }

  // First param is the source directory URL.
  std::string dir_url;
  if (!args_->GetString(0, &dir_url) || dir_url.empty())
    return false;

  base::FilePath src_dir = GetLocalPathFromURL(
      render_view_host(), profile(), GURL(dir_url));
  if (src_dir.empty())
    return false;

  // Second param is the list of selected file URLs.
  ListValue* selection_urls = NULL;
  args_->GetList(1, &selection_urls);
  if (!selection_urls || !selection_urls->GetSize())
    return false;

  std::vector<base::FilePath> files;
  for (size_t i = 0; i < selection_urls->GetSize(); ++i) {
    std::string file_url;
    selection_urls->GetString(i, &file_url);
    base::FilePath path = GetLocalPathFromURL(
        render_view_host(), profile(), GURL(file_url));
    if (path.empty())
      return false;
    files.push_back(path);
  }

  // Third param is the name of the output zip file.
  std::string dest_name;
  if (!args_->GetString(2, &dest_name) || dest_name.empty())
    return false;

  // Check if the dir path is under Drive mount point.
  // TODO(hshi): support create zip file on Drive (crbug.com/158690).
  if (drive::util::IsUnderDriveMountPoint(src_dir))
    return false;

  base::FilePath dest_file = src_dir.Append(dest_name);
  std::vector<base::FilePath> src_relative_paths;
  for (size_t i = 0; i != files.size(); ++i) {
    const base::FilePath& file_path = files[i];

    // Obtain the relative path of |file_path| under |src_dir|.
    base::FilePath relative_path;
    if (!src_dir.AppendRelativePath(file_path, &relative_path))
      return false;
    src_relative_paths.push_back(relative_path);
  }

  zip_file_creator_ = new ZipFileCreator(this, src_dir, src_relative_paths,
                                         dest_file);

  // Keep the refcount until the zipping is complete on utility process.
  AddRef();

  zip_file_creator_->Start();
  return true;
}

void ZipSelectionFunction::OnZipDone(bool success) {
  SetResult(new base::FundamentalValue(success));
  SendResponse(true);
  Release();
}

ZoomFunction::ZoomFunction() {
}

ZoomFunction::~ZoomFunction() {
}

bool ZoomFunction::RunImpl() {
  content::RenderViewHost* const view_host = render_view_host();
  std::string operation;
  args_->GetString(0, &operation);
  content::PageZoom zoom_type;
  if (operation == "in") {
    zoom_type = content::PAGE_ZOOM_IN;
  } else if (operation == "out") {
    zoom_type = content::PAGE_ZOOM_OUT;
  } else if (operation == "reset") {
    zoom_type = content::PAGE_ZOOM_RESET;
  } else {
    NOTREACHED();
    return false;
  }
  view_host->Zoom(zoom_type);
  return true;
}

}  // namespace file_manager
