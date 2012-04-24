// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/save_package_file_picker_chromeos.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/i18n/file_util_icu.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chromeos/gdata/gdata_download_observer.h"
#include "chrome/browser/chromeos/gdata/gdata_file_system.h"
#include "chrome/browser/chromeos/gdata/gdata_system_service.h"
#include "chrome/browser/chromeos/gdata/gdata_util.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

using content::BrowserThread;

SavePackageFilePickerChromeOS::SavePackageFilePickerChromeOS(
    content::WebContents* web_contents,
    const FilePath& suggested_path)
    : content::WebContentsObserver(web_contents) {
  select_file_dialog_ = SelectFileDialog::Create(this);
  select_file_dialog_->SelectFile(SelectFileDialog::SELECT_SAVEAS_FILE,
                                  string16(),
                                  suggested_path.ReplaceExtension("mhtml"),
                                  NULL,
                                  0,
                                  "mhtml",
                                  web_contents,
                                  platform_util::GetTopLevel(
                                      web_contents->GetNativeView()),
                                  NULL);
}

SavePackageFilePickerChromeOS::~SavePackageFilePickerChromeOS() {
}

void SavePackageFilePickerChromeOS::FileSelected(const FilePath& selected_path,
                                                 int index,
                                                 void* params) {
  if (!web_contents()) {
    delete this;
    return;
  }

  FilePath path = selected_path;
  file_util::NormalizeFileNameEncoding(&path);

  gdata::GDataFileSystem* gdata_filesystem = GetGDataFileSystem();
  if (gdata_filesystem && gdata::util::IsUnderGDataMountPoint(path)) {
    FilePath gdata_tmp_download_dir =
        gdata_filesystem->GetCacheDirectoryPath(
            gdata::GDataRootDirectory::CACHE_TYPE_TMP_DOWNLOADS);

    selected_path_ = path;
    FilePath* gdata_tmp_download_path = new FilePath();
    BrowserThread::GetBlockingPool()->PostTaskAndReply(FROM_HERE,
        base::Bind(&gdata::GDataDownloadObserver::GetGDataTempDownloadPath,
                   gdata_tmp_download_dir,
                   gdata_tmp_download_path),
        base::Bind(&SavePackageFilePickerChromeOS::GenerateMHTML,
                   base::Unretained(this),
                   base::Owned(gdata_tmp_download_path)));
  } else {
    DVLOG(1) << "SavePackageFilePickerChromeOS non-gdata file";
    GenerateMHTML(&path);
  }
}

void SavePackageFilePickerChromeOS::FileSelectionCanceled(void* params) {
  delete this;
}

void SavePackageFilePickerChromeOS::GenerateMHTML(const FilePath* mhtml_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!web_contents()) {
    delete this;
    return;
  }

  DVLOG(1) << "GenerateMHTML " << mhtml_path->value();
  web_contents()->GenerateMHTML(*mhtml_path,
      base::Bind(&SavePackageFilePickerChromeOS::OnMHTMLGenerated,
                 base::Unretained(this)));
}

void SavePackageFilePickerChromeOS::OnMHTMLGenerated(const FilePath& src_path,
                                                     int64 file_size) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!web_contents()) {
    delete this;
    return;
  }

  gdata::GDataFileSystem* gdata_filesystem = GetGDataFileSystem();
  if (gdata_filesystem && !selected_path_.empty()) {
    DVLOG(1) << "TransferFile from " << src_path.value()
             << " to " << selected_path_.value();
    gdata_filesystem->TransferFile(src_path,
        gdata::util::ExtractGDataPath(selected_path_),
        base::Bind(&SavePackageFilePickerChromeOS::OnTransferFile,
                   base::Unretained(this)));
  } else {
    delete this;
  }
}

void SavePackageFilePickerChromeOS::OnTransferFile(
    base::PlatformFileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(error, base::PLATFORM_FILE_OK);
  delete this;
}

gdata::GDataFileSystem*
SavePackageFilePickerChromeOS::GetGDataFileSystem() {
  DCHECK(web_contents());
  Profile* profile = Profile::FromBrowserContext(
      web_contents()->GetBrowserContext());
  DCHECK(profile);
  gdata::GDataSystemService* system_service =
      gdata::GDataSystemServiceFactory::GetForProfile(profile);
  // system_service is NULL in incognito.
  return system_service ? system_service->file_system() : NULL;
}
