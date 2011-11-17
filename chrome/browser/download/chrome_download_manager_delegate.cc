// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/chrome_download_manager_delegate.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_crx_util.h"
#include "chrome/browser/download/download_extensions.h"
#include "chrome/browser/download/download_file_picker.h"
#include "chrome/browser/download/download_history.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/download/save_package_file_picker.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/user_script.h"
#include "chrome/common/pref_names.h"
#include "content/browser/download/download_file.h"
#include "content/browser/download/download_item.h"
#include "content/browser/download/download_manager.h"
#include "content/browser/download/download_status_updater.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/notification_source.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;
using safe_browsing::DownloadProtectionService;

ChromeDownloadManagerDelegate::ChromeDownloadManagerDelegate(Profile* profile)
    : profile_(profile),
      download_prefs_(new DownloadPrefs(profile->GetPrefs())) {
}

ChromeDownloadManagerDelegate::~ChromeDownloadManagerDelegate() {
}

bool ChromeDownloadManagerDelegate::IsExtensionDownload(
    const DownloadItem* item) {
  if (item->prompt_user_for_save_location())
    return false;

  return (item->mime_type() == Extension::kMimeType) ||
      UserScript::IsURLUserScript(item->GetURL(), item->mime_type());
}

void ChromeDownloadManagerDelegate::SetDownloadManager(DownloadManager* dm) {
  download_manager_ = dm;
  download_history_.reset(new DownloadHistory(profile_));
  download_history_->Load(
      base::Bind(&DownloadManager::OnPersistentStoreQueryComplete,
                 base::Unretained(dm)));
}

void ChromeDownloadManagerDelegate::Shutdown() {
  download_history_.reset();
  download_prefs_.reset();
}

bool ChromeDownloadManagerDelegate::ShouldStartDownload(int32 download_id) {
  // We create a download item and store it in our download map, and inform the
  // history system of a new download. Since this method can be called while the
  // history service thread is still reading the persistent state, we do not
  // insert the new DownloadItem into 'history_downloads_' or inform our
  // observers at this point. OnCreateDownloadEntryComplete() handles that
  // finalization of the the download creation as a callback from the history
  // thread.
  DownloadItem* download =
      download_manager_->GetActiveDownloadItem(download_id);
  if (!download)
    return false;

#if defined(ENABLE_SAFE_BROWSING)
  SafeBrowsingService* sb_service =
      g_browser_process->safe_browsing_service();
  if (sb_service && sb_service->download_protection_service() &&
      profile_->GetPrefs()->GetBoolean(prefs::kSafeBrowsingEnabled)) {
    VLOG(2) << __FUNCTION__ << "() Start SB URL check for download = "
            << download->DebugString(false);
    sb_service->download_protection_service()->CheckDownloadUrl(
        DownloadProtectionService::DownloadInfo::FromDownloadItem(*download),
        base::Bind(
            &ChromeDownloadManagerDelegate::CheckDownloadUrlDone,
            this,
            download->id()));
    return false;
  }
#endif
  CheckDownloadUrlDone(download_id, DownloadProtectionService::SAFE);
  return false;
}

void ChromeDownloadManagerDelegate::ChooseDownloadPath(
    TabContents* tab_contents,
    const FilePath& suggested_path,
    void* data) {
  // Deletes itself.
  new DownloadFilePicker(
      download_manager_, tab_contents, suggested_path, data);
}

bool ChromeDownloadManagerDelegate::OverrideIntermediatePath(
    DownloadItem* item,
    FilePath* intermediate_path) {
  if (item->IsDangerous()) {
    // The download is not safe.  It's name is already set to an intermediate
    // name, so no need to override.
    return false;
  }

  // The download is a safe download.  We need to rename it to its intermediate
  // '.crdownload' path.  The final name after user confirmation will be set
  // from DownloadItem::OnDownloadCompleting.
  *intermediate_path = download_util::GetCrDownloadPath(item->full_path());
  return true;
}

