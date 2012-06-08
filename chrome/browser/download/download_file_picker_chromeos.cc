// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_file_picker_chromeos.h"

#include "base/bind.h"
#include "base/i18n/file_util_icu.h"
#include "chrome/browser/chromeos/gdata/gdata_download_observer.h"
#include "chrome/browser/chromeos/gdata/gdata_util.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"

using content::DownloadItem;
using content::DownloadManager;

namespace {

// Call FileSelected on |download_manager|.
void FileSelectedHelper(DownloadManager* download_manager,
                        int32 download_id,
                        const FilePath& file_path) {
  download_manager->FileSelected(file_path, download_id);
}

}  // namespace

DownloadFilePickerChromeOS::DownloadFilePickerChromeOS() {
}

DownloadFilePickerChromeOS::~DownloadFilePickerChromeOS() {
}

void DownloadFilePickerChromeOS::InitSuggestedPath(DownloadItem* item) {
  // For GData downloads, suggested path is the virtual gdata path instead of
  // the temporary local one.
  if (gdata::GDataDownloadObserver::IsGDataDownload(item)) {
    set_suggested_path(gdata::util::GetSpecialRemoteRootPath().Append(
        gdata::GDataDownloadObserver::GetGDataPath(item)));
  } else {
    DownloadFilePicker::InitSuggestedPath(item);
  }
}

void DownloadFilePickerChromeOS::FileSelected(const FilePath& selected_path,
                                              int index,
                                              void* params) {
  FilePath path = selected_path;
  file_util::NormalizeFileNameEncoding(&path);

  RecordFileSelected(path);

  if (download_manager_) {
    content::DownloadItem* download =
        download_manager_->GetActiveDownloadItem(download_id_);
    gdata::GDataDownloadObserver::SubstituteGDataDownloadPath(
        NULL, path, download,
        base::Bind(&FileSelectedHelper, download_manager_, download_id_));
  }
  delete this;
}
