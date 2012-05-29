// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_file_picker_chromeos.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/i18n/file_util_icu.h"
#include "base/memory/ref_counted.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chromeos/gdata/gdata_download_observer.h"
#include "chrome/browser/chromeos/gdata/gdata_file_system.h"
#include "chrome/browser/chromeos/gdata/gdata_system_service.h"
#include "chrome/browser/chromeos/gdata/gdata_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"

using content::BrowserThread;
using content::DownloadManager;

namespace {

// Call FileSelected on |download_manager|.
void GDataTempFileSelected(DownloadManager* download_manager,
                           FilePath* file_path,
                           int32 download_id) {
  download_manager->FileSelected(*file_path, download_id);
}

}  // namespace

DownloadFilePickerChromeOS::DownloadFilePickerChromeOS(
    content::DownloadManager* download_manager,
    content::WebContents* web_contents,
    const FilePath& suggested_path,
    int32 download_id)
    : DownloadFilePicker(download_manager,
                         web_contents,
                         suggested_path,
                         download_id) {
}

DownloadFilePickerChromeOS::~DownloadFilePickerChromeOS() {
}

void DownloadFilePickerChromeOS::FileSelected(const FilePath& selected_path,
                                              int index,
                                              void* params) {
  FilePath path = selected_path;
  file_util::NormalizeFileNameEncoding(&path);

  RecordFileSelected(path);

  if (download_manager_) {
    gdata::GDataSystemService* system_service =
        gdata::GDataSystemServiceFactory::GetForProfile(
            ProfileManager::GetDefaultProfile());
    if (system_service && gdata::util::IsUnderGDataMountPoint(path)) {
      // If we're trying to download a file into gdata, save path in external
      // data.
      content::DownloadItem* download =
          download_manager_->GetActiveDownloadItem(download_id_);
      if (download) {
        gdata::GDataDownloadObserver::SetGDataPath(download, path);
        download->SetDisplayName(path.BaseName());
        download->SetIsTemporary(true);

        const FilePath gdata_tmp_download_dir =
            system_service->file_system()->GetCacheDirectoryPath(
                gdata::GDataCache::CACHE_TYPE_TMP_DOWNLOADS);

        // Swap the gdata path with a local path. Local path must be created
        // on the IO thread pool.
        FilePath* gdata_tmp_download_path(new FilePath());
        BrowserThread::GetBlockingPool()->PostTaskAndReply(FROM_HERE,
            base::Bind(&gdata::GDataDownloadObserver::GetGDataTempDownloadPath,
                       gdata_tmp_download_dir,
                       gdata_tmp_download_path),
            base::Bind(&GDataTempFileSelected,
                       download_manager_,
                       base::Owned(gdata_tmp_download_path),
                       download_id_));
      }
    } else {
      download_manager_->FileSelected(path, download_id_);
    }
  }
  delete this;
}
