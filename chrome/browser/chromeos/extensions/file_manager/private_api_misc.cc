// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/private_api_misc.h"

#include "base/files/file_path.h"
#include "base/prefs/pref_service.h"
#include "base/values.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/drive/logging.h"
#include "chrome/browser/chromeos/extensions/file_manager/private_api_util.h"
#include "chrome/browser/chromeos/file_manager/file_manager_installer.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/page_zoom.h"
#include "url/gurl.h"

namespace extensions {

FileBrowserPrivateLogoutUserFunction::FileBrowserPrivateLogoutUserFunction() {
}

FileBrowserPrivateLogoutUserFunction::~FileBrowserPrivateLogoutUserFunction() {
}

bool FileBrowserPrivateLogoutUserFunction::RunImpl() {
  chrome::AttemptUserExit();
  return true;
}

FileBrowserPrivateGetPreferencesFunction::
    FileBrowserPrivateGetPreferencesFunction() {
}

FileBrowserPrivateGetPreferencesFunction::
    ~FileBrowserPrivateGetPreferencesFunction() {
}

bool FileBrowserPrivateGetPreferencesFunction::RunImpl() {
  scoped_ptr<DictionaryValue> value(new DictionaryValue());

  const PrefService* service = profile_->GetPrefs();

  value->SetBoolean("driveEnabled",
                    drive::util::IsDriveEnabledForProfile(profile_));

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

FileBrowserPrivateSetPreferencesFunction::
    FileBrowserPrivateSetPreferencesFunction() {
}

FileBrowserPrivateSetPreferencesFunction::
    ~FileBrowserPrivateSetPreferencesFunction() {
}

bool FileBrowserPrivateSetPreferencesFunction::RunImpl() {
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

FileBrowserPrivateZipSelectionFunction::
    FileBrowserPrivateZipSelectionFunction() {
}

FileBrowserPrivateZipSelectionFunction::
    ~FileBrowserPrivateZipSelectionFunction() {
}

bool FileBrowserPrivateZipSelectionFunction::RunImpl() {
  if (args_->GetSize() < 3) {
    return false;
  }

  // First param is the source directory URL.
  std::string dir_url;
  if (!args_->GetString(0, &dir_url) || dir_url.empty())
    return false;

  base::FilePath src_dir = file_manager::util::GetLocalPathFromURL(
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
    base::FilePath path = file_manager::util::GetLocalPathFromURL(
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

  zip_file_creator_ = new file_manager::ZipFileCreator(this,
                                                       src_dir,
                                                       src_relative_paths,
                                                       dest_file);

  // Keep the refcount until the zipping is complete on utility process.
  AddRef();

  zip_file_creator_->Start();
  return true;
}

void FileBrowserPrivateZipSelectionFunction::OnZipDone(bool success) {
  SetResult(new base::FundamentalValue(success));
  SendResponse(true);
  Release();
}

FileBrowserPrivateZoomFunction::FileBrowserPrivateZoomFunction() {
}

FileBrowserPrivateZoomFunction::~FileBrowserPrivateZoomFunction() {
}

bool FileBrowserPrivateZoomFunction::RunImpl() {
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

FileBrowserPrivateInstallWebstoreItemFunction::
    FileBrowserPrivateInstallWebstoreItemFunction() {
}

FileBrowserPrivateInstallWebstoreItemFunction::
    ~FileBrowserPrivateInstallWebstoreItemFunction() {
}

bool FileBrowserPrivateInstallWebstoreItemFunction::RunImpl() {
  if (args_->GetSize() < 1)
    return false;

  if (!args_->GetString(0, &webstore_item_id_) || webstore_item_id_.empty())
    return false;

  extensions::WebstoreStandaloneInstaller::Callback callback =
      base::Bind(
          &FileBrowserPrivateInstallWebstoreItemFunction::OnInstallComplete,
          this);

  scoped_refptr<file_manager::FileManagerInstaller> installer(
      new file_manager::FileManagerInstaller(
          GetAssociatedWebContents(),  // web_contents(),
          webstore_item_id_,
          profile(),
          callback));
  // installer will be AddRef()'d in BeginInstall().
  installer->BeginInstall();
  return true;
}

void FileBrowserPrivateInstallWebstoreItemFunction::OnInstallComplete(
    bool success,
    const std::string& error) {
  if (success) {
    drive::util::Log(logging::LOG_INFO,
                     "App install succeeded. (item id: %s)",
                     webstore_item_id_.c_str());
  } else {
    drive::util::Log(logging::LOG_ERROR,
                     "App install failed. (item id: %s, reason: %s)",
                     webstore_item_id_.c_str(),
                     error.c_str());
    error_ = error;
  }

  SendResponse(success);
}

}  // namespace extensions
