// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/save_package_file_picker_chromeos.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/i18n/file_util_icu.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chromeos/gdata/drive_download_observer.h"
#include "chrome/browser/chromeos/gdata/drive_file_system_util.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/dialogs/selected_file_info.h"

namespace {

// If false, we don't prompt the user as to where to save the file.  This
// exists only for testing.
bool g_should_prompt_for_filename = true;

// Trampoline callback between GetDriveTempDownloadPath() and |callback|.
void ContinueSettingUpDriveDownload(
    const content::SavePackagePathPickedCallback& callback,
    const FilePath& drive_path,
    const FilePath& drive_tmp_download_path) {
  callback.Run(drive_tmp_download_path, content::SAVE_PAGE_TYPE_AS_MHTML,
               base::Bind(&gdata::DriveDownloadObserver::SetDownloadParams,
                          drive_path));
}

}  // anonymous namespace

SavePackageFilePickerChromeOS::SavePackageFilePickerChromeOS(
    content::WebContents* web_contents,
    const FilePath& suggested_path,
    const content::SavePackagePathPickedCallback& callback)
    : content::WebContentsObserver(web_contents),
      callback_(callback) {
  if (g_should_prompt_for_filename) {
    select_file_dialog_ = ui::SelectFileDialog::Create(
        this, new ChromeSelectFilePolicy(web_contents));
    ui::SelectFileDialog::FileTypeInfo file_types;
    file_types.support_gdata = true;
    select_file_dialog_->SelectFile(ui::SelectFileDialog::SELECT_SAVEAS_FILE,
                                    string16(),
                                    suggested_path.ReplaceExtension("mhtml"),
                                    &file_types,
                                    0,
                                    "mhtml",
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
    const FilePath& selected_path,
    int unused_index,
    void* unused_params) {
  FileSelectedWithExtraInfo(
      ui::SelectedFileInfo(selected_path, selected_path),
      unused_index,
      unused_params);
}

void SavePackageFilePickerChromeOS::FileSelectedWithExtraInfo(
    const ui::SelectedFileInfo& selected_file_info,
    int unused_index,
    void* unused_params) {
  if (!web_contents()) {
    delete this;
    return;
  }
  FilePath selected_path = selected_file_info.file_path;
  file_util::NormalizeFileNameEncoding(&selected_path);
  Profile* profile = Profile::FromBrowserContext(
      web_contents()->GetBrowserContext());
  DCHECK(profile);

  if (gdata::util::IsUnderDriveMountPoint(selected_path)) {
    // Here's a map to the callback chain:
    // GetDriveTempDownloadPath ->
    //   ContinueSettingUpDriveDownload ->
    //     callback_ = SavePackage::OnPathPicked ->
    //       download_created_callback = OnSavePackageDownloadCreated
    gdata::DriveDownloadObserver::SubstituteDriveDownloadPath(
        profile, selected_path, NULL,
        base::Bind(&ContinueSettingUpDriveDownload, callback_, selected_path));
  } else {
    callback_.Run(selected_path, content::SAVE_PAGE_TYPE_AS_MHTML,
                  content::SavePackageDownloadCreatedCallback());
  }
  delete this;
}

void SavePackageFilePickerChromeOS::FileSelectionCanceled(void* params) {
  delete this;
}
