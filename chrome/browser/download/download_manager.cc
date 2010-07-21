// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_manager.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/sys_string_conversions.h"
#include "base/task.h"
#include "build/build_config.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/download/download_file_manager.h"
#include "chrome/browser/download/download_item.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/history/download_types.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "net/base/mime_util.h"
#include "net/base/net_util.h"

#if defined(OS_WIN)
#include "app/win_util.h"
#include "base/registry.h"
#include "base/win_util.h"
#endif

namespace {

// Used to sort download items based on descending start time.
bool CompareStartTime(DownloadItem* first, DownloadItem* second) {
  return first->start_time() > second->start_time();
}

void DeleteDownloadedFile(const FilePath& path) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));

  // Make sure we only delete files.
  if (!file_util::DirectoryExists(path))
    file_util::Delete(path, false);
}

}  // namespace

// Our download table ID starts at 1, so we use 0 to represent a download that
// has started, but has not yet had its data persisted in the table. We use fake
// database handles in incognito mode starting at -1 and progressively getting
// more negative.
// static
const int DownloadManager::kUninitializedHandle = 0;

// static
void DownloadManager::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kPromptForDownload, false);
  prefs->RegisterStringPref(prefs::kDownloadExtensionsToOpen, "");
  prefs->RegisterBooleanPref(prefs::kDownloadDirUpgraded, false);

  // The default download path is userprofile\download.
  const FilePath& default_download_path =
      download_util::GetDefaultDownloadDirectory();
  prefs->RegisterFilePathPref(prefs::kDownloadDefaultDirectory,
                              default_download_path);

  // If the download path is dangerous we forcefully reset it. But if we do
  // so we set a flag to make sure we only do it once, to avoid fighting
  // the user if he really wants it on an unsafe place such as the desktop.

  if (!prefs->GetBoolean(prefs::kDownloadDirUpgraded)) {
    FilePath current_download_dir = prefs->GetFilePath(
        prefs::kDownloadDefaultDirectory);
    if (download_util::DownloadPathIsDangerous(current_download_dir)) {
      prefs->SetFilePath(prefs::kDownloadDefaultDirectory,
                         default_download_path);
    }
    prefs->SetBoolean(prefs::kDownloadDirUpgraded, true);
  }
}

DownloadManager::DownloadManager()
    : shutdown_needed_(false),
      profile_(NULL),
      file_manager_(NULL),
      fake_db_handle_(kUninitializedHandle - 1) {
}

DownloadManager::~DownloadManager() {
  FOR_EACH_OBSERVER(Observer, observers_, ManagerGoingDown());

  if (shutdown_needed_)
    Shutdown();
}

void DownloadManager::Shutdown() {
  DCHECK(shutdown_needed_) << "Shutdown called when not needed.";

  // Stop receiving download updates
  if (file_manager_)
    file_manager_->RemoveDownloadManager(this);

  // Stop making history service requests
  cancelable_consumer_.CancelAllRequests();

  // 'in_progress_' may contain DownloadItems that have not finished the start
  // complete (from the history service) and thus aren't in downloads_.
  DownloadMap::iterator it = in_progress_.begin();
  std::set<DownloadItem*> to_remove;
  for (; it != in_progress_.end(); ++it) {
    DownloadItem* download = it->second;
    if (download->safety_state() == DownloadItem::DANGEROUS) {
      // Forget about any download that the user did not approve.
      // Note that we cannot call download->Remove() this would invalidate our
      // iterator.
      to_remove.insert(download);
      continue;
    }
    DCHECK_EQ(DownloadItem::IN_PROGRESS, download->state());
    download->Cancel(false);
    UpdateHistoryForDownload(download);
    if (download->db_handle() == kUninitializedHandle) {
      // An invalid handle means that 'download' does not yet exist in
      // 'downloads_', so we have to delete it here.
      delete download;
    }
  }

  // 'dangerous_finished_' contains all complete downloads that have not been
  // approved.  They should be removed.
  it = dangerous_finished_.begin();
  for (; it != dangerous_finished_.end(); ++it)
    to_remove.insert(it->second);

  // Remove the dangerous download that are not approved.
  for (std::set<DownloadItem*>::const_iterator rm_it = to_remove.begin();
       rm_it != to_remove.end(); ++rm_it) {
    DownloadItem* download = *rm_it;
    int64 handle = download->db_handle();
    download->Remove(true);
    // Same as above, delete the download if it is not in 'downloads_' (as the
    // Remove() call above won't have deleted it).
    if (handle == kUninitializedHandle)
      delete download;
  }
  to_remove.clear();

  in_progress_.clear();
  dangerous_finished_.clear();
  STLDeleteValues(&downloads_);

  file_manager_ = NULL;

  // Save our file extensions to auto open.
  SaveAutoOpens();

  // Make sure the save as dialog doesn't notify us back if we're gone before
  // it returns.
  if (select_file_dialog_.get())
    select_file_dialog_->ListenerDestroyed();

  shutdown_needed_ = false;
}

// Issue a history query for downloads matching 'search_text'. If 'search_text'
// is empty, return all downloads that we know about.
void DownloadManager::GetDownloads(Observer* observer,
                                   const std::wstring& search_text) {
  std::vector<DownloadItem*> otr_downloads;

  if (profile_->IsOffTheRecord() && search_text.empty()) {
    // List all incognito downloads and add that to the downloads the parent
    // profile lists.
    otr_downloads.reserve(downloads_.size());
    for (DownloadMap::iterator it = downloads_.begin();
         it != downloads_.end(); ++it) {
      DownloadItem* download = it->second;
      if (download->is_otr() && !download->is_extension_install() &&
          !download->is_temporary()) {
        otr_downloads.push_back(download);
      }
    }
  }

  profile_->GetOriginalProfile()->GetDownloadManager()->
    DoGetDownloads(observer, search_text, otr_downloads);
}

void DownloadManager::DoGetDownloads(
    Observer* observer,
    const std::wstring& search_text,
    std::vector<DownloadItem*>& otr_downloads) {
  DCHECK(observer);

  // Return a empty list if we've not yet received the set of downloads from the
  // history system (we'll update all observers once we get that list in
  // OnQueryDownloadEntriesComplete), or if there are no downloads at all.
  if (downloads_.empty()) {
    observer->SetDownloads(otr_downloads);
    return;
  }

  std::vector<DownloadItem*> download_copy;
  // We already know all the downloads and there is no filter, so just return a
  // copy to the observer.
  if (search_text.empty()) {
    download_copy.reserve(downloads_.size());
    for (DownloadMap::iterator it = downloads_.begin();
         it != downloads_.end(); ++it) {
      if (it->second->db_handle() > kUninitializedHandle)
        download_copy.push_back(it->second);
    }

    // Merge sort based on start time.
    std::vector<DownloadItem*> merged_downloads;
    std::merge(otr_downloads.begin(), otr_downloads.end(),
               download_copy.begin(), download_copy.end(),
               std::back_inserter(merged_downloads),
               CompareStartTime);

    // We retain ownership of the DownloadItems.
    observer->SetDownloads(merged_downloads);
    return;
  }

  DCHECK(otr_downloads.empty());

  // Issue a request to the history service for a list of downloads matching
  // our search text.
  HistoryService* hs =
      profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (hs) {
    HistoryService::Handle h =
        hs->SearchDownloads(WideToUTF16(search_text),
                            &cancelable_consumer_,
                            NewCallback(this,
                                        &DownloadManager::OnSearchComplete));
    cancelable_consumer_.SetClientData(hs, h, observer);
  }
}

