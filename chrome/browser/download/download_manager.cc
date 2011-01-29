// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_manager.h"

#include "base/callback.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/stl_util-inl.h"
#include "base/stringprintf.h"
#include "base/sys_string_conversions.h"
#include "base/task.h"
#include "base/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/download/download_extensions.h"
#include "chrome/browser/download/download_file_manager.h"
#include "chrome/browser/download/download_history.h"
#include "chrome/browser/download/download_item.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/download/download_status_updater.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/history/download_create_info.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/notification_type.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "net/base/mime_util.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

DownloadManager::DownloadManager(DownloadStatusUpdater* status_updater)
    : shutdown_needed_(false),
      profile_(NULL),
      file_manager_(NULL),
      status_updater_(status_updater->AsWeakPtr()) {
  if (status_updater_)
    status_updater_->AddDelegate(this);
}

DownloadManager::~DownloadManager() {
  DCHECK(!shutdown_needed_);
  if (status_updater_)
    status_updater_->RemoveDelegate(this);
}

void DownloadManager::Shutdown() {
  VLOG(20) << __FUNCTION__ << "()"
           << " shutdown_needed_ = " << shutdown_needed_;
  if (!shutdown_needed_)
    return;
  shutdown_needed_ = false;

  FOR_EACH_OBSERVER(Observer, observers_, ManagerGoingDown());

  if (file_manager_) {
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
        NewRunnableMethod(file_manager_,
                          &DownloadFileManager::OnDownloadManagerShutdown,
                          make_scoped_refptr(this)));
  }

  AssertContainersConsistent();

  // Go through all downloads in downloads_.  Dangerous ones we need to
  // remove on disk, and in progress ones we need to cancel.
  for (DownloadSet::iterator it = downloads_.begin(); it != downloads_.end();) {
    DownloadItem* download = *it;

    // Save iterator from potential erases in this set done by called code.
    // Iterators after an erasure point are still valid for lists and
    // associative containers such as sets.
    it++;

    if (download->safety_state() == DownloadItem::DANGEROUS &&
        (download->state() == DownloadItem::IN_PROGRESS ||
         download->state() == DownloadItem::COMPLETE)) {
      // The user hasn't accepted it, so we need to remove it
      // from the disk.  This may or may not result in it being
      // removed from the DownloadManager queues and deleted
      // (specifically, DownloadManager::RemoveDownload only
      // removes and deletes it if it's known to the history service)
      // so the only thing we know after calling this function is that
      // the download was deleted if-and-only-if it was removed
      // from all queues.
      download->Remove(true);
    } else if (download->state() == DownloadItem::IN_PROGRESS) {
      download->Cancel(false);
      download_history_->UpdateEntry(download);
    }
  }

  // At this point, all dangerous downloads have had their files removed
  // and all in progress downloads have been cancelled.  We can now delete
  // anything left.
  STLDeleteElements(&downloads_);

  // And clear all non-owning containers.
  in_progress_.clear();
  active_downloads_.clear();
#if !defined(NDEBUG)
  save_page_as_downloads_.clear();
#endif

  file_manager_ = NULL;

  // Make sure the save as dialog doesn't notify us back if we're gone before
  // it returns.
  if (select_file_dialog_.get())
    select_file_dialog_->ListenerDestroyed();

  download_history_.reset();
  download_prefs_.reset();

  request_context_getter_ = NULL;

  shutdown_needed_ = false;
}

void DownloadManager::GetTemporaryDownloads(
    const FilePath& dir_path, std::vector<DownloadItem*>* result) {
  DCHECK(result);

  for (DownloadMap::iterator it = history_downloads_.begin();
       it != history_downloads_.end(); ++it) {
    if (it->second->is_temporary() &&
        it->second->full_path().DirName() == dir_path)
      result->push_back(it->second);
  }
}

void DownloadManager::GetAllDownloads(
    const FilePath& dir_path, std::vector<DownloadItem*>* result) {
  DCHECK(result);

  for (DownloadMap::iterator it = history_downloads_.begin();
       it != history_downloads_.end(); ++it) {
    if (!it->second->is_temporary() &&
        (dir_path.empty() || it->second->full_path().DirName() == dir_path))
      result->push_back(it->second);
  }
}

