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
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/web_contents.h"

namespace {

// If false, we don't prompt the user as to where to save the file.  This
// exists only for testing.
bool g_should_prompt_for_filename = true;

// This method is passed as a callback to SavePackage, which calls it when the
// DownloadItem is created. SavePackage is in content, so it cannot access gdata
// concepts. SetGDataPath must be called after the DownloadItem is created.
void OnSavePackageDownloadCreated(
    const FilePath& gdata_path,
    content::DownloadItem* download) {
  gdata::GDataDownloadObserver::SetGDataPath(download, gdata_path);
  download->SetDisplayName(gdata_path.BaseName());
  download->SetIsTemporary(true);
}

// Trampoline callback between GetGDataTempDownloadPath() and |callback|.
void ContinueSettingUpGDataDownload(
    const content::SavePackagePathPickedCallback& callback,
    FilePath* gdata_tmp_download_path,
    const FilePath& gdata_path) {
  callback.Run(*gdata_tmp_download_path, content::SAVE_PAGE_TYPE_AS_MHTML,
               base::Bind(&OnSavePackageDownloadCreated, gdata_path));
}

}  // anonymous namespace

SavePackageFilePickerChromeOS::SavePackageFilePickerChromeOS(
    content::WebContents* web_contents,
    const FilePath& suggested_path,
    const content::SavePackagePathPickedCallback& callback)
    : content::WebContentsObserver(web_contents),
      callback_(callback) {
  if (g_should_prompt_for_filename) {
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
  } else {
    FileSelected(suggested_path.ReplaceExtension("mhtml"), 0, NULL);
  }
}

void SavePackageFilePickerChromeOS::SetShouldPromptUser(bool should_prompt) {
  g_should_prompt_for_filename = should_prompt;
}

SavePackageFilePickerChromeOS::~SavePackageFilePickerChromeOS() {
}

void SavePackageFilePickerChromeOS::FileSelected(
    const FilePath& selected_path_const,
    int unused_index,
    void* unused_params) {
  if (!web_contents()) {
    delete this;
    return;
  }
  FilePath selected_path = selected_path_const;
  file_util::NormalizeFileNameEncoding(&selected_path);
  Profile* profile = Profile::FromBrowserContext(
      web_contents()->GetBrowserContext());
  DCHECK(profile);
  gdata::GDataSystemService* system_service =
      gdata::GDataSystemServiceFactory::GetForProfile(profile);
  // system_service is NULL in incognito.
  if (system_service && gdata::util::IsUnderGDataMountPoint(selected_path)) {
    // Here's a map to the callback chain:
    // GetGDataTempDownloadPath ->
    //   ContinueSettingUpGDataDownload ->
    //     callback_ = SavePackage::OnPathPicked ->
    //       download_created_callback = OnSavePackageDownloadCreated
    FilePath gdata_tmp_download_dir =
        system_service->file_system()->GetCacheDirectoryPath(
            gdata::GDataRootDirectory::CACHE_TYPE_TMP_DOWNLOADS);
    FilePath* gdata_tmp_download_path(new FilePath());
    content::BrowserThread::GetBlockingPool()->PostTaskAndReply(FROM_HERE,
        base::Bind(&gdata::GDataDownloadObserver::GetGDataTempDownloadPath,
                    gdata_tmp_download_dir,
                    gdata_tmp_download_path),
        base::Bind(&ContinueSettingUpGDataDownload,
                    callback_,
                    base::Owned(gdata_tmp_download_path),
                    selected_path));
  } else {
    callback_.Run(selected_path, content::SAVE_PAGE_TYPE_AS_MHTML,
                  content::SavePackageDownloadCreatedCallback());
  }
  delete this;
}

void SavePackageFilePickerChromeOS::FileSelectionCanceled(void* params) {
  delete this;
}