void DownloadManager::GetTemporaryDownloads(Observer* observer,
                                            const FilePath& dir_path) {
  DCHECK(observer);

  std::vector<DownloadItem*> download_copy;

  for (DownloadMap::iterator it = downloads_.begin();
       it != downloads_.end(); ++it) {
    if (it->second->is_temporary() &&
        it->second->full_path().DirName() == dir_path)
      download_copy.push_back(it->second);
  }

  observer->SetDownloads(download_copy);
}

void DownloadManager::GetAllDownloads(Observer* observer,
                                      const FilePath& dir_path) {
  DCHECK(observer);

  std::vector<DownloadItem*> download_copy;

  for (DownloadMap::iterator it = downloads_.begin();
       it != downloads_.end(); ++it) {
    if (!it->second->is_temporary() &&
        (dir_path.empty() || it->second->full_path().DirName() == dir_path))
      download_copy.push_back(it->second);
  }

  observer->SetDownloads(download_copy);
}

void DownloadManager::GetCurrentDownloads(Observer* observer,
                                          const FilePath& dir_path) {
  DCHECK(observer);

  std::vector<DownloadItem*> download_copy;

  for (DownloadMap::iterator it = downloads_.begin();
       it != downloads_.end(); ++it) {
    if (!it->second->is_temporary() &&
        (it->second->state() == DownloadItem::IN_PROGRESS ||
         it->second->safety_state() == DownloadItem::DANGEROUS) &&
        (dir_path.empty() || it->second->full_path().DirName() == dir_path))
      download_copy.push_back(it->second);
  }

  observer->SetDownloads(download_copy);
}

// Query the history service for information about all persisted downloads.
bool DownloadManager::Init(Profile* profile) {
  DCHECK(profile);
  DCHECK(!shutdown_needed_)  << "DownloadManager already initialized.";
  shutdown_needed_ = true;

  profile_ = profile;
  request_context_getter_ = profile_->GetRequestContext();

  // 'incognito mode' will have access to past downloads, but we won't store
  // information about new downloads while in that mode.
  QueryHistoryForDownloads();

  // Cleans up entries only when called for the first time. Subsequent calls are
  // a no op.
  CleanUpInProgressHistoryEntries();

  // In test mode, there may be no ResourceDispatcherHost.  In this case it's
  // safe to avoid setting |file_manager_| because we only call a small set of
  // functions, none of which need it.
  ResourceDispatcherHost* rdh = g_browser_process->resource_dispatcher_host();
  if (rdh) {
    file_manager_ = rdh->download_file_manager();
    DCHECK(file_manager_);
  }

  // Get our user preference state.
  PrefService* prefs = profile_->GetPrefs();
  DCHECK(prefs);
  prompt_for_download_.Init(prefs::kPromptForDownload, prefs, NULL);

  download_path_.Init(prefs::kDownloadDefaultDirectory, prefs, NULL);

  // Ensure that the download directory specified in the preferences exists.
  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableFunction(&file_util::CreateDirectory, download_path()));

  // We store any file extension that should be opened automatically at
  // download completion in this pref.
  std::string extensions_to_open =
      prefs->GetString(prefs::kDownloadExtensionsToOpen);
  std::vector<std::string> extensions;
  SplitString(extensions_to_open, ':', &extensions);

  for (size_t i = 0; i < extensions.size(); ++i) {
#if defined(OS_POSIX)
    FilePath path(extensions[i]);
#elif defined(OS_WIN)
    FilePath path(UTF8ToWide(extensions[i]));
#endif
    if (!extensions[i].empty() && !IsExecutableFile(path))
      auto_open_.insert(path.value());
  }

  other_download_manager_observer_.reset(
      new OtherDownloadManagerObserver(this));

  return true;
}

void DownloadManager::QueryHistoryForDownloads() {
  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (hs) {
    hs->QueryDownloads(
        &cancelable_consumer_,
        NewCallback(this, &DownloadManager::OnQueryDownloadEntriesComplete));
  }
}

void DownloadManager::CleanUpInProgressHistoryEntries() {
  static bool already_cleaned_up = false;

  if (!already_cleaned_up) {
    HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
    if (hs) {
      hs->CleanUpInProgressEntries();
      already_cleaned_up = true;
    }
  }
}

// We have received a message from DownloadFileManager about a new download. We
// create a download item and store it in our download map, and inform the
// history system of a new download. Since this method can be called while the
// history service thread is still reading the persistent state, we do not
// insert the new DownloadItem into 'downloads_' or inform our observers at this
// point. OnCreateDatabaseEntryComplete() handles that finalization of the the
// download creation as a callback from the history thread.
void DownloadManager::StartDownload(DownloadCreateInfo* info) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  DCHECK(info);

  // Check whether this download is for an extension install or not.
  // Allow extensions to be explicitly saved.
  if (!info->prompt_user_for_save_location) {
    if (UserScript::HasUserScriptFileExtension(info->url) ||
        info->mime_type == Extension::kMimeType)
      info->is_extension_install = true;
  }

  if (info->save_info.file_path.empty()) {
    FilePath generated_name;
    GenerateFileNameFromInfo(info, &generated_name);

    // Freeze the user's preference for showing a Save As dialog.  We're going
    // to bounce around a bunch of threads and we don't want to worry about race
    // conditions where the user changes this pref out from under us.
    if (*prompt_for_download_) {
      // But ignore the user's preference for the following scenarios:
      // 1) Extension installation. Note that we only care here about the case
      //    where an extension is installed, not when one is downloaded with
      //    "save as...".
      // 2) Filetypes marked "always open." If the user just wants this file
      //    opened, don't bother asking where to keep it.
      if (!info->is_extension_install &&
          !ShouldOpenFileBasedOnExtension(generated_name))
        info->prompt_user_for_save_location = true;
    }

    // Determine the proper path for a download, by either one of the following:
    // 1) using the default download directory.
    // 2) prompting the user.
    if (info->prompt_user_for_save_location && !last_download_path_.empty())
      info->suggested_path = last_download_path_;
    else
      info->suggested_path = download_path();
    info->suggested_path = info->suggested_path.Append(generated_name);
  } else {
    info->suggested_path = info->save_info.file_path;
  }

  if (!info->prompt_user_for_save_location &&
      info->save_info.file_path.empty()) {
    // Downloads can be marked as dangerous for two reasons:
    // a) They have a dangerous-looking filename
    // b) They are an extension that is not from the gallery
    if (IsDangerous(info->suggested_path.BaseName()))
      info->is_dangerous = true;
    else if (info->is_extension_install &&
             !ExtensionsService::IsDownloadFromGallery(info->url,
                                                       info->referrer_url)) {
      info->is_dangerous = true;
    }
  }

  // We need to move over to the download thread because we don't want to stat
  // the suggested path on the UI thread.
  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(
          this, &DownloadManager::CheckIfSuggestedPathExists, info));
}