void DownloadManager::GetCurrentDownloads(
    const FilePath& dir_path, std::vector<DownloadItem*>* result) {
  DCHECK(result);

  for (DownloadMap::iterator it = history_downloads_.begin();
       it != history_downloads_.end(); ++it) {
    if (!it->second->is_temporary() &&
        (it->second->state() == DownloadItem::IN_PROGRESS ||
         it->second->safety_state() == DownloadItem::DANGEROUS) &&
        (dir_path.empty() || it->second->full_path().DirName() == dir_path))
      result->push_back(it->second);
  }

  // If we have a parent profile, let it add its downloads to the results.
  Profile* original_profile = profile_->GetOriginalProfile();
  if (original_profile != profile_)
    original_profile->GetDownloadManager()->GetCurrentDownloads(dir_path,
                                                                result);
}

void DownloadManager::SearchDownloads(const string16& query,
                                      std::vector<DownloadItem*>* result) {
  DCHECK(result);

  string16 query_lower(l10n_util::ToLower(query));

  for (DownloadMap::iterator it = history_downloads_.begin();
       it != history_downloads_.end(); ++it) {
    DownloadItem* download_item = it->second;

    if (download_item->is_temporary() || download_item->is_extension_install())
      continue;

    // Display Incognito downloads only in Incognito window, and vice versa.
    // The Incognito Downloads page will get the list of non-Incognito downloads
    // from its parent profile.
    if (profile_->IsOffTheRecord() != download_item->is_otr())
      continue;

    if (download_item->MatchesQuery(query_lower))
      result->push_back(download_item);
  }

  // If we have a parent profile, let it add its downloads to the results.
  Profile* original_profile = profile_->GetOriginalProfile();
  if (original_profile != profile_)
    original_profile->GetDownloadManager()->SearchDownloads(query, result);
}

// Query the history service for information about all persisted downloads.
bool DownloadManager::Init(Profile* profile) {
  DCHECK(profile);
  DCHECK(!shutdown_needed_)  << "DownloadManager already initialized.";
  shutdown_needed_ = true;

  profile_ = profile;
  request_context_getter_ = profile_->GetRequestContext();
  download_history_.reset(new DownloadHistory(profile));
  download_history_->Load(
      NewCallback(this, &DownloadManager::OnQueryDownloadEntriesComplete));

  download_prefs_.reset(new DownloadPrefs(profile_->GetPrefs()));

  // In test mode, there may be no ResourceDispatcherHost.  In this case it's
  // safe to avoid setting |file_manager_| because we only call a small set of
  // functions, none of which need it.
  ResourceDispatcherHost* rdh = g_browser_process->resource_dispatcher_host();
  if (rdh) {
    file_manager_ = rdh->download_file_manager();
    DCHECK(file_manager_);
  }

  other_download_manager_observer_.reset(
      new OtherDownloadManagerObserver(this));

  return true;
}

// We have received a message from DownloadFileManager about a new download. We
// create a download item and store it in our download map, and inform the
// history system of a new download. Since this method can be called while the
// history service thread is still reading the persistent state, we do not
// insert the new DownloadItem into 'history_downloads_' or inform our
// observers at this point. OnCreateDownloadEntryComplete() handles that
// finalization of the the download creation as a callback from the
// history thread.
void DownloadManager::StartDownload(DownloadCreateInfo* info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
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
    download_util::GenerateFileNameFromInfo(info, &generated_name);

    // Freeze the user's preference for showing a Save As dialog.  We're going
    // to bounce around a bunch of threads and we don't want to worry about race
    // conditions where the user changes this pref out from under us.
    if (download_prefs_->prompt_for_download()) {
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
    if (info->prompt_user_for_save_location && !last_download_path_.empty()) {
      info->suggested_path = last_download_path_;
    } else {
      info->suggested_path = download_prefs_->download_path();
    }
    info->suggested_path = info->suggested_path.Append(generated_name);
  } else {
    info->suggested_path = info->save_info.file_path;
  }

  if (!info->prompt_user_for_save_location &&
      info->save_info.file_path.empty()) {
    info->is_dangerous = download_util::IsDangerous(
        info, profile(), ShouldOpenFileBasedOnExtension(info->suggested_path));
  }

  // We need to move over to the download thread because we don't want to stat
  // the suggested path on the UI thread.
  // We can only access preferences on the UI thread, so check the download path
  // now and pass the value to the FILE thread.
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(
          this,
          &DownloadManager::CheckIfSuggestedPathExists,
          info,
          download_prefs()->download_path()));
}