TabContents* ChromeDownloadManagerDelegate::
    GetAlternativeTabContentsToNotifyForDownload() {
  // Start the download in the last active browser. This is not ideal but better
  // than fully hiding the download from the user.
  Browser* last_active = BrowserList::GetLastActiveWithProfile(profile_);
  return last_active ? last_active->GetSelectedTabContents() : NULL;
}


bool ChromeDownloadManagerDelegate::ShouldOpenFileBasedOnExtension(
    const FilePath& path) {
  FilePath::StringType extension = path.Extension();
  if (extension.empty())
    return false;
  if (Extension::IsExtension(path))
    return false;
  DCHECK(extension[0] == FilePath::kExtensionSeparator);
  extension.erase(0, 1);
  return download_prefs_->IsAutoOpenEnabledForExtension(extension);
}

bool ChromeDownloadManagerDelegate::ShouldCompleteDownload(DownloadItem* item) {
#if defined(ENABLE_SAFE_BROWSING)
  // See if there is already a pending SafeBrowsing check for that download.
  SafeBrowsingStateMap::iterator it = safe_browsing_state_.find(item->id());
  if (it != safe_browsing_state_.end()) {
    SafeBrowsingState state = it->second;
    if (!state.pending) {
      safe_browsing_state_.erase(it);
    }
    return !state.pending;
  }
  // Begin the safe browsing download protection check.
  SafeBrowsingService* sb_service =
      g_browser_process->safe_browsing_service();
  if (sb_service && sb_service->download_protection_service() &&
      profile_->GetPrefs()->GetBoolean(prefs::kSafeBrowsingEnabled)) {
    VLOG(2) << __FUNCTION__ << "() Start SB download check for download = "
            << item->DebugString(false);
    sb_service->download_protection_service()->CheckClientDownload(
        DownloadProtectionService::DownloadInfo::FromDownloadItem(*item),
        base::Bind(
            &ChromeDownloadManagerDelegate::CheckClientDownloadDone,
            this,
            item->id()));
    SafeBrowsingState state;
    state.pending = true;
    state.verdict = DownloadProtectionService::SAFE;
    safe_browsing_state_[item->id()] = state;
    return false;
  }
#endif
  return true;
}

bool ChromeDownloadManagerDelegate::ShouldOpenDownload(DownloadItem* item) {
  if (!IsExtensionDownload(item)) {
    return true;
  }

  scoped_refptr<CrxInstaller> crx_installer =
      download_crx_util::OpenChromeExtension(profile_, *item);

  // CRX_INSTALLER_DONE will fire when the install completes.  Observe()
  // will call DelayedDownloadOpened() on this item.  If this DownloadItem is
  // not around when CRX_INSTALLER_DONE fires, Complete() will not be called.
  registrar_.Add(this,
                 chrome::NOTIFICATION_CRX_INSTALLER_DONE,
                 content::Source<CrxInstaller>(crx_installer.get()));

  crx_installers_[crx_installer.get()] = item->id();
  // The status text and percent complete indicator will change now
  // that we are installing a CRX.  Update observers so that they pick
  // up the change.
  item->UpdateObservers();
  return false;
}

bool ChromeDownloadManagerDelegate::GenerateFileHash() {
#if defined(ENABLE_SAFE_BROWSING)
  return profile_->GetPrefs()->GetBoolean(prefs::kSafeBrowsingEnabled) &&
      g_browser_process->safe_browsing_service()->DownloadBinHashNeeded();
#else
  return false;
#endif
}

void ChromeDownloadManagerDelegate::OnResponseCompleted(DownloadItem* item) {
  // TODO(noelutz): remove this method from the delegate API.
}

void ChromeDownloadManagerDelegate::AddItemToPersistentStore(
    DownloadItem* item) {
  download_history_->AddEntry(item,
      base::Bind(&ChromeDownloadManagerDelegate::OnItemAddedToPersistentStore,
                 base::Unretained(this)));
}