void DownloadManager::CheckIfSuggestedPathExists(DownloadCreateInfo* info) {
  DCHECK(info);

  // Check writability of the suggested path. If we can't write to it, default
  // to the user's "My Documents" directory. We'll prompt them in this case.
  FilePath dir = info->suggested_path.DirName();
  FilePath filename = info->suggested_path.BaseName();
  if (!file_util::PathIsWritable(dir)) {
    info->prompt_user_for_save_location = true;
    PathService::Get(chrome::DIR_USER_DOCUMENTS, &info->suggested_path);
    info->suggested_path = info->suggested_path.Append(filename);
  }

  // If the download is deemed dangerous, we'll use a temporary name for it.
  if (info->is_dangerous) {
    info->original_name = FilePath(info->suggested_path).BaseName();
    // Create a temporary file to hold the file until the user approves its
    // download.
    FilePath::StringType file_name;
    FilePath path;
    while (path.empty()) {
      SStringPrintf(&file_name, FILE_PATH_LITERAL("unconfirmed %d.crdownload"),
                    base::RandInt(0, 100000));
      path = dir.Append(file_name);
      if (file_util::PathExists(path))
        path = FilePath();
    }
    info->suggested_path = path;
  } else {
    // Do not add the path uniquifier if we are saving to a specific path as in
    // the drag-out case.
    if (info->save_info.file_path.empty()) {
      info->path_uniquifier = download_util::GetUniquePathNumberWithCrDownload(
          info->suggested_path);
    }
    // We know the final path, build it if necessary.
    if (info->path_uniquifier > 0) {
      download_util::AppendNumberToPath(&(info->suggested_path),
                                        info->path_uniquifier);
      // Setting path_uniquifier to 0 to make sure we don't try to unique it
      // later on.
      info->path_uniquifier = 0;
    } else if (info->path_uniquifier == -1) {
      // We failed to find a unique path.  We have to prompt the user.
      info->prompt_user_for_save_location = true;
    }
  }

  // Create an empty file at the suggested path so that we don't allocate the
  // same "non-existant" path to multiple downloads.
  // See: http://code.google.com/p/chromium/issues/detail?id=3662
  if (!info->prompt_user_for_save_location &&
      info->save_info.file_path.empty()) {
    if (info->is_dangerous)
      file_util::WriteFile(info->suggested_path, "", 0);
    else
      file_util::WriteFile(download_util::GetCrDownloadPath(
          info->suggested_path), "", 0);
  }

  // Now we return to the UI thread.
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(this,
                        &DownloadManager::OnPathExistenceAvailable,
                        info));
}

void DownloadManager::OnPathExistenceAvailable(DownloadCreateInfo* info) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  DCHECK(info);

  if (info->prompt_user_for_save_location) {
    // We must ask the user for the place to put the download.
    if (!select_file_dialog_.get())
      select_file_dialog_ = SelectFileDialog::Create(this);

    TabContents* contents = tab_util::GetTabContentsByID(info->child_id,
                                                         info->render_view_id);
    SelectFileDialog::FileTypeInfo file_type_info;
    file_type_info.extensions.resize(1);
    file_type_info.extensions[0].push_back(info->suggested_path.Extension());
    if (!file_type_info.extensions[0][0].empty())
      file_type_info.extensions[0][0].erase(0, 1);  // drop the .
    file_type_info.include_all_files = true;
    gfx::NativeWindow owning_window =
        contents ? platform_util::GetTopLevel(contents->GetNativeView()) : NULL;
    select_file_dialog_->SelectFile(SelectFileDialog::SELECT_SAVEAS_FILE,
                                    string16(),
                                    info->suggested_path,
                                    &file_type_info, 0, FILE_PATH_LITERAL(""),
                                    owning_window, info);
  } else {
    // No prompting for download, just continue with the suggested name.
    ContinueStartDownload(info, info->suggested_path);
  }
}

void DownloadManager::ContinueStartDownload(DownloadCreateInfo* info,
                                            const FilePath& target_path) {
  scoped_ptr<DownloadCreateInfo> infop(info);
  info->path = target_path;

  DownloadItem* download = NULL;
  DownloadMap::iterator it = in_progress_.find(info->download_id);
  if (it == in_progress_.end()) {
    download = new DownloadItem(info->download_id,
                                info->path,
                                info->path_uniquifier,
                                info->url,
                                info->referrer_url,
                                info->mime_type,
                                info->original_mime_type,
                                info->original_name,
                                info->start_time,
                                info->total_bytes,
                                info->child_id,
                                info->request_id,
                                info->is_dangerous,
                                info->prompt_user_for_save_location,
                                profile_->IsOffTheRecord(),
                                info->is_extension_install,
                                !info->save_info.file_path.empty());
    download->set_manager(this);
    in_progress_[info->download_id] = download;
  } else {
    NOTREACHED();  // Should not exist!
    return;
  }

  PendingFinishedMap::iterator pending_it =
      pending_finished_downloads_.find(info->download_id);
  bool download_finished = (pending_it != pending_finished_downloads_.end());

  if (download_finished || info->is_dangerous) {
    // The download has already finished or the download is not safe.
    // We can now rename the file to its final name (or its tentative name
    // in dangerous download cases).
    ChromeThread::PostTask(
        ChromeThread::FILE, FROM_HERE,
        NewRunnableMethod(
            file_manager_, &DownloadFileManager::OnFinalDownloadName,
            download->id(), target_path, !info->is_dangerous, this));
  } else {
    // The download hasn't finished and it is a safe download.  We need to
    // rename it to its intermediate '.crdownload' path.
    FilePath download_path = download_util::GetCrDownloadPath(target_path);
    ChromeThread::PostTask(
        ChromeThread::FILE, FROM_HERE,
        NewRunnableMethod(
            file_manager_, &DownloadFileManager::OnIntermediateDownloadName,
            download->id(), download_path, this));
    download->set_need_final_rename(true);
  }

  if (download_finished) {
    // If the download already completed by the time we reached this point, then
    // notify observers that it did.
    DownloadFinished(pending_it->first, pending_it->second);
  }

  download->Rename(target_path);

  // Do not store the download in the history database for a few special cases:
  // - incognito mode (that is the point of this mode)
  // - extensions (users don't think of extension installation as 'downloading')
  // - temporary download, like in drag-and-drop
  // We have to make sure that these handles don't collide with normal db
  // handles, so we use a negative value. Eventually, they could overlap, but
  // you'd have to do enough downloading that your ISP would likely stab you in
  // the neck first. YMMV.
  if (download->is_otr() || download->is_extension_install() ||
      download->is_temporary()) {
    OnCreateDownloadEntryComplete(*info, fake_db_handle_.GetNext());
  } else {
    // Update the history system with the new download.
    // FIXME(paulg) see bug 958058. EXPLICIT_ACCESS below is wrong.
    HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
    if (hs) {
      hs->CreateDownload(
          *info, &cancelable_consumer_,
          NewCallback(this, &DownloadManager::OnCreateDownloadEntryComplete));
    }
  }

  UpdateAppIcon();
}