void DownloadManager::CheckIfSuggestedPathExists(DownloadCreateInfo* info,
                                                 const FilePath& default_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(info);

  // Make sure the default download directory exists.
  // TODO(phajdan.jr): only create the directory when we're sure the user
  // is going to save there and not to another directory of his choice.
  file_util::CreateDirectory(default_path);

  // Check writability of the suggested path. If we can't write to it, default
  // to the user's "My Documents" directory. We'll prompt them in this case.
  FilePath dir = info->suggested_path.DirName();
  FilePath filename = info->suggested_path.BaseName();
  if (!file_util::PathIsWritable(dir)) {
    VLOG(1) << "Unable to write to directory \"" << dir.value() << "\"";
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
      VLOG(1) << "Unable to find a unique path for suggested path \""
                   << info->suggested_path.value() << "\"";
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

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(this,
                        &DownloadManager::OnPathExistenceAvailable,
                        info));
}

void DownloadManager::OnPathExistenceAvailable(DownloadCreateInfo* info) {
  VLOG(20) << __FUNCTION__ << "()" << " info = " << info->DebugString();
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
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
    FOR_EACH_OBSERVER(Observer, observers_, SelectFileDialogDisplayed());
  } else {
    // No prompting for download, just continue with the suggested name.
    AttachDownloadItem(info, info->suggested_path);
  }
}

void DownloadManager::CreateDownloadItem(DownloadCreateInfo* info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DownloadItem* download = new DownloadItem(this, *info,
                                            profile_->IsOffTheRecord());
  DCHECK(!ContainsKey(in_progress_, info->download_id));
  DCHECK(!ContainsKey(active_downloads_, info->download_id));
  downloads_.insert(download);
  active_downloads_[info->download_id] = download;
}

void DownloadManager::AttachDownloadItem(DownloadCreateInfo* info,
                                         const FilePath& target_path) {
  VLOG(20) << __FUNCTION__ << "()" << " info = " << info->DebugString();

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  scoped_ptr<DownloadCreateInfo> infop(info);
  info->path = target_path;

  // NOTE(ahendrickson) Eventually |active_downloads_| will replace
  // |in_progress_|, but we don't want to change the semantics yet.
  DCHECK(!ContainsKey(in_progress_, info->download_id));
  DCHECK(ContainsKey(active_downloads_, info->download_id));
  DownloadItem* download = active_downloads_[info->download_id];
  DCHECK(download != NULL);
  DCHECK(ContainsKey(downloads_, download));

  download->SetFileCheckResults(info->path,
                                info->is_dangerous,
                                info->path_uniquifier,
                                info->prompt_user_for_save_location,
                                info->is_extension_install,
                                info->original_name);
  in_progress_[info->download_id] = download;
  UpdateAppIcon();  // Reflect entry into in_progress_.

  // Rename to intermediate name.
  if (info->is_dangerous) {
    // The download is not safe.  We can now rename the file to its
    // tentative name using OnFinalDownloadName (the actual final
    // name after user confirmation will be set in
    // ProceedWithFinishedDangerousDownload).
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        NewRunnableMethod(
            file_manager_, &DownloadFileManager::OnFinalDownloadName,
            download->id(), target_path, false,
            make_scoped_refptr(this)));
  } else {
    // The download is a safe download.  We need to
    // rename it to its intermediate '.crdownload' path.  The final
    // name after user confirmation will be set from
    // DownloadItem::OnSafeDownloadFinished.
    FilePath download_path = download_util::GetCrDownloadPath(target_path);
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        NewRunnableMethod(
            file_manager_, &DownloadFileManager::OnIntermediateDownloadName,
            download->id(), download_path, make_scoped_refptr(this)));
    download->Rename(download_path);
  }

  download_history_->AddEntry(*info, download,
      NewCallback(this, &DownloadManager::OnCreateDownloadEntryComplete));
}

void DownloadManager::UpdateDownload(int32 download_id, int64 size) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DownloadMap::iterator it = active_downloads_.find(download_id);
  if (it != active_downloads_.end()) {
    DownloadItem* download = it->second;
    if (download->state() == DownloadItem::IN_PROGRESS) {
      download->Update(size);
      UpdateAppIcon();  // Reflect size updates.
      download_history_->UpdateEntry(download);
    }
  }
}