void ChromeDownloadManagerDelegate::UpdateItemInPersistentStore(
    DownloadItem* item) {
  download_history_->UpdateEntry(item);
}

void ChromeDownloadManagerDelegate::UpdatePathForItemInPersistentStore(
    DownloadItem* item,
    const FilePath& new_path) {
  download_history_->UpdateDownloadPath(item, new_path);
}

void ChromeDownloadManagerDelegate::RemoveItemFromPersistentStore(
    DownloadItem* item) {
  download_history_->RemoveEntry(item);
}

void ChromeDownloadManagerDelegate::RemoveItemsFromPersistentStoreBetween(
    const base::Time remove_begin,
    const base::Time remove_end) {
  download_history_->RemoveEntriesBetween(remove_begin, remove_end);
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
      save_package, suggested_path, can_save_as_complete,
      download_prefs_.get());
}

void ChromeDownloadManagerDelegate::DownloadProgressUpdated() {
  if (!g_browser_process->download_status_updater())
    return;

  float progress = 0;
  int download_count = 0;
  bool progress_known =
      g_browser_process->download_status_updater()->GetProgress(
          &progress, &download_count);
  download_util::UpdateAppIconDownloadProgress(
      download_count, progress_known, progress);
}

void ChromeDownloadManagerDelegate::CheckDownloadUrlDone(
    int32 download_id,
    DownloadProtectionService::DownloadCheckResult result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DownloadItem* download =
      download_manager_->GetActiveDownloadItem(download_id);
  if (!download)
    return;

  VLOG(2) << __FUNCTION__ << "() download = " << download->DebugString(false)
          << " verdict = " << result;
  if (result == DownloadProtectionService::DANGEROUS)
    download->MarkUrlDangerous();

  download_history_->CheckVisitedReferrerBefore(
      download_id, download->referrer_url(),
      base::Bind(&ChromeDownloadManagerDelegate::CheckVisitedReferrerBeforeDone,
                 base::Unretained(this)));
}

void ChromeDownloadManagerDelegate::CheckClientDownloadDone(
    int32 download_id,
    DownloadProtectionService::DownloadCheckResult result) {
  DownloadItem* item = download_manager_->GetActiveDownloadItem(download_id);
  if (!item) {
    safe_browsing_state_.erase(download_id);  // Just in case.
    return;
  }

  VLOG(2) << __FUNCTION__ << "() download = " << item->DebugString(false)
          << " verdict = " << result;
  // TODO(noelutz):
  //  1) display a warning if the result is DANGEROUS.
  //  2) make sure we haven't already displayed a warning for the URL.
  //  3) disable the existing dangerous file warning for executables.

  SafeBrowsingStateMap::iterator it = safe_browsing_state_.find(item->id());
  DCHECK(it != safe_browsing_state_.end() && it->second.pending);
  if (it != safe_browsing_state_.end()) {
    it->second.pending = false;
    it->second.verdict = result;
  }
  download_manager_->MaybeCompleteDownload(item);
}

// content::NotificationObserver implementation.
void ChromeDownloadManagerDelegate::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_CRX_INSTALLER_DONE);

  registrar_.Remove(this,
                    chrome::NOTIFICATION_CRX_INSTALLER_DONE,
                    source);

  CrxInstaller* installer = content::Source<CrxInstaller>(source).ptr();
  int download_id = crx_installers_[installer];
  crx_installers_.erase(installer);

  DownloadItem* item = download_manager_->GetActiveDownloadItem(download_id);
  if (item)
    item->DelayedDownloadOpened();
}