// Convenience function for updating the history service for a download.
void DownloadManager::UpdateHistoryForDownload(DownloadItem* download) {
  DCHECK(download);

  // Don't store info in the database if the download was initiated while in
  // incognito mode or if it hasn't been initialized in our database table.
  if (download->db_handle() <= kUninitializedHandle)
    return;

  // FIXME(paulg) see bug 958058. EXPLICIT_ACCESS below is wrong.
  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (hs) {
    hs->UpdateDownload(download->received_bytes(),
                       download->state(),
                       download->db_handle());
  }
}

void DownloadManager::RemoveDownloadFromHistory(DownloadItem* download) {
  DCHECK(download);
  // FIXME(paulg) see bug 958058. EXPLICIT_ACCESS below is wrong.
  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (download->db_handle() > kUninitializedHandle && hs)
    hs->RemoveDownload(download->db_handle());
}

void DownloadManager::RemoveDownloadsFromHistoryBetween(
    const base::Time remove_begin,
    const base::Time remove_end) {
  // FIXME(paulg) see bug 958058. EXPLICIT_ACCESS below is wrong.
  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (hs)
    hs->RemoveDownloadsBetween(remove_begin, remove_end);
}

void DownloadManager::UpdateDownload(int32 download_id, int64 size) {
  DownloadMap::iterator it = in_progress_.find(download_id);
  if (it != in_progress_.end()) {
    DownloadItem* download = it->second;
    download->Update(size);
    UpdateHistoryForDownload(download);
  }
  UpdateAppIcon();
}

void DownloadManager::DownloadFinished(int32 download_id, int64 size) {
  DownloadMap::iterator it = in_progress_.find(download_id);
  if (it == in_progress_.end()) {
    // The download is done, but the user hasn't selected a final location for
    // it yet (the Save As dialog box is probably still showing), so just keep
    // track of the fact that this download id is complete, when the
    // DownloadItem is constructed later we'll notify its completion then.
    PendingFinishedMap::iterator erase_it =
        pending_finished_downloads_.find(download_id);
    DCHECK(erase_it == pending_finished_downloads_.end());
    pending_finished_downloads_[download_id] = size;
    return;
  }

  // Remove the id from the list of pending ids.
  PendingFinishedMap::iterator erase_it =
      pending_finished_downloads_.find(download_id);
  if (erase_it != pending_finished_downloads_.end())
    pending_finished_downloads_.erase(erase_it);

  DownloadItem* download = it->second;
  download->Finished(size);

  // Clean up will happen when the history system create callback runs if we
  // don't have a valid db_handle yet.
  if (download->db_handle() != kUninitializedHandle) {
    in_progress_.erase(it);
    UpdateHistoryForDownload(download);
  }

  UpdateAppIcon();

  // If this a dangerous download not yet validated by the user, don't do
  // anything. When the user notifies us, it will trigger a call to
  // ProceedWithFinishedDangerousDownload.
  if (download->safety_state() == DownloadItem::DANGEROUS) {
    dangerous_finished_[download_id] = download;
    return;
  }

  if (download->safety_state() == DownloadItem::DANGEROUS_BUT_VALIDATED) {
    // We first need to rename the downloaded file from its temporary name to
    // its final name before we can continue.
    ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
        NewRunnableMethod(
            this, &DownloadManager::ProceedWithFinishedDangerousDownload,
            download->db_handle(),
            download->full_path(), download->original_name()));
    return;
  }

  if (download->need_final_rename()) {
    ChromeThread::PostTask(
        ChromeThread::FILE, FROM_HERE,
        NewRunnableMethod(
            file_manager_, &DownloadFileManager::OnFinalDownloadName,
            download->id(), download->full_path(), false, this));
    return;
  }

  ContinueDownloadFinished(download);
}

void DownloadManager::DownloadRenamedToFinalName(int download_id,
                                                 const FilePath& full_path) {
  DownloadMap::iterator it = downloads_.begin();
  while (it != downloads_.end()) {
    DownloadItem* download = it->second;
    if (download->id() == download_id) {
      // The download file is meant to be completed if both the filename is
      // finalized and the file data is downloaded. The ordering of these two
      // actions is indeterministic. Thus, if we are still in downloading the
      // file, delay the notification.
      download->set_name_finalized(true);
      if (download->state() == DownloadItem::COMPLETE)
        download->NotifyObserversDownloadFileCompleted();

      // This was called from DownloadFinished; continue to call
      // ContinueDownloadFinished.
      if (download->need_final_rename()) {
        download->set_need_final_rename(false);
        ContinueDownloadFinished(download);
      }
      return;
    }
    it++;
  }
}

void DownloadManager::ContinueDownloadFinished(DownloadItem* download) {
  // If this was a dangerous download, it has now been approved and must be
  // removed from dangerous_finished_ so it does not get deleted on shutdown.
  DownloadMap::iterator it = dangerous_finished_.find(download->id());
  if (it != dangerous_finished_.end())
    dangerous_finished_.erase(it);

  // Handle chrome extensions explicitly and skip the shell execute.
  if (download->is_extension_install()) {
    OpenChromeExtension(download->full_path(),
                        download->url(),
                        download->referrer_url(),
                        download->original_mime_type());
    download->set_auto_opened(true);
  } else if (download->open_when_complete() ||
             ShouldOpenFileBasedOnExtension(download->full_path()) ||
             download->is_temporary()) {
    // If the download is temporary, like in drag-and-drop, do not open it but
    // we still need to set it auto-opened so that it can be removed from the
    // download shelf.
    if (!download->is_temporary())
      OpenDownloadInShell(download, NULL);
    download->set_auto_opened(true);
  }

  // Notify our observers that we are complete (the call to Finished() set the
  // state to complete but did not notify).
  download->UpdateObservers();

  // The download file is meant to be completed if both the filename is
  // finalized and the file data is downloaded. The ordering of these two
  // actions is indeterministic. Thus, if the filename is not finalized yet,
  // delay the notification.
  if (download->name_finalized())
    download->NotifyObserversDownloadFileCompleted();
}