void DownloadManager::OnAllDataSaved(int32 download_id, int64 size) {
  VLOG(20) << __FUNCTION__ << "()" << " download_id = " << download_id
           << " size = " << size;

  // If it's not in active_downloads_, that means it was cancelled; just
  // ignore the notification.
  if (active_downloads_.count(download_id) == 0)
    return;

  DownloadItem* download = active_downloads_[download_id];
  download->OnAllDataSaved(size);

  MaybeCompleteDownload(download);
}

bool DownloadManager::IsDownloadReadyForCompletion(DownloadItem* download) {
  // If we don't have all the data, the download is not ready for
  // completion.
  if (!download->all_data_saved())
    return false;

  // If the download isn't active (e.g. has been cancelled) it's not
  // ready for completion.
  if (active_downloads_.count(download->id()) == 0)
    return false;

  // If the download hasn't been inserted into the history system
  // (which occurs strictly after file name determination, intermediate
  // file rename, and UI display) then it's not ready for completion.
  return (download->db_handle() != DownloadHistory::kUninitializedHandle);
}

void DownloadManager::MaybeCompleteDownload(DownloadItem* download) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  VLOG(20) << __FUNCTION__ << "()" << " download = "
           << download->DebugString(false);

  if (!IsDownloadReadyForCompletion(download))
    return;

  // TODO(rdsmith): DCHECK that we only pass through this point
  // once per download.  The natural way to do this is by a state
  // transition on the DownloadItem.

  // Confirm we're in the proper set of states to be here;
  // in in_progress_, have all data, have a history handle.
  DCHECK_EQ(1u, in_progress_.count(download->id()));
  DCHECK(download->all_data_saved());
  DCHECK(download->db_handle() != DownloadHistory::kUninitializedHandle);
  DCHECK_EQ(1u, history_downloads_.count(download->db_handle()));

  VLOG(20) << __FUNCTION__ << "()" << " executing: download = "
           << download->DebugString(false);

  // Remove the id from in_progress
  in_progress_.erase(download->id());
  UpdateAppIcon();  // Reflect removal from in_progress_.

  // Final update of download item and history.
  download->MarkAsComplete();
  download_history_->UpdateEntry(download);

  switch (download->safety_state()) {
    case DownloadItem::DANGEROUS:
      // If this a dangerous download not yet validated by the user, don't do
      // anything. When the user notifies us, it will trigger a call to
      // ProceedWithFinishedDangerousDownload.
      return;
    case DownloadItem::DANGEROUS_BUT_VALIDATED:
      // The dangerous download has been validated by the user.  We first
      // need to rename the downloaded file from its temporary name to
      // its final name.  We will continue the download processing in the
      // callback.
      BrowserThread::PostTask(
          BrowserThread::FILE, FROM_HERE,
          NewRunnableMethod(
              this, &DownloadManager::ProceedWithFinishedDangerousDownload,
              download->db_handle(),
              download->full_path(), download->target_name()));
      return;
    case DownloadItem::SAFE:
      // The download is safe; just finish it.
      download->OnSafeDownloadFinished(file_manager_);
      return;
  }
}

void DownloadManager::RemoveFromActiveList(int32 download_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  active_downloads_.erase(download_id);
}

void DownloadManager::DownloadRenamedToFinalName(int download_id,
                                                 const FilePath& full_path) {
  VLOG(20) << __FUNCTION__ << "()" << " download_id = " << download_id
           << " full_path = \"" << full_path.value() << "\"";
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DownloadItem* item = GetDownloadItem(download_id);
  if (!item)
    return;
  item->OnDownloadRenamedToFinalName(full_path);
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

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(this, &DownloadManager::DangerousDownloadRenamed,
                        download_handle, success, new_path, uniquifier));
}

