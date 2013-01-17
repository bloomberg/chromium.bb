// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_file_picker_chromeos.h"

#include "base/bind.h"
#include "base/i18n/file_util_icu.h"
#include "chrome/browser/chromeos/drive/drive_download_observer.h"
#include "chrome/browser/chromeos/drive/drive_file_system_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"
#include "ui/shell_dialogs/selected_file_info.h"

using content::DownloadItem;
using content::DownloadManager;

DownloadFilePickerChromeOS::DownloadFilePickerChromeOS() {
}

DownloadFilePickerChromeOS::~DownloadFilePickerChromeOS() {
}

void DownloadFilePickerChromeOS::InitSuggestedPath(DownloadItem* item,
                                                   const FilePath& path) {
  // For Drive downloads, |path| is the virtual gdata path instead of the
  // temporary local one.
  if (drive::DriveDownloadObserver::IsDriveDownload(item)) {
    set_suggested_path(drive::util::GetSpecialRemoteRootPath().Append(
        drive::DriveDownloadObserver::GetDrivePath(item)));
  } else {
    DownloadFilePicker::InitSuggestedPath(item, path);
  }
}

void DownloadFilePickerChromeOS::FileSelected(const FilePath& selected_path,
                                              int index,
                                              void* params) {
  FileSelectedWithExtraInfo(
      ui::SelectedFileInfo(selected_path, selected_path),
      index,
      params);
}

void DownloadFilePickerChromeOS::FileSelectedWithExtraInfo(
    const ui::SelectedFileInfo& file_info,
    int index,
    void* params) {
  FilePath path = file_info.file_path;
  file_util::NormalizeFileNameEncoding(&path);

  // Need to do this before we substitute with a temporary path. Otherwise we
  // won't be able to detect path changes.
  RecordFileSelected(path);

  if (download_manager_) {
    Profile* profile =
        Profile::FromBrowserContext(download_manager_->GetBrowserContext());
    DownloadItem* download = download_manager_->GetDownload(download_id_);
    drive::DriveDownloadObserver::SubstituteDriveDownloadPath(
        profile, path, download,
        base::Bind(&DownloadFilePickerChromeOS::OnFileSelected,
                   base::Unretained(this)));
  } else {
    OnFileSelected(FilePath());
  }
  // The OnFileSelected() call deletes |this|
}