// Called on the file thread.  Renames the downloaded file to its original name.
void DownloadManager::ProceedWithFinishedDangerousDownload(
    int64 download_handle,
    const FilePath& path,
    const FilePath& original_name) {
  bool success = false;
  FilePath new_path;
  int uniquifier = 0;
  if (file_util::PathExists(path)) {
    new_path = path.DirName().Append(original_name);
    // Make our name unique at this point, as if a dangerous file is downloading
    // and a 2nd download is started for a file with the same name, they would
    // have the same path.  This is because we uniquify the name on download
    // start, and at that time the first file does not exists yet, so the second
    // file gets the same name.
    uniquifier = download_util::GetUniquePathNumber(new_path);
    if (uniquifier > 0)
      download_util::AppendNumberToPath(&new_path, uniquifier);
    success = file_util::Move(path, new_path);
  } else {
    NOTREACHED();
  }

  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(this, &DownloadManager::DangerousDownloadRenamed,
                        download_handle, success, new_path, uniquifier));
}

// Call from the file thread when the finished dangerous download was renamed.
void DownloadManager::DangerousDownloadRenamed(int64 download_handle,
                                               bool success,
                                               const FilePath& new_path,
                                               int new_path_uniquifier) {
  DownloadMap::iterator it = downloads_.find(download_handle);
  if (it == downloads_.end()) {
    NOTREACHED();
    return;
  }

  DownloadItem* download = it->second;
  // If we failed to rename the file, we'll just keep the name as is.
  if (success) {
    // We need to update the path uniquifier so that the UI shows the right
    // name when calling GetFileName().
    download->set_path_uniquifier(new_path_uniquifier);
    RenameDownload(download, new_path);
  }

  // Continue the download finished sequence.
  ContinueDownloadFinished(download);
}

void DownloadManager::DownloadCancelled(int32 download_id) {
  DownloadMap::iterator it = in_progress_.find(download_id);
  if (it == in_progress_.end())
    return;
  DownloadItem* download = it->second;

  // Clean up will happen when the history system create callback runs if we
  // don't have a valid db_handle yet.
  if (download->db_handle() != kUninitializedHandle) {
    in_progress_.erase(it);
    UpdateHistoryForDownload(download);
  }

  DownloadCancelledInternal(download_id,
                            download->render_process_id(),
                            download->request_id());
  UpdateAppIcon();
}

void DownloadManager::DownloadCancelledInternal(int download_id,
                                                int render_process_id,
                                                int request_id) {
  // Cancel the network request.  RDH is guaranteed to outlive the IO thread.
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableFunction(&download_util::CancelDownloadRequest,
                          g_browser_process->resource_dispatcher_host(),
                          render_process_id,
                          request_id));

  // Tell the file manager to cancel the download.
  file_manager_->RemoveDownload(download_id, this);  // On the UI thread
  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(
          file_manager_, &DownloadFileManager::CancelDownload, download_id));
}

void DownloadManager::PauseDownload(int32 download_id, bool pause) {
  DownloadMap::iterator it = in_progress_.find(download_id);
  if (it == in_progress_.end())
    return;

  DownloadItem* download = it->second;
  if (pause == download->is_paused())
    return;

  // Inform the ResourceDispatcherHost of the new pause state.
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableFunction(&DownloadManager::OnPauseDownloadRequest,
                          g_browser_process->resource_dispatcher_host(),
                          download->render_process_id(),
                          download->request_id(),
                          pause));
}

// static
void DownloadManager::OnPauseDownloadRequest(ResourceDispatcherHost* rdh,
                                             int render_process_id,
                                             int request_id,
                                             bool pause) {
  rdh->PauseRequest(render_process_id, request_id, pause);
}

bool DownloadManager::IsDangerous(const FilePath& file_name) {
  // TODO(jcampan): Improve me.
  return IsExecutableFile(file_name);
}

void DownloadManager::UpdateAppIcon() {
  int64 total_bytes = 0;
  int64 received_bytes = 0;
  int download_count = 0;
  bool progress_known = true;

  for (DownloadMap::iterator i = in_progress_.begin();
       i != in_progress_.end();
       ++i) {
    ++download_count;
    const DownloadItem* item = i->second;
    if (item->total_bytes() > 0) {
      total_bytes += item->total_bytes();
      received_bytes += item->received_bytes();
    } else {
      // This download didn't specify a Content-Length, so the combined progress
      // bar neeeds to be indeterminate.
      progress_known = false;
    }
  }

  float progress = 0;
  if (progress_known && download_count)
    progress = (float)received_bytes / total_bytes;

  download_util::UpdateAppIconDownloadProgress(download_count,
                                               progress_known,
                                               progress);
}

void DownloadManager::RenameDownload(DownloadItem* download,
                                     const FilePath& new_path) {
  download->Rename(new_path);

  // Update the history.

  // No update necessary if the download was initiated while in incognito mode.
  if (download->db_handle() <= kUninitializedHandle)
    return;

  // FIXME(paulg) see bug 958058. EXPLICIT_ACCESS below is wrong.
  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (hs)
    hs->UpdateDownloadPath(new_path, download->db_handle());
}

void DownloadManager::RemoveDownload(int64 download_handle) {
  DownloadMap::iterator it = downloads_.find(download_handle);
  if (it == downloads_.end())
    return;

  // Make history update.
  DownloadItem* download = it->second;
  RemoveDownloadFromHistory(download);

  // Remove from our tables and delete.
  downloads_.erase(it);
  it = dangerous_finished_.find(download->id());
  if (it != dangerous_finished_.end())
    dangerous_finished_.erase(it);

  // Tell observers to refresh their views.
  NotifyModelChanged();

  delete download;
}

int DownloadManager::RemoveDownloadsBetween(const base::Time remove_begin,
                                            const base::Time remove_end) {
  RemoveDownloadsFromHistoryBetween(remove_begin, remove_end);

  DownloadMap::iterator it = downloads_.begin();
  std::vector<DownloadItem*> pending_deletes;
  while (it != downloads_.end()) {
    DownloadItem* download = it->second;
    DownloadItem::DownloadState state = download->state();
    if (download->start_time() >= remove_begin &&
        (remove_end.is_null() || download->start_time() < remove_end) &&
        (state == DownloadItem::COMPLETE ||
         state == DownloadItem::CANCELLED)) {
      // Remove from the map and move to the next in the list.
      downloads_.erase(it++);

      // Also remove it from any completed dangerous downloads.
      DownloadMap::iterator dit = dangerous_finished_.find(download->id());
      if (dit != dangerous_finished_.end())
        dangerous_finished_.erase(dit);

      pending_deletes.push_back(download);
    // Observer interface.

      continue;
    }

    ++it;
  }

  // Tell observers to refresh their views.
  int num_deleted = static_cast<int>(pending_deletes.size());
  if (num_deleted > 0)
    NotifyModelChanged();

  // Delete the download items after updating the observers.
  STLDeleteContainerPointers(pending_deletes.begin(), pending_deletes.end());
  pending_deletes.clear();

  return num_deleted;
}