// Call from the file thread when the finished dangerous download was renamed.
void DownloadManager::DangerousDownloadRenamed(int64 download_handle,
                                               bool success,
                                               const FilePath& new_path,
                                               int new_path_uniquifier) {
  VLOG(20) << __FUNCTION__ << "()" << " download_handle = " << download_handle
           << " success = " << success
           << " new_path = \"" << new_path.value() << "\""
           << " new_path_uniquifier = " << new_path_uniquifier;
  DownloadMap::iterator it = history_downloads_.find(download_handle);
  if (it == history_downloads_.end()) {
    NOTREACHED();
    return;
  }

  DownloadItem* download = it->second;
  // If we failed to rename the file, we'll just keep the name as is.
  if (success) {
    // We need to update the path uniquifier so that the UI shows the right
    // name when calling GetFileNameToReportUser().
    download->set_path_uniquifier(new_path_uniquifier);
    RenameDownload(download, new_path);
  }

  // Continue the download finished sequence.
  download->Finished();
}

void DownloadManager::DownloadCancelled(int32 download_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DownloadMap::iterator it = in_progress_.find(download_id);
  if (it == in_progress_.end())
    return;
  DownloadItem* download = it->second;

  VLOG(20) << __FUNCTION__ << "()" << " download_id = " << download_id
           << " download = " << download->DebugString(true);

  // Clean up will happen when the history system create callback runs if we
  // don't have a valid db_handle yet.
  if (download->db_handle() != DownloadHistory::kUninitializedHandle) {
    in_progress_.erase(it);
    active_downloads_.erase(download_id);
    UpdateAppIcon();  // Reflect removal from in_progress_.
    download_history_->UpdateEntry(download);
  }

  DownloadCancelledInternal(download_id,
                            download->render_process_id(),
                            download->request_id());
}

void DownloadManager::DownloadCancelledInternal(int download_id,
                                                int render_process_id,
                                                int request_id) {
  // Cancel the network request.  RDH is guaranteed to outlive the IO thread.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableFunction(&download_util::CancelDownloadRequest,
                          g_browser_process->resource_dispatcher_host(),
                          render_process_id,
                          request_id));

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
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

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(this,
                        &DownloadManager::PauseDownloadRequest,
                        g_browser_process->resource_dispatcher_host(),
                        download->render_process_id(),
                        download->request_id(),
                        pause));
}

void DownloadManager::UpdateAppIcon() {
  if (status_updater_)
    status_updater_->Update();
}

void DownloadManager::RenameDownload(DownloadItem* download,
                                     const FilePath& new_path) {
  download->Rename(new_path);
  download_history_->UpdateDownloadPath(download, new_path);
}

void DownloadManager::PauseDownloadRequest(ResourceDispatcherHost* rdh,
                                           int render_process_id,
                                           int request_id,
                                           bool pause) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  rdh->PauseRequest(render_process_id, request_id, pause);
}

void DownloadManager::RemoveDownload(int64 download_handle) {
  DownloadMap::iterator it = history_downloads_.find(download_handle);
  if (it == history_downloads_.end())
    return;

  // Make history update.
  DownloadItem* download = it->second;
  download_history_->RemoveEntry(download);

  // Remove from our tables and delete.
  history_downloads_.erase(it);
  int downloads_count = downloads_.erase(download);
  DCHECK_EQ(1, downloads_count);

  // Tell observers to refresh their views.
  NotifyModelChanged();

  delete download;
}