void ChromeDownloadManagerDelegate::CheckVisitedReferrerBeforeDone(
    int32 download_id,
    bool visited_referrer_before) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DownloadItem* download =
      download_manager_->GetActiveDownloadItem(download_id);
  if (!download)
    return;

  // Check whether this download is for an extension install or not.
  // Allow extensions to be explicitly saved.
  DownloadStateInfo state = download->state_info();

  if (state.force_file_name.empty()) {
    FilePath generated_name;
    download_util::GenerateFileNameFromRequest(*download,
                                               &generated_name);

    // Freeze the user's preference for showing a Save As dialog.  We're going
    // to bounce around a bunch of threads and we don't want to worry about race
    // conditions where the user changes this pref out from under us.
    if (download_prefs_->PromptForDownload()) {
      // But ignore the user's preference for the following scenarios:
      // 1) Extension installation. Note that we only care here about the case
      //    where an extension is installed, not when one is downloaded with
      //    "save as...".
      // 2) Filetypes marked "always open." If the user just wants this file
      //    opened, don't bother asking where to keep it.
      if (!IsExtensionDownload(download) &&
          !ShouldOpenFileBasedOnExtension(generated_name))
        state.prompt_user_for_save_location = true;
    }
    if (download_prefs_->IsDownloadPathManaged()) {
      state.prompt_user_for_save_location = false;
    }

    // Determine the proper path for a download, by either one of the following:
    // 1) using the default download directory.
    // 2) prompting the user.
    if (state.prompt_user_for_save_location &&
        !download_manager_->LastDownloadPath().empty()) {
      state.suggested_path = download_manager_->LastDownloadPath();
    } else {
      state.suggested_path = download_prefs_->download_path();
    }
    state.suggested_path = state.suggested_path.Append(generated_name);
  } else {
    state.suggested_path = state.force_file_name;
  }

  if (!state.prompt_user_for_save_location && state.force_file_name.empty()) {
    state.is_dangerous_file =
        IsDangerousFile(*download, state, visited_referrer_before);
  }

  // We need to move over to the download thread because we don't want to stat
  // the suggested path on the UI thread.
  // We can only access preferences on the UI thread, so check the download path
  // now and pass the value to the FILE thread.
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&ChromeDownloadManagerDelegate::CheckIfSuggestedPathExists,
                 this, download->id(), state,
                 download_prefs_->download_path()));
}

void ChromeDownloadManagerDelegate::CheckIfSuggestedPathExists(
    int32 download_id,
    DownloadStateInfo state,
    const FilePath& default_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  // Make sure the default download directory exists.
  // TODO(phajdan.jr): only create the directory when we're sure the user
  // is going to save there and not to another directory of his choice.
  file_util::CreateDirectory(default_path);

  // Check writability of the suggested path. If we can't write to it, default
  // to the user's "My Documents" directory. We'll prompt them in this case.
  FilePath dir = state.suggested_path.DirName();
  FilePath filename = state.suggested_path.BaseName();
  if (!file_util::PathIsWritable(dir)) {
    VLOG(1) << "Unable to write to directory \"" << dir.value() << "\"";
    state.prompt_user_for_save_location = true;
    PathService::Get(chrome::DIR_USER_DOCUMENTS, &state.suggested_path);
    state.suggested_path = state.suggested_path.Append(filename);
  }

  // If the download is deemed dangerous, we'll use a temporary name for it.
  if (state.IsDangerous()) {
    state.target_name = FilePath(state.suggested_path).BaseName();
    // Create a temporary file to hold the file until the user approves its
    // download.
    FilePath::StringType file_name;
    FilePath path;
#if defined(OS_WIN)
    string16 unconfirmed_prefix =
        l10n_util::GetStringUTF16(IDS_DOWNLOAD_UNCONFIRMED_PREFIX);
#else
    std::string unconfirmed_prefix =
        l10n_util::GetStringUTF8(IDS_DOWNLOAD_UNCONFIRMED_PREFIX);
#endif

    while (path.empty()) {
      base::SStringPrintf(
          &file_name,
          unconfirmed_prefix.append(
              FILE_PATH_LITERAL(" %d.crdownload")).c_str(),
          base::RandInt(0, 100000));
      path = dir.Append(file_name);
      if (file_util::PathExists(path))
        path = FilePath();
    }
    state.suggested_path = path;
  } else {
    // Do not add the path uniquifier if we are saving to a specific path as in
    // the drag-out case.
    if (state.force_file_name.empty()) {
      state.path_uniquifier = download_util::GetUniquePathNumberWithCrDownload(
          state.suggested_path);
    }
    // We know the final path, build it if necessary.
    if (state.path_uniquifier > 0) {
      DownloadFile::AppendNumberToPath(&(state.suggested_path),
                                       state.path_uniquifier);
      // Setting path_uniquifier to 0 to make sure we don't try to unique it
      // later on.
      state.path_uniquifier = 0;
    } else if (state.path_uniquifier == -1) {
      // We failed to find a unique path.  We have to prompt the user.
      VLOG(1) << "Unable to find a unique path for suggested path \""
              << state.suggested_path.value() << "\"";
      state.prompt_user_for_save_location = true;
    }
  }

  // Create an empty file at the suggested path so that we don't allocate the
  // same "non-existant" path to multiple downloads.
  // See: http://code.google.com/p/chromium/issues/detail?id=3662
  if (!state.prompt_user_for_save_location &&
      state.force_file_name.empty()) {
    if (state.IsDangerous())
      file_util::WriteFile(state.suggested_path, "", 0);
    else
      file_util::WriteFile(download_util::GetCrDownloadPath(
          state.suggested_path), "", 0);
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&ChromeDownloadManagerDelegate::OnPathExistenceAvailable,
                 this, download_id, state));
}