int DownloadManager::RemoveDownloads(const base::Time remove_begin) {
  return RemoveDownloadsBetween(remove_begin, base::Time());
}

int DownloadManager::RemoveAllDownloads() {
  if (this != profile_->GetOriginalProfile()->GetDownloadManager()) {
    // This is an incognito downloader. Clear All should clear main download
    // manager as well.
    profile_->GetOriginalProfile()->GetDownloadManager()->RemoveAllDownloads();
  }
  // The null times make the date range unbounded.
  return RemoveDownloadsBetween(base::Time(), base::Time());
}

// Initiate a download of a specific URL. We send the request to the
// ResourceDispatcherHost, and let it send us responses like a regular
// download.
void DownloadManager::DownloadUrl(const GURL& url,
                                  const GURL& referrer,
                                  const std::string& referrer_charset,
                                  TabContents* tab_contents) {
  DownloadUrlToFile(url, referrer, referrer_charset, DownloadSaveInfo(),
                    tab_contents);
}

void DownloadManager::DownloadUrlToFile(const GURL& url,
                                        const GURL& referrer,
                                        const std::string& referrer_charset,
                                        const DownloadSaveInfo& save_info,
                                        TabContents* tab_contents) {
  DCHECK(tab_contents);
  ChromeThread::PostTask(ChromeThread::IO, FROM_HERE,
      NewRunnableFunction(&download_util::DownloadUrl,
                          url,
                          referrer,
                          referrer_charset,
                          save_info,
                          g_browser_process->resource_dispatcher_host(),
                          tab_contents->GetRenderProcessHost()->id(),
                          tab_contents->render_view_host()->routing_id(),
                          request_context_getter_));
}

void DownloadManager::GenerateExtension(
    const FilePath& file_name,
    const std::string& mime_type,
    FilePath::StringType* generated_extension) {
  // We're worried about three things here:
  //
  // 1) Security.  Many sites let users upload content, such as buddy icons, to
  //    their web sites.  We want to mitigate the case where an attacker
  //    supplies a malicious executable with an executable file extension but an
  //    honest site serves the content with a benign content type, such as
  //    image/jpeg.
  //
  // 2) Usability.  If the site fails to provide a file extension, we want to
  //    guess a reasonable file extension based on the content type.
  //
  // 3) Shell integration.  Some file extensions automatically integrate with
  //    the shell.  We block these extensions to prevent a malicious web site
  //    from integrating with the user's shell.

  static const FilePath::CharType default_extension[] =
      FILE_PATH_LITERAL("download");

  // See if our file name already contains an extension.
  FilePath::StringType extension = file_name.Extension();
  if (!extension.empty())
    extension.erase(extension.begin());  // Erase preceding '.'.

#if defined(OS_WIN)
  // Rename shell-integrated extensions.
  if (win_util::IsShellIntegratedExtension(extension))
    extension.assign(default_extension);
#endif

  std::string mime_type_from_extension;
  net::GetMimeTypeFromFile(file_name,
                           &mime_type_from_extension);
  if (mime_type == mime_type_from_extension) {
    // The hinted extension matches the mime type.  It looks like a winner.
    generated_extension->swap(extension);
    return;
  }

  if (IsExecutableExtension(extension) && !IsExecutableMimeType(mime_type)) {
    // We want to be careful about executable extensions.  The worry here is
    // that a trusted web site could be tricked into dropping an executable file
    // on the user's filesystem.
    if (!net::GetPreferredExtensionForMimeType(mime_type, &extension)) {
      // We couldn't find a good extension for this content type.  Use a dummy
      // extension instead.
      extension.assign(default_extension);
    }
  }

  if (extension.empty()) {
    net::GetPreferredExtensionForMimeType(mime_type, &extension);
  } else {
    // Append extension generated from the mime type if:
    // 1. New extension is not ".txt"
    // 2. New extension is not the same as the already existing extension.
    // 3. New extension is not executable. This action mitigates the case when
    //    an executable is hidden in a benign file extension;
    //    E.g. my-cat.jpg becomes my-cat.jpg.js if content type is
    //         application/x-javascript.
    // 4. New extension is not ".tar" for .gz files. For misconfigured web
    //    servers, i.e. bug 5772.
    // 5. The original extension is not ".tgz" & the new extension is not "gz".
    FilePath::StringType append_extension;
    if (net::GetPreferredExtensionForMimeType(mime_type, &append_extension)) {
      if (append_extension != FILE_PATH_LITERAL("txt") &&
          append_extension != extension &&
          !IsExecutableExtension(append_extension) &&
          !(append_extension == FILE_PATH_LITERAL("gz") &&
            extension == FILE_PATH_LITERAL("tgz")) &&
          (append_extension != FILE_PATH_LITERAL("tar") ||
           extension != FILE_PATH_LITERAL("gz"))) {
        extension += FILE_PATH_LITERAL(".");
        extension += append_extension;
      }
    }
  }

  generated_extension->swap(extension);
}

void DownloadManager::GenerateFileNameFromInfo(DownloadCreateInfo* info,
                                               FilePath* generated_name) {
  GenerateFileName(GURL(info->url),
                   info->content_disposition,
                   info->referrer_charset,
                   info->mime_type,
                   generated_name);
}

void DownloadManager::GenerateFileName(const GURL& url,
                                       const std::string& content_disposition,
                                       const std::string& referrer_charset,
                                       const std::string& mime_type,
                                       FilePath* generated_name) {
  std::wstring default_name =
      l10n_util::GetString(IDS_DEFAULT_DOWNLOAD_FILENAME);
#if defined(OS_WIN)
  FilePath default_file_path(default_name);
#elif defined(OS_POSIX)
  FilePath default_file_path(base::SysWideToNativeMB(default_name));
#endif

  *generated_name = net::GetSuggestedFilename(GURL(url),
                                              content_disposition,
                                              referrer_charset,
                                              default_file_path);

  DCHECK(!generated_name->empty());

  GenerateSafeFileName(mime_type, generated_name);
}

void DownloadManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
  observer->ModelChanged();
}

void DownloadManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

// Post Windows Shell operations to the Download thread, to avoid blocking the
// user interface.
void DownloadManager::ShowDownloadInShell(const DownloadItem* download) {
  DCHECK(file_manager_);
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
#if defined(OS_MACOSX)
  // Mac needs to run this operation on the UI thread.
  platform_util::ShowItemInFolder(download->full_path());
#else
  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(
          file_manager_, &DownloadFileManager::OnShowDownloadInShell,
          FilePath(download->full_path())));
#endif
}