int DownloadManager::RemoveDownloadsBetween(const base::Time remove_begin,
                                            const base::Time remove_end) {
  download_history_->RemoveEntriesBetween(remove_begin, remove_end);

  // All downloads visible to the user will be in the history,
  // so scan that map.
  DownloadMap::iterator it = history_downloads_.begin();
  std::vector<DownloadItem*> pending_deletes;
  while (it != history_downloads_.end()) {
    DownloadItem* download = it->second;
    DownloadItem::DownloadState state = download->state();
    if (download->start_time() >= remove_begin &&
        (remove_end.is_null() || download->start_time() < remove_end) &&
        (state == DownloadItem::COMPLETE ||
         state == DownloadItem::CANCELLED)) {
      // Remove from the map and move to the next in the list.
      history_downloads_.erase(it++);

      // Also remove it from any completed dangerous downloads.
      pending_deletes.push_back(download);

      continue;
    }

    ++it;
  }

  // If we aren't deleting anything, we're done.
  if (pending_deletes.empty())
    return 0;

  // Remove the chosen downloads from the main owning container.
  for (std::vector<DownloadItem*>::iterator it = pending_deletes.begin();
       it != pending_deletes.end(); it++) {
    downloads_.erase(*it);
  }

  // Tell observers to refresh their views.
  NotifyModelChanged();

  // Delete the download items themselves.
  int num_deleted = static_cast<int>(pending_deletes.size());

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

void DownloadManager::SavePageAsDownloadStarted(DownloadItem* download_item) {
#if !defined(NDEBUG)
  save_page_as_downloads_.insert(download_item);
#endif
  downloads_.insert(download_item);
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
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
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

void DownloadManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
  observer->ModelChanged();
}

void DownloadManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool DownloadManager::ShouldOpenFileBasedOnExtension(
    const FilePath& path) const {
  FilePath::StringType extension = path.Extension();
  if (extension.empty())
    return false;
  if (Extension::IsExtension(path))
    return false;
  DCHECK(extension[0] == FilePath::kExtensionSeparator);
  extension.erase(0, 1);
  return download_prefs_->IsAutoOpenEnabledForExtension(extension);
}

bool DownloadManager::IsDownloadProgressKnown() {
  for (DownloadMap::iterator i = in_progress_.begin();
       i != in_progress_.end(); ++i) {
    if (i->second->total_bytes() <= 0)
      return false;
  }

  return true;
}

int64 DownloadManager::GetInProgressDownloadCount() {
  return in_progress_.size();
}

int64 DownloadManager::GetReceivedDownloadBytes() {
  DCHECK(IsDownloadProgressKnown());
  int64 received_bytes = 0;
  for (DownloadMap::iterator i = in_progress_.begin();
       i != in_progress_.end(); ++i) {
    received_bytes += i->second->received_bytes();
  }
  return received_bytes;
}

int64 DownloadManager::GetTotalDownloadBytes() {
  DCHECK(IsDownloadProgressKnown());
  int64 total_bytes = 0;
  for (DownloadMap::iterator i = in_progress_.begin();
       i != in_progress_.end(); ++i) {
    total_bytes += i->second->total_bytes();
  }
  return total_bytes;
}

void DownloadManager::FileSelected(const FilePath& path,
                                   int index, void* params) {
  DownloadCreateInfo* info = reinterpret_cast<DownloadCreateInfo*>(params);
  if (info->prompt_user_for_save_location)
    last_download_path_ = path.DirName();
  AttachDownloadItem(info, path);
}

void DownloadManager::FileSelectionCanceled(void* params) {
  // The user didn't pick a place to save the file, so need to cancel the
  // download that's already in progress to the temporary location.
  DownloadCreateInfo* info = reinterpret_cast<DownloadCreateInfo*>(params);
  DownloadCancelledInternal(info->download_id,
                            info->child_id,
                            info->request_id);
}

void DownloadManager::DangerousDownloadValidated(DownloadItem* download) {
  DCHECK_EQ(DownloadItem::DANGEROUS, download->safety_state());
  download->set_safety_state(DownloadItem::DANGEROUS_BUT_VALIDATED);
  download->UpdateObservers();

  // If the download is not complete, nothing to do.  The required
  // post-processing will be performed when it does complete.
  if (download->state() != DownloadItem::COMPLETE)
    return;

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(
          this, &DownloadManager::ProceedWithFinishedDangerousDownload,
          download->db_handle(), download->full_path(),
          download->target_name()));
}

// Operations posted to us from the history service ----------------------------

// The history service has retrieved all download entries. 'entries' contains
// 'DownloadCreateInfo's in sorted order (by ascending start_time).
void DownloadManager::OnQueryDownloadEntriesComplete(
    std::vector<DownloadCreateInfo>* entries) {
  for (size_t i = 0; i < entries->size(); ++i) {
    DownloadItem* download = new DownloadItem(this, entries->at(i));
    DCHECK(!ContainsKey(history_downloads_, download->db_handle()));
    downloads_.insert(download);
    history_downloads_[download->db_handle()] = download;
    VLOG(20) << __FUNCTION__ << "()" << i << ">"
             << " download = " << download->DebugString(true);
  }
  NotifyModelChanged();
}

