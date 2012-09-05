// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_file_picker_chromeos.h"

#include "base/bind.h"
#include "base/i18n/file_util_icu.h"
#include "chrome/browser/chromeos/gdata/drive_download_observer.h"
#include "chrome/browser/chromeos/gdata/drive_file_system_util.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"
#include "ui/base/dialogs/selected_file_info.h"

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
  if (gdata::DriveDownloadObserver::IsDriveDownload(item)) {
    set_suggested_path(gdata::util::GetSpecialRemoteRootPath().Append(
        gdata::DriveDownloadObserver::GetDrivePath(item)));
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
    content::DownloadItem* download =
        download_manager_->GetActiveDownloadItem(download_id_);
    gdata::DriveDownloadObserver::SubstituteDriveDownloadPath(
        NULL, path, download,
        base::Bind(&DownloadFilePickerChromeOS::OnFileSelected,
                   base::Unretained(this)));
  } else {
    OnFileSelected(FilePath());
  }
  // The OnFileSelected() call deletes |this|
}
