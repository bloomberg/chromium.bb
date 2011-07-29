// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/chrome_download_manager_delegate.h"

#include "chrome/browser/download/download_file_picker.h"
#include "chrome/browser/download/save_package_file_picker.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/pref_names.h"
#include "content/browser/tab_contents/tab_contents.h"

ChromeDownloadManagerDelegate::ChromeDownloadManagerDelegate()
    : download_manager_(NULL) {
}

void ChromeDownloadManagerDelegate::GetSaveDir(TabContents* tab_contents,
                                               FilePath* website_save_dir,
                                               FilePath* download_save_dir) {
  Profile* profile =
      Profile::FromBrowserContext(tab_contents->browser_context());
  PrefService* prefs = profile->GetPrefs();

  // Check whether the preference has the preferred directory for saving file.
  // If not, initialize it with default directory.
  if (!prefs->FindPreference(prefs::kSaveFileDefaultDirectory)) {
    DCHECK(prefs->FindPreference(prefs::kDownloadDefaultDirectory));
    FilePath default_save_path = prefs->GetFilePath(
        prefs::kDownloadDefaultDirectory);
    prefs->RegisterFilePathPref(prefs::kSaveFileDefaultDirectory,
                                default_save_path,
                                PrefService::UNSYNCABLE_PREF);
  }

  // Get the directory from preference.
  *website_save_dir = prefs->GetFilePath(prefs::kSaveFileDefaultDirectory);
  DCHECK(!website_save_dir->empty());

  *download_save_dir = prefs->GetFilePath(prefs::kDownloadDefaultDirectory);
}

void ChromeDownloadManagerDelegate::ChooseSavePath(
    const base::WeakPtr<SavePackage>& save_package,
    const FilePath& suggested_path,
    bool can_save_as_complete) {
  // Deletes itself.
  new SavePackageFilePicker(
      save_package, suggested_path, can_save_as_complete);
}

void ChromeDownloadManagerDelegate::ChooseDownloadPath(
    DownloadManager* download_manager,
    TabContents* tab_contents,
    const FilePath& suggested_path,
    void* data) {
  // Deletes itself.
  new DownloadFilePicker(
      download_manager, tab_contents, suggested_path, data);
}

TabContents*
    ChromeDownloadManagerDelegate::GetAlternativeTabContentsToNotifyForDownload(
        DownloadManager* download_manager) {
  // Start the download in the last active browser. This is not ideal but better
  // than fully hiding the download from the user.
  Browser* last_active = BrowserList::GetLastActiveWithProfile(
      download_manager->profile());
  return last_active ? last_active->GetSelectedTabContents() : NULL;
}