// Once the new DownloadItem's creation info has been committed to the history
// service, we associate the DownloadItem with the db handle, update our
// 'history_downloads_' map and inform observers.
void DownloadManager::OnCreateDownloadEntryComplete(
    DownloadCreateInfo info,
    int64 db_handle) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DownloadMap::iterator it = in_progress_.find(info.download_id);
  DCHECK(it != in_progress_.end());

  DownloadItem* download = it->second;
  VLOG(20) << __FUNCTION__ << "()" << " db_handle = " << db_handle
           << " download_id = " << info.download_id
           << " download = " << download->DebugString(true);

  // It's not immediately obvious, but HistoryBackend::CreateDownload() can
  // call this function with an invalid |db_handle|. For instance, this can
  // happen when the history database is offline. We cannot have multiple
  // DownloadItems with the same invalid db_handle, so we need to assign a
  // unique |db_handle| here.
  if (db_handle == DownloadHistory::kUninitializedHandle)
    db_handle = download_history_->GetNextFakeDbHandle();

  DCHECK(download->db_handle() == DownloadHistory::kUninitializedHandle);
  download->set_db_handle(db_handle);

  // Insert into our full map.
  DCHECK(!ContainsKey(history_downloads_, download->db_handle()));
  history_downloads_[download->db_handle()] = download;

  // Show in the appropriate browser UI.
  ShowDownloadInBrowser(info, download);

  // Inform interested objects about the new download.
  NotifyModelChanged();

  // If this download has been cancelled before we've received the DB handle,
  // post one final message to the history service so that it can be properly
  // in sync with the DownloadItem's completion status, and also inform any
  // observers so that they get more than just the start notification.
  //
  // Otherwise, try to complete the download
  if (download->state() != DownloadItem::IN_PROGRESS) {
    DCHECK_EQ(DownloadItem::CANCELLED, download->state());
    in_progress_.erase(it);
    active_downloads_.erase(info.download_id);
    download_history_->UpdateEntry(download);
    download->UpdateObservers();
  } else {
    MaybeCompleteDownload(download);
  }
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

DownloadItem* DownloadManager::GetDownloadItem(int id) {
  for (DownloadMap::iterator it = history_downloads_.begin();
       it != history_downloads_.end(); ++it) {
    DownloadItem* item = it->second;
    if (item->id() == id)
      return item;
  }
  return NULL;
}

// Confirm that everything in all maps is also in |downloads_|, and that
// everything in |downloads_| is also in some other map.
void DownloadManager::AssertContainersConsistent() const {
#if !defined(NDEBUG)
  // Turn everything into sets.
  DownloadSet active_set, history_set;
  const DownloadMap* input_maps[] = {&active_downloads_, &history_downloads_};
  DownloadSet* local_sets[] = {&active_set, &history_set};
  DCHECK_EQ(ARRAYSIZE_UNSAFE(input_maps), ARRAYSIZE_UNSAFE(local_sets));
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(input_maps); i++) {
    for (DownloadMap::const_iterator it = input_maps[i]->begin();
         it != input_maps[i]->end(); it++) {
      local_sets[i]->insert(&*it->second);
    }
  }

  // Check if each set is fully present in downloads, and create a union.
  const DownloadSet* all_sets[] = {&active_set, &history_set,
                                   &save_page_as_downloads_};
  DownloadSet downloads_union;
  for (int i = 0; i < static_cast<int>(ARRAYSIZE_UNSAFE(all_sets)); i++) {
    DownloadSet remainder;
    std::insert_iterator<DownloadSet> insert_it(remainder, remainder.begin());
    std::set_difference(all_sets[i]->begin(), all_sets[i]->end(),
                        downloads_.begin(), downloads_.end(),
                        insert_it);
    DCHECK(remainder.empty());
    std::insert_iterator<DownloadSet>
        insert_union(downloads_union, downloads_union.end());
    std::set_union(downloads_union.begin(), downloads_union.end(),
                   all_sets[i]->begin(), all_sets[i]->end(),
                   insert_union);
  }

  // Is everything in downloads_ present in one of the other sets?
  DownloadSet remainder;
  std::insert_iterator<DownloadSet>
      insert_remainder(remainder, remainder.begin());
  std::set_difference(downloads_.begin(), downloads_.end(),
                      downloads_union.begin(), downloads_union.end(),
                      insert_remainder);
  DCHECK(remainder.empty());
#endif
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

void DownloadManager::OtherDownloadManagerObserver::ManagerGoingDown() {
  observed_download_manager_ = NULL;
}