void DownloadManager::OpenDownload(const DownloadItem* download,
                                   gfx::NativeView parent_window) {
  // Open Chrome extensions with ExtensionsService. For everything else do shell
  // execute.
  if (download->is_extension_install()) {
    OpenChromeExtension(download->full_path(),
                        download->url(),
                        download->referrer_url(),
                        download->original_mime_type());
  } else {
    OpenDownloadInShell(download, parent_window);
  }
}

void DownloadManager::OpenChromeExtension(
    const FilePath& full_path,
    const GURL& download_url,
    const GURL& referrer_url,
    const std::string& original_mime_type) {
  // We don't support extensions in OTR mode.
  ExtensionsService* service = profile_->GetExtensionsService();
  if (service) {
    NotificationService* nservice = NotificationService::current();
    GURL nonconst_download_url = download_url;
    nservice->Notify(NotificationType::EXTENSION_READY_FOR_INSTALL,
                     Source<DownloadManager>(this),
                     Details<GURL>(&nonconst_download_url));

    scoped_refptr<CrxInstaller> installer(
        new CrxInstaller(service->install_directory(),
                         service,
                         new ExtensionInstallUI(profile_)));
    installer->set_delete_source(true);

    if (UserScript::HasUserScriptFileExtension(download_url)) {
      installer->InstallUserScript(full_path, download_url);
    } else {
      bool is_gallery_download =
          ExtensionsService::IsDownloadFromGallery(download_url, referrer_url);
      installer->set_original_mime_type(original_mime_type);
      installer->set_apps_require_extension_mime_type(true);
      installer->set_allow_privilege_increase(true);
      installer->set_original_url(download_url);
      installer->set_limit_web_extent_to_download_host(!is_gallery_download);
      installer->InstallCrx(full_path);
    }
  } else {
    TabContents* contents = NULL;
    // Get last active normal browser of profile.
    Browser* last_active = BrowserList::FindBrowserWithType(profile_,
        Browser::TYPE_NORMAL, true);
    if (last_active)
      contents = last_active->GetSelectedTabContents();
    if (contents) {
      contents->AddInfoBar(
          new SimpleAlertInfoBarDelegate(contents,
              l10n_util::GetString(
                  IDS_EXTENSION_INCOGNITO_INSTALL_INFOBAR_LABEL),
              ResourceBundle::GetSharedInstance().GetBitmapNamed(
                  IDR_INFOBAR_PLUGIN_INSTALL),
              true));
    }
  }
}

void DownloadManager::OpenDownloadInShell(const DownloadItem* download,
                                          gfx::NativeView parent_window) {
  DCHECK(file_manager_);
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
#if defined(OS_MACOSX)
  // Mac OS X requires opening downloads on the UI thread.
  platform_util::OpenItem(download->full_path());
#else
  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(
          file_manager_, &DownloadFileManager::OnOpenDownloadInShell,
          download->full_path(), download->url(), parent_window));
#endif
}

void DownloadManager::OpenFilesBasedOnExtension(
    const FilePath& path, bool open) {
  FilePath::StringType extension = path.Extension();
  if (extension.empty())
    return;
  DCHECK(extension[0] == FilePath::kExtensionSeparator);
  extension.erase(0, 1);
  if (open && !IsExecutableExtension(extension))
    auto_open_.insert(extension);
  else
    auto_open_.erase(extension);
  SaveAutoOpens();
}

bool DownloadManager::ShouldOpenFileBasedOnExtension(
    const FilePath& path) const {
  FilePath::StringType extension = path.Extension();
  if (extension.empty())
    return false;
  if (IsExecutableExtension(extension))
    return false;
  if (Extension::IsExtension(path))
    return false;
  DCHECK(extension[0] == FilePath::kExtensionSeparator);
  extension.erase(0, 1);
  if (auto_open_.find(extension) != auto_open_.end())
    return true;
  return false;
}

static const char* kExecutableWhiteList[] = {
  // JavaScript is just as powerful as EXE.
  "text/javascript",
  "text/javascript;version=*",
  // Registry files can cause critical changes to the MS OS behavior.
  // Addition of this mimetype also addresses bug 7337.
  "text/x-registry",
  // Some sites use binary/octet-stream to mean application/octet-stream.
  // See http://code.google.com/p/chromium/issues/detail?id=1573
  "binary/octet-stream"
};

static const char* kExecutableBlackList[] = {
  // These application types are not executable.
  "application/*+xml",
  "application/xml"
};

// static
bool DownloadManager::IsExecutableMimeType(const std::string& mime_type) {
  for (size_t i = 0; i < arraysize(kExecutableWhiteList); ++i) {
    if (net::MatchesMimeType(kExecutableWhiteList[i], mime_type))
      return true;
  }
  for (size_t i = 0; i < arraysize(kExecutableBlackList); ++i) {
    if (net::MatchesMimeType(kExecutableBlackList[i], mime_type))
      return false;
  }
  // We consider only other application types to be executable.
  return net::MatchesMimeType("application/*", mime_type);
}

bool DownloadManager::IsExecutableFile(const FilePath& path) const {
  return IsExecutableExtension(path.Extension());
}

bool DownloadManager::IsExecutableExtension(
    const FilePath::StringType& extension) {
  if (extension.empty())
    return false;
  if (!IsStringASCII(extension))
    return false;
#if defined(OS_WIN)
  std::string ascii_extension = WideToASCII(extension);
#elif defined(OS_POSIX)
  std::string ascii_extension = extension;
#endif

  // Strip out leading dot if it's still there
  if (ascii_extension[0] == FilePath::kExtensionSeparator)
    ascii_extension.erase(0, 1);

  return download_util::IsExecutableExtension(ascii_extension);
}

void DownloadManager::ResetAutoOpenFiles() {
  auto_open_.clear();
  SaveAutoOpens();
}

bool DownloadManager::HasAutoOpenFileTypesRegistered() const {
  return !auto_open_.empty();
}

void DownloadManager::SaveAutoOpens() {
  PrefService* prefs = profile_->GetPrefs();
  if (prefs) {
    std::string extensions;
    for (AutoOpenSet::iterator it = auto_open_.begin();
         it != auto_open_.end(); ++it) {
#if defined(OS_POSIX)
      std::string this_extension = *it;
#elif defined(OS_WIN)
      std::string this_extension = base::SysWideToUTF8(*it);
#endif
      extensions += this_extension + ":";
    }
    if (!extensions.empty())
      extensions.erase(extensions.size() - 1);

    prefs->SetString(prefs::kDownloadExtensionsToOpen, extensions);
  }
}

void DownloadManager::FileSelected(const FilePath& path,
                                   int index, void* params) {
  DownloadCreateInfo* info = reinterpret_cast<DownloadCreateInfo*>(params);
  if (info->prompt_user_for_save_location)
    last_download_path_ = path.DirName();
  ContinueStartDownload(info, path);
}

void DownloadManager::FileSelectionCanceled(void* params) {
  // The user didn't pick a place to save the file, so need to cancel the
  // download that's already in progress to the temporary location.
  DownloadCreateInfo* info = reinterpret_cast<DownloadCreateInfo*>(params);
  DownloadCancelledInternal(info->download_id,
                            info->child_id,
                            info->request_id);
}