void ChromeDownloadManagerDelegate::OnPathExistenceAvailable(
    int32 download_id,
    const DownloadStateInfo& new_state) {
  DownloadItem* download =
      download_manager_->GetActiveDownloadItem(download_id);
  if (!download)
    return;
  download->SetFileCheckResults(new_state);
  download_manager_->RestartDownload(download_id);
}

// TODO(phajdan.jr): This is apparently not being exercised in tests.
bool ChromeDownloadManagerDelegate::IsDangerousFile(
    const DownloadItem& download,
    const DownloadStateInfo& state,
    bool visited_referrer_before) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Anything loaded directly from the address bar is OK.
  if (state.transition_type & content::PAGE_TRANSITION_FROM_ADDRESS_BAR)
    return false;

  // Return false if this type of files is handled by the enhanced SafeBrowsing
  // download protection.
  // TODO(noelutz): implement that check.

  // Extensions that are not from the gallery are considered dangerous.
  if (IsExtensionDownload(&download)) {
    ExtensionService* service = profile_->GetExtensionService();
    if (!service || !service->IsDownloadFromGallery(download.GetURL(),
                                                    download.referrer_url()))
      return true;
  }

  // Anything the user has marked auto-open is OK if it's user-initiated.
  if (ShouldOpenFileBasedOnExtension(state.suggested_path) &&
      state.has_user_gesture)
    return false;

  // "Allow on user gesture" is OK when we have a user gesture and the hosting
  // page has been visited before today.
  download_util::DownloadDangerLevel danger_level =
      download_util::GetFileDangerLevel(state.suggested_path.BaseName());
  if (danger_level == download_util::AllowOnUserGesture)
    return !state.has_user_gesture || !visited_referrer_before;

  return danger_level == download_util::Dangerous;
}

void ChromeDownloadManagerDelegate::OnItemAddedToPersistentStore(
    int32 download_id, int64 db_handle) {
  // It's not immediately obvious, but HistoryBackend::CreateDownload() can
  // call this function with an invalid |db_handle|. For instance, this can
  // happen when the history database is offline. We cannot have multiple
  // DownloadItems with the same invalid db_handle, so we need to assign a
  // unique |db_handle| here.
  if (db_handle == DownloadItem::kUninitializedHandle)
    db_handle = download_history_->GetNextFakeDbHandle();
  download_manager_->OnItemAddedToPersistentStore(download_id, db_handle);
}