void DownloadManager::DeleteDownload(const FilePath& path) {
  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableFunction(&DeleteDownloadedFile, path));
}

void DownloadManager::DangerousDownloadValidated(DownloadItem* download) {
  DCHECK_EQ(DownloadItem::DANGEROUS, download->safety_state());
  download->set_safety_state(DownloadItem::DANGEROUS_BUT_VALIDATED);
  download->UpdateObservers();

  // If the download is not complete, nothing to do.  The required
  // post-processing will be performed when it does complete.
  if (download->state() != DownloadItem::COMPLETE)
    return;

  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(
          this, &DownloadManager::ProceedWithFinishedDangerousDownload,
          download->db_handle(), download->full_path(),
          download->original_name()));
}

void DownloadManager::GenerateSafeFileName(const std::string& mime_type,
                                           FilePath* file_name) {
  // Make sure we get the right file extension
  FilePath::StringType extension;
  GenerateExtension(*file_name, mime_type, &extension);
  *file_name = file_name->ReplaceExtension(extension);

#if defined(OS_WIN)
  // Prepend "_" to the file name if it's a reserved name
  FilePath::StringType leaf_name = file_name->BaseName().value();
  DCHECK(!leaf_name.empty());
  if (win_util::IsReservedName(leaf_name)) {
    leaf_name = FilePath::StringType(FILE_PATH_LITERAL("_")) + leaf_name;
    *file_name = file_name->DirName();
    if (file_name->value() == FilePath::kCurrentDirectory) {
      *file_name = FilePath(leaf_name);
    } else {
      *file_name = file_name->Append(leaf_name);
    }
  }
#endif
}

// Operations posted to us from the history service ----------------------------

// The history service has retrieved all download entries. 'entries' contains
// 'DownloadCreateInfo's in sorted order (by ascending start_time).
void DownloadManager::OnQueryDownloadEntriesComplete(
    std::vector<DownloadCreateInfo>* entries) {
  for (size_t i = 0; i < entries->size(); ++i) {
    DownloadItem* download = new DownloadItem(entries->at(i));
    DCHECK(downloads_.find(download->db_handle()) == downloads_.end());
    downloads_[download->db_handle()] = download;
    download->set_manager(this);
  }
  NotifyModelChanged();
}

// Once the new DownloadItem's creation info has been committed to the history
// service, we associate the DownloadItem with the db handle, update our
// 'downloads_' map and inform observers.
void DownloadManager::OnCreateDownloadEntryComplete(DownloadCreateInfo info,
                                                    int64 db_handle) {
  DownloadMap::iterator it = in_progress_.find(info.download_id);
  DCHECK(it != in_progress_.end());

  DownloadItem* download = it->second;

  // It's not immediately obvious, but HistoryBackend::CreateDownload() can
  // call this function with an invalid |db_handle|. For instance, this can
  // happen when the history database is offline. We cannot have multiple
  // DownloadItems with the same invalid db_handle, so we need to assign a
  // unique |db_handle| here.
  if (db_handle == kUninitializedHandle)
    db_handle = fake_db_handle_.GetNext();

  DCHECK(download->db_handle() == kUninitializedHandle);
  download->set_db_handle(db_handle);

  // Insert into our full map.
  DCHECK(downloads_.find(download->db_handle()) == downloads_.end());
  downloads_[download->db_handle()] = download;

  // Show in the appropropriate browser UI.
  ShowDownloadInBrowser(info, download);

  // Inform interested objects about the new download.
  NotifyModelChanged();

  // If this download has been completed before we've received the db handle,
  // post one final message to the history service so that it can be properly
  // in sync with the DownloadItem's completion status, and also inform any
  // observers so that they get more than just the start notification.
  if (download->state() != DownloadItem::IN_PROGRESS) {
    in_progress_.erase(it);
    UpdateHistoryForDownload(download);
    download->UpdateObservers();
  }

  UpdateAppIcon();
}

// Called when the history service has retrieved the list of downloads that
// match the search text.
void DownloadManager::OnSearchComplete(HistoryService::Handle handle,
                                       std::vector<int64>* results) {
  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  Observer* requestor = cancelable_consumer_.GetClientData(hs, handle);
  if (!requestor)
    return;

  std::vector<DownloadItem*> searched_downloads;
  for (std::vector<int64>::iterator it = results->begin();
       it != results->end(); ++it) {
    DownloadMap::iterator dit = downloads_.find(*it);
    if (dit != downloads_.end())
      searched_downloads.push_back(dit->second);
  }

  requestor->SetDownloads(searched_downloads);
}

void DownloadManager::ShowDownloadInBrowser(const DownloadCreateInfo& info,
                                            DownloadItem* download) {
  // The 'contents' may no longer exist if the user closed the tab before we
  // get this start completion event. If it does, tell the origin TabContents
  // to display its download shelf.
  TabContents* contents = tab_util::GetTabContentsByID(info.child_id,
                                                       info.render_view_id);

  // If the contents no longer exists, we start the download in the last active
  // browser. This is not ideal but better than fully hiding the download from
  // the user.
  if (!contents) {
    Browser* last_active = BrowserList::GetLastActive();
    if (last_active)
      contents = last_active->GetSelectedTabContents();
  }

  if (contents)
    contents->OnStartDownload(download);
}

// Clears the last download path, used to initialize "save as" dialogs.
void DownloadManager::ClearLastDownloadPath() {
  last_download_path_ = FilePath();
}

void DownloadManager::NotifyModelChanged() {
  FOR_EACH_OBSERVER(Observer, observers_, ModelChanged());
}

// DownloadManager::OtherDownloadManagerObserver implementation ----------------

DownloadManager::OtherDownloadManagerObserver::OtherDownloadManagerObserver(
    DownloadManager* observing_download_manager)
    : observing_download_manager_(observing_download_manager),
      observed_download_manager_(NULL) {
  if (observing_download_manager->profile_->GetOriginalProfile() ==
      observing_download_manager->profile_) {
    return;
  }

  observed_download_manager_ = observing_download_manager_->
      profile_->GetOriginalProfile()->GetDownloadManager();
  observed_download_manager_->AddObserver(this);
}

DownloadManager::OtherDownloadManagerObserver::~OtherDownloadManagerObserver() {
  if (observed_download_manager_)
    observed_download_manager_->RemoveObserver(this);
}

void DownloadManager::OtherDownloadManagerObserver::ModelChanged() {
  observing_download_manager_->NotifyModelChanged();
}

void DownloadManager::OtherDownloadManagerObserver::SetDownloads(
    std::vector<DownloadItem*>& downloads) {
}

void DownloadManager::OtherDownloadManagerObserver::ManagerGoingDown() {
  observed_download_manager_ = NULL;
}
