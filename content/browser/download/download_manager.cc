// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_manager.h"

#include <iterator>

#include "base/callback.h"
#include "base/file_util.h"
#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/task.h"
#include "build/build_config.h"
#include "content/browser/browser_context.h"
#include "content/browser/browser_thread.h"
#include "content/browser/content_browser_client.h"
#include "content/browser/download/download_create_info.h"
#include "content/browser/download/download_file_manager.h"
#include "content/browser/download/download_item.h"
#include "content/browser/download/download_manager_delegate.h"
#include "content/browser/download/download_persistent_store_info.h"
#include "content/browser/download/download_status_updater.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/content_notification_types.h"
#include "content/common/notification_service.h"

namespace {

void BeginDownload(
    const GURL& url,
    const GURL& referrer,
    const DownloadSaveInfo& save_info,
    ResourceDispatcherHost* resource_dispatcher_host,
    int render_process_id,
    int render_view_id,
    const content::ResourceContext* context) {
  resource_dispatcher_host->BeginDownload(
      url, referrer, save_info, true, render_process_id, render_view_id,
      *context);
}

}  // namespace

DownloadManager::DownloadManager(DownloadManagerDelegate* delegate,
                                 DownloadStatusUpdater* status_updater)
    : shutdown_needed_(false),
      browser_context_(NULL),
      file_manager_(NULL),
      status_updater_(status_updater->AsWeakPtr()),
      delegate_(delegate),
      largest_db_handle_in_history_(DownloadItem::kUninitializedHandle) {
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
        download->IsPartialDownload()) {
      // The user hasn't accepted it, so we need to remove it
      // from the disk.  This may or may not result in it being
      // removed from the DownloadManager queues and deleted
      // (specifically, DownloadManager::RemoveDownload only
      // removes and deletes it if it's known to the history service)
      // so the only thing we know after calling this function is that
      // the download was deleted if-and-only-if it was removed
      // from all queues.
      download->Delete(DownloadItem::DELETE_DUE_TO_BROWSER_SHUTDOWN);
    } else if (download->IsPartialDownload()) {
      download->Cancel(false);
      delegate_->UpdateItemInPersistentStore(download);
    }
  }

  // At this point, all dangerous downloads have had their files removed
  // and all in progress downloads have been cancelled.  We can now delete
  // anything left.

  // Copy downloads_ to separate container so as not to set off checks
  // in DownloadItem destruction.
  DownloadSet downloads_to_delete;
  downloads_to_delete.swap(downloads_);

  in_progress_.clear();
  active_downloads_.clear();
  history_downloads_.clear();
  STLDeleteElements(&downloads_to_delete);

  DCHECK(save_page_downloads_.empty());

  file_manager_ = NULL;
  delegate_->Shutdown();

  shutdown_needed_ = false;
}

void DownloadManager::GetTemporaryDownloads(
    const FilePath& dir_path, DownloadVector* result) {
  DCHECK(result);

  for (DownloadMap::iterator it = history_downloads_.begin();
       it != history_downloads_.end(); ++it) {
    if (it->second->is_temporary() &&
        it->second->full_path().DirName() == dir_path)
      result->push_back(it->second);
  }
}

void DownloadManager::GetAllDownloads(
    const FilePath& dir_path, DownloadVector* result) {
  DCHECK(result);

  for (DownloadMap::iterator it = history_downloads_.begin();
       it != history_downloads_.end(); ++it) {
    if (!it->second->is_temporary() &&
        (dir_path.empty() || it->second->full_path().DirName() == dir_path))
      result->push_back(it->second);
  }
}

void DownloadManager::SearchDownloads(const string16& query,
                                      DownloadVector* result) {
  string16 query_lower(base::i18n::ToLower(query));

  for (DownloadMap::iterator it = history_downloads_.begin();
       it != history_downloads_.end(); ++it) {
    DownloadItem* download_item = it->second;

    if (download_item->is_temporary())
      continue;

    // Display Incognito downloads only in Incognito window, and vice versa.
    // The Incognito Downloads page will get the list of non-Incognito downloads
    // from its parent profile.
    if (browser_context_->IsOffTheRecord() != download_item->is_otr())
      continue;

    if (download_item->MatchesQuery(query_lower))
      result->push_back(download_item);
  }
}

// Query the history service for information about all persisted downloads.
bool DownloadManager::Init(content::BrowserContext* browser_context) {
  DCHECK(browser_context);
  DCHECK(!shutdown_needed_)  << "DownloadManager already initialized.";
  shutdown_needed_ = true;

  browser_context_ = browser_context;

  // In test mode, there may be no ResourceDispatcherHost.  In this case it's
  // safe to avoid setting |file_manager_| because we only call a small set of
  // functions, none of which need it.
  ResourceDispatcherHost* rdh =
      content::GetContentClient()->browser()->GetResourceDispatcherHost();
  if (rdh) {
    file_manager_ = rdh->download_file_manager();
    DCHECK(file_manager_);
  }

  return true;
}

// We have received a message from DownloadFileManager about a new download.
void DownloadManager::StartDownload(int32 download_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (delegate_->ShouldStartDownload(download_id))
    RestartDownload(download_id);
}

void DownloadManager::CheckForHistoryFilesRemoval() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  for (DownloadMap::iterator it = history_downloads_.begin();
       it != history_downloads_.end(); ++it) {
    CheckForFileRemoval(it->second);
  }
}

void DownloadManager::CheckForFileRemoval(DownloadItem* download_item) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (download_item->IsComplete() &&
      !download_item->file_externally_removed()) {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        NewRunnableMethod(this,
                          &DownloadManager::CheckForFileRemovalOnFileThread,
                          download_item->db_handle(),
                          download_item->GetTargetFilePath()));
  }
}

void DownloadManager::CheckForFileRemovalOnFileThread(
    int64 db_handle, const FilePath& path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  if (!file_util::PathExists(path)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(this,
                          &DownloadManager::OnFileRemovalDetected,
                          db_handle));
  }
}

void DownloadManager::OnFileRemovalDetected(int64 db_handle) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DownloadMap::iterator it = history_downloads_.find(db_handle);
  if (it != history_downloads_.end()) {
    DownloadItem* download_item = it->second;
    download_item->OnDownloadedFileRemoved();
  }
}

void DownloadManager::RestartDownload(
    int32 download_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DownloadItem* download = GetActiveDownloadItem(download_id);
  if (!download)
    return;

  VLOG(20) << __FUNCTION__ << "()"
           << " download = " << download->DebugString(true);

  FilePath suggested_path = download->suggested_path();

  if (download->prompt_user_for_save_location()) {
    // We must ask the user for the place to put the download.
    DownloadRequestHandle request_handle = download->request_handle();
    TabContents* contents = request_handle.GetTabContents();

    // |id_ptr| will be deleted in either FileSelected() or
    // FileSelectionCancelled().
    int32* id_ptr = new int32;
    *id_ptr = download_id;

    delegate_->ChooseDownloadPath(
        contents, suggested_path, reinterpret_cast<void*>(id_ptr));

    FOR_EACH_OBSERVER(Observer, observers_,
                      SelectFileDialogDisplayed(download_id));
  } else {
    // No prompting for download, just continue with the suggested name.
    ContinueDownloadWithPath(download, suggested_path);
  }
}

void DownloadManager::CreateDownloadItem(DownloadCreateInfo* info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DownloadItem* download = new DownloadItem(this, *info,
                                            browser_context_->IsOffTheRecord());
  int32 download_id = info->download_id;
  DCHECK(!ContainsKey(in_progress_, download_id));

  // TODO(rdsmith): Remove after http://crbug.com/85408 resolved.
  CHECK(!ContainsKey(active_downloads_, download_id));
  downloads_.insert(download);
  active_downloads_[download_id] = download;
}

void DownloadManager::ContinueDownloadWithPath(DownloadItem* download,
                                               const FilePath& chosen_file) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(download);

  int32 download_id = download->id();

  // NOTE(ahendrickson) Eventually |active_downloads_| will replace
  // |in_progress_|, but we don't want to change the semantics yet.
  DCHECK(!ContainsKey(in_progress_, download_id));
  DCHECK(ContainsKey(downloads_, download));
  DCHECK(ContainsKey(active_downloads_, download_id));

  // Make sure the initial file name is set only once.
  DCHECK(download->full_path().empty());
  download->OnPathDetermined(chosen_file);

  VLOG(20) << __FUNCTION__ << "()"
           << " download = " << download->DebugString(true);

  in_progress_[download_id] = download;
  UpdateDownloadProgress();  // Reflect entry into in_progress_.

  // Rename to intermediate name.
  FilePath download_path;
  if (!delegate_->OverrideIntermediatePath(download, &download_path))
    download_path = download->full_path();

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(
          file_manager_, &DownloadFileManager::RenameInProgressDownloadFile,
          download->id(), download_path));

  download->Rename(download_path);

  delegate_->AddItemToPersistentStore(download);
}

void DownloadManager::UpdateDownload(int32 download_id, int64 size) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DownloadMap::iterator it = active_downloads_.find(download_id);
  if (it != active_downloads_.end()) {
    DownloadItem* download = it->second;
    if (download->IsInProgress()) {
      download->Update(size);
      UpdateDownloadProgress();  // Reflect size updates.
      delegate_->UpdateItemInPersistentStore(download);
    }
  }
}

void DownloadManager::OnResponseCompleted(int32 download_id,
                                          int64 size,
                                          int os_error,
                                          const std::string& hash) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // ERR_CONNECTION_CLOSED is allowed since a number of servers in the wild
  // advertise a larger Content-Length than the amount of bytes in the message
  // body, and then close the connection. Other browsers - IE8, Firefox 4.0.1,
  // and Safari 5.0.4 - treat the download as complete in this case, so we
  // follow their lead.
  if (os_error == 0 || os_error == net::ERR_CONNECTION_CLOSED) {
    OnAllDataSaved(download_id, size, hash);
  } else {
    OnDownloadError(download_id, size, os_error);
  }
}

void DownloadManager::OnAllDataSaved(int32 download_id,
                                     int64 size,
                                     const std::string& hash) {
  VLOG(20) << __FUNCTION__ << "()" << " download_id = " << download_id
           << " size = " << size;
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // If it's not in active_downloads_, that means it was cancelled; just
  // ignore the notification.
  if (active_downloads_.count(download_id) == 0)
    return;

  DownloadItem* download = active_downloads_[download_id];
  download->OnAllDataSaved(size);

  MaybeCompleteDownload(download);
}

void DownloadManager::AssertQueueStateConsistent(DownloadItem* download) {
  // TODO(rdsmith): Change to DCHECK after http://crbug.com/85408 resolved.
  if (download->state() == DownloadItem::REMOVING) {
    CHECK(!ContainsKey(downloads_, download));
    CHECK(!ContainsKey(active_downloads_, download->id()));
    CHECK(!ContainsKey(in_progress_, download->id()));
    CHECK(!ContainsKey(history_downloads_, download->db_handle()));
    return;
  }

  // Should be in downloads_ if we're not REMOVING.
  CHECK(ContainsKey(downloads_, download));

  // Check history_downloads_ consistency.
  if (download->db_handle() != DownloadItem::kUninitializedHandle) {
    CHECK(ContainsKey(history_downloads_, download->db_handle()));
  } else {
    // TODO(rdsmith): Somewhat painful; make sure to disable in
    // release builds after resolution of http://crbug.com/85408.
    for (DownloadMap::iterator it = history_downloads_.begin();
         it != history_downloads_.end(); ++it) {
      CHECK(it->second != download);
    }
  }

  if (ContainsKey(active_downloads_, download->id()))
    CHECK_EQ(DownloadItem::IN_PROGRESS, download->state());
  if (DownloadItem::IN_PROGRESS == download->state())
    CHECK(ContainsKey(active_downloads_, download->id()));
}

bool DownloadManager::IsDownloadReadyForCompletion(DownloadItem* download) {
  // If we don't have all the data, the download is not ready for
  // completion.
  if (!download->all_data_saved())
    return false;

  // If the download is dangerous, but not yet validated, it's not ready for
  // completion.
  if (download->safety_state() == DownloadItem::DANGEROUS)
    return false;

  // If the download isn't active (e.g. has been cancelled) it's not
  // ready for completion.
  if (active_downloads_.count(download->id()) == 0)
    return false;

  // If the download hasn't been inserted into the history system
  // (which occurs strictly after file name determination, intermediate
  // file rename, and UI display) then it's not ready for completion.
  if (download->db_handle() == DownloadItem::kUninitializedHandle)
    return false;

  return true;
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
  // in in_progress_, have all data, have a history handle, (validated or safe).
  DCHECK_NE(DownloadItem::DANGEROUS, download->safety_state());
  DCHECK_EQ(1u, in_progress_.count(download->id()));
  DCHECK(download->all_data_saved());
  DCHECK(download->db_handle() != DownloadItem::kUninitializedHandle);
  DCHECK_EQ(1u, history_downloads_.count(download->db_handle()));

  VLOG(20) << __FUNCTION__ << "()" << " executing: download = "
           << download->DebugString(false);

  // Remove the id from in_progress
  in_progress_.erase(download->id());
  UpdateDownloadProgress();  // Reflect removal from in_progress_.

  delegate_->UpdateItemInPersistentStore(download);

  // Finish the download.
  download->OnDownloadCompleting(file_manager_);
}

void DownloadManager::DownloadCompleted(int32 download_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DownloadItem* download = GetDownloadItem(download_id);
  DCHECK(download);
  delegate_->UpdateItemInPersistentStore(download);
  active_downloads_.erase(download_id);
  AssertQueueStateConsistent(download);
}

void DownloadManager::OnDownloadRenamedToFinalName(int download_id,
                                                   const FilePath& full_path,
                                                   int uniquifier) {
  VLOG(20) << __FUNCTION__ << "()" << " download_id = " << download_id
           << " full_path = \"" << full_path.value() << "\""
           << " uniquifier = " << uniquifier;
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DownloadItem* item = GetDownloadItem(download_id);
  if (!item)
    return;

  if (item->safety_state() == DownloadItem::SAFE) {
    DCHECK_EQ(0, uniquifier) << "We should not uniquify SAFE downloads twice";
  }

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(
          file_manager_, &DownloadFileManager::CompleteDownload, download_id));

  if (uniquifier)
    item->set_path_uniquifier(uniquifier);

  item->OnDownloadRenamedToFinalName(full_path);
  delegate_->UpdatePathForItemInPersistentStore(item, full_path);
}

void DownloadManager::CancelDownload(int32 download_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DownloadItem* download = GetDownloadItem(download_id);
  if (!download)
    return;

  download->Cancel(true);
}

void DownloadManager::DownloadCancelledInternal(DownloadItem* download) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  int download_id = download->id();

  VLOG(20) << __FUNCTION__ << "()"
           << " download = " << download->DebugString(true);

  // Clean up will happen when the history system create callback runs if we
  // don't have a valid db_handle yet.
  if (download->db_handle() != DownloadItem::kUninitializedHandle) {
    in_progress_.erase(download_id);
    active_downloads_.erase(download_id);
    UpdateDownloadProgress();  // Reflect removal from in_progress_.
    delegate_->UpdateItemInPersistentStore(download);

    // This function is called from the DownloadItem, so DI state
    // should already have been updated.
    AssertQueueStateConsistent(download);
  }

  download->OffThreadCancel(file_manager_);
}

void DownloadManager::OnDownloadError(int32 download_id,
                                      int64 size,
                                      int os_error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DownloadMap::iterator it = active_downloads_.find(download_id);
  // A cancel at the right time could remove the download from the
  // |active_downloads_| map before we get here.
  if (it == active_downloads_.end())
    return;

  DownloadItem* download = it->second;

  VLOG(20) << __FUNCTION__ << "()" << " Error " << os_error
           << " at offset " << download->received_bytes()
           << " for download = " << download->DebugString(true);

  download->Interrupted(size, os_error);

  // TODO(ahendrickson) - Remove this when we add resuming of interrupted
  // downloads, as we will keep the download item around in that case.
  //
  // Clean up will happen when the history system create callback runs if we
  // don't have a valid db_handle yet.
  if (download->db_handle() != DownloadItem::kUninitializedHandle) {
    in_progress_.erase(download_id);
    active_downloads_.erase(download_id);
    UpdateDownloadProgress();  // Reflect removal from in_progress_.
    delegate_->UpdateItemInPersistentStore(download);
  }

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(
          file_manager_, &DownloadFileManager::CancelDownload, download_id));
}

void DownloadManager::UpdateDownloadProgress() {
  delegate_->DownloadProgressUpdated();
}

int DownloadManager::RemoveDownloadItems(
    const DownloadVector& pending_deletes) {
  if (pending_deletes.empty())
    return 0;

  // Delete from internal maps.
  for (DownloadVector::const_iterator it = pending_deletes.begin();
      it != pending_deletes.end();
      ++it) {
    DownloadItem* download = *it;
    DCHECK(download);
    history_downloads_.erase(download->db_handle());
    save_page_downloads_.erase(download->id());
    downloads_.erase(download);
  }

  // Tell observers to refresh their views.
  NotifyModelChanged();

  // Delete the download items themselves.
  const int num_deleted = static_cast<int>(pending_deletes.size());
  STLDeleteContainerPointers(pending_deletes.begin(), pending_deletes.end());
  return num_deleted;
}

void DownloadManager::RemoveDownload(int64 download_handle) {
  DownloadMap::iterator it = history_downloads_.find(download_handle);
  if (it == history_downloads_.end())
    return;

  // Make history update.
  DownloadItem* download = it->second;
  delegate_->RemoveItemFromPersistentStore(download);

  // Remove from our tables and delete.
  int downloads_count = RemoveDownloadItems(DownloadVector(1, download));
  DCHECK_EQ(1, downloads_count);
}

int DownloadManager::RemoveDownloadsBetween(const base::Time remove_begin,
                                            const base::Time remove_end) {
  delegate_->RemoveItemsFromPersistentStoreBetween(remove_begin, remove_end);

  // All downloads visible to the user will be in the history,
  // so scan that map.
  DownloadVector pending_deletes;
  for (DownloadMap::const_iterator it = history_downloads_.begin();
      it != history_downloads_.end();
      ++it) {
    DownloadItem* download = it->second;
    if (download->start_time() >= remove_begin &&
        (remove_end.is_null() || download->start_time() < remove_end) &&
        (download->IsComplete() || download->IsCancelled())) {
      AssertQueueStateConsistent(download);
      pending_deletes.push_back(download);
    }
  }
  return RemoveDownloadItems(pending_deletes);
}

int DownloadManager::RemoveDownloads(const base::Time remove_begin) {
  return RemoveDownloadsBetween(remove_begin, base::Time());
}

int DownloadManager::RemoveAllDownloads() {
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
  ResourceDispatcherHost* resource_dispatcher_host =
      content::GetContentClient()->browser()->GetResourceDispatcherHost();
  // We send a pointer to content::ResourceContext, instead of the usual
  // reference, so that a copy of the object isn't made.
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      NewRunnableFunction(&BeginDownload,
                          url,
                          referrer,
                          save_info,
                          resource_dispatcher_host,
                          tab_contents->GetRenderProcessHost()->id(),
                          tab_contents->render_view_host()->routing_id(),
                          &tab_contents->browser_context()->
                              GetResourceContext()));
}

void DownloadManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
  observer->ModelChanged();
}

void DownloadManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
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

void DownloadManager::FileSelected(const FilePath& path, void* params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  int32* id_ptr = reinterpret_cast<int32*>(params);
  DCHECK(id_ptr != NULL);
  int32 download_id = *id_ptr;
  delete id_ptr;

  DownloadItem* download = GetActiveDownloadItem(download_id);
  if (!download)
    return;
  VLOG(20) << __FUNCTION__ << "()" << " path = \"" << path.value() << "\""
            << " download = " << download->DebugString(true);

  if (download->prompt_user_for_save_location())
    last_download_path_ = path.DirName();

  // Make sure the initial file name is set only once.
  ContinueDownloadWithPath(download, path);
}

void DownloadManager::FileSelectionCanceled(void* params) {
  // The user didn't pick a place to save the file, so need to cancel the
  // download that's already in progress to the temporary location.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  int32* id_ptr = reinterpret_cast<int32*>(params);
  DCHECK(id_ptr != NULL);
  int32 download_id = *id_ptr;
  delete id_ptr;

  DownloadItem* download = GetActiveDownloadItem(download_id);
  if (!download)
    return;

  VLOG(20) << __FUNCTION__ << "()"
           << " download = " << download->DebugString(true);

  download->OffThreadCancel(file_manager_);
}

// Operations posted to us from the history service ----------------------------

// The history service has retrieved all download entries. 'entries' contains
// 'DownloadPersistentStoreInfo's in sorted order (by ascending start_time).
void DownloadManager::OnPersistentStoreQueryComplete(
    std::vector<DownloadPersistentStoreInfo>* entries) {
  // TODO(rdsmith): Remove this and related logic when
  // http://crbug.com/84508 is fixed.
  largest_db_handle_in_history_ = 0;

  for (size_t i = 0; i < entries->size(); ++i) {
    DownloadItem* download = new DownloadItem(this, entries->at(i));
    // TODO(rdsmith): Remove after http://crbug.com/85408 resolved.
    CHECK(!ContainsKey(history_downloads_, download->db_handle()));
    downloads_.insert(download);
    history_downloads_[download->db_handle()] = download;
    VLOG(20) << __FUNCTION__ << "()" << i << ">"
             << " download = " << download->DebugString(true);

    if (download->db_handle() > largest_db_handle_in_history_)
      largest_db_handle_in_history_ = download->db_handle();
  }
  NotifyModelChanged();
  CheckForHistoryFilesRemoval();
}

void DownloadManager::AddDownloadItemToHistory(DownloadItem* download,
                                               int64 db_handle) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // TODO(rdsmith): Convert to DCHECK() when http://crbug.com/84508
  // is fixed.
  CHECK_NE(DownloadItem::kUninitializedHandle, db_handle);

  DCHECK(download->db_handle() == DownloadItem::kUninitializedHandle);
  download->set_db_handle(db_handle);

  // TODO(rdsmith): Convert to DCHECK() when http://crbug.com/84508
  // is fixed.
  CHECK(!ContainsKey(history_downloads_, download->db_handle()));
  history_downloads_[download->db_handle()] = download;

  // Show in the appropriate browser UI.
  // This includes buttons to save or cancel, for a dangerous download.
  ShowDownloadInBrowser(download);

  // Inform interested objects about the new download.
  NotifyModelChanged();
}


void DownloadManager::OnItemAddedToPersistentStore(int32 download_id,
                                                   int64 db_handle) {
  if (save_page_downloads_.count(download_id)) {
    OnSavePageItemAddedToPersistentStore(download_id, db_handle);
  } else if (active_downloads_.count(download_id)) {
    OnDownloadItemAddedToPersistentStore(download_id, db_handle);
  }
  // It's valid that we don't find a matching item, i.e. on shutdown.
}

// Once the new DownloadItem's creation info has been committed to the history
// service, we associate the DownloadItem with the db handle, update our
// 'history_downloads_' map and inform observers.
void DownloadManager::OnDownloadItemAddedToPersistentStore(int32 download_id,
                                                           int64 db_handle) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DownloadItem* download = GetActiveDownloadItem(download_id);
  if (!download)
    return;

  VLOG(20) << __FUNCTION__ << "()" << " db_handle = " << db_handle
           << " download_id = " << download_id
           << " download = " << download->DebugString(true);

  // TODO(rdsmith): Remove after http://crbug.com/85408 resolved.
  CHECK(!ContainsKey(history_downloads_, download->db_handle()));
  int64 largest_handle = largest_db_handle_in_history_;
  base::debug::Alias(&largest_handle);

  AddDownloadItemToHistory(download, db_handle);

  // If the download is still in progress, try to complete it.
  //
  // Otherwise, download has been cancelled or interrupted before we've
  // received the DB handle.  We post one final message to the history
  // service so that it can be properly in sync with the DownloadItem's
  // completion status, and also inform any observers so that they get
  // more than just the start notification.
  if (download->IsInProgress()) {
    MaybeCompleteDownload(download);
  } else {
    // TODO(rdsmith): Convert to DCHECK() when http://crbug.com/84508
    // is fixed.
    CHECK(download->IsCancelled())
        << " download = " << download->DebugString(true);
    in_progress_.erase(download_id);
    active_downloads_.erase(download_id);
    delegate_->UpdateItemInPersistentStore(download);
    download->UpdateObservers();
  }
}

void DownloadManager::ShowDownloadInBrowser(DownloadItem* download) {
  // The 'contents' may no longer exist if the user closed the tab before we
  // get this start completion event.
  DownloadRequestHandle request_handle = download->request_handle();
  TabContents* content = request_handle.GetTabContents();

  // If the contents no longer exists, we ask the embedder to suggest another
  // tab.
  if (!content)
    content = delegate_->GetAlternativeTabContentsToNotifyForDownload();

  if (content)
    content->OnStartDownload(download);
}

// Clears the last download path, used to initialize "save as" dialogs.
void DownloadManager::ClearLastDownloadPath() {
  last_download_path_ = FilePath();
}

void DownloadManager::NotifyModelChanged() {
  FOR_EACH_OBSERVER(Observer, observers_, ModelChanged());
}

DownloadItem* DownloadManager::GetDownloadItem(int download_id) {
  // The |history_downloads_| map is indexed by the download's db_handle,
  // not its id, so we have to iterate.
  for (DownloadMap::iterator it = history_downloads_.begin();
       it != history_downloads_.end(); ++it) {
    DownloadItem* item = it->second;
    if (item->id() == download_id)
      return item;
  }
  return NULL;
}

DownloadItem* DownloadManager::GetActiveDownloadItem(int download_id) {
  DCHECK(ContainsKey(active_downloads_, download_id));
  DownloadItem* download = active_downloads_[download_id];
  DCHECK(download != NULL);
  return download;
}

// Confirm that everything in all maps is also in |downloads_|, and that
// everything in |downloads_| is also in some other map.
void DownloadManager::AssertContainersConsistent() const {
#if !defined(NDEBUG)
  // Turn everything into sets.
  const DownloadMap* input_maps[] = {&active_downloads_,
                                     &history_downloads_,
                                     &save_page_downloads_};
  DownloadSet active_set, history_set, save_page_set;
  DownloadSet* all_sets[] = {&active_set, &history_set, &save_page_set};
  DCHECK_EQ(ARRAYSIZE_UNSAFE(input_maps), ARRAYSIZE_UNSAFE(all_sets));
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(input_maps); i++) {
    for (DownloadMap::const_iterator it = input_maps[i]->begin();
         it != input_maps[i]->end(); ++it) {
      all_sets[i]->insert(&*it->second);
    }
  }

  // Check if each set is fully present in downloads, and create a union.
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

void DownloadManager::SavePageDownloadStarted(DownloadItem* download) {
  DCHECK(!ContainsKey(save_page_downloads_, download->id()));
  downloads_.insert(download);
  save_page_downloads_[download->id()] = download;

  // Add this entry to the history service.
  // Additionally, the UI is notified in the callback.
  delegate_->AddItemToPersistentStore(download);
}

// SavePackage will call SavePageDownloadFinished upon completion/cancellation.
// The history callback will call OnSavePageItemAddedToPersistentStore.
// If the download finishes before the history callback,
// OnSavePageItemAddedToPersistentStore calls SavePageDownloadFinished, ensuring
// that the history event is update regardless of the order in which these two
// events complete.
// If something removes the download item from the download manager (Remove,
// Shutdown) the result will be that the SavePage system will not be able to
// properly update the download item (which no longer exists) or the download
// history, but the action will complete properly anyway.  This may lead to the
// history entry being wrong on a reload of chrome (specifically in the case of
// Initiation -> History Callback -> Removal -> Completion), but there's no way
// to solve that without canceling on Remove (which would then update the DB).

void DownloadManager::OnSavePageItemAddedToPersistentStore(int32 download_id,
                                                           int64 db_handle) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DownloadMap::const_iterator it = save_page_downloads_.find(download_id);
  // This can happen if the download manager is shutting down and all maps
  // have been cleared.
  if (it == save_page_downloads_.end())
    return;

  DownloadItem* download = it->second;
  if (!download) {
    NOTREACHED();
    return;
  }

  // TODO(rdsmith): Remove after http://crbug.com/85408 resolved.
  CHECK(!ContainsKey(history_downloads_, download->db_handle()));
  int64 largest_handle = largest_db_handle_in_history_;
  base::debug::Alias(&largest_handle);

  AddDownloadItemToHistory(download, db_handle);

  // Finalize this download if it finished before the history callback.
  if (!download->IsInProgress())
    SavePageDownloadFinished(download);
}

void DownloadManager::SavePageDownloadFinished(DownloadItem* download) {
  if (download->db_handle() != DownloadItem::kUninitializedHandle) {
    delegate_->UpdateItemInPersistentStore(download);
    DCHECK(ContainsKey(save_page_downloads_, download->id()));
    save_page_downloads_.erase(download->id());

    if (download->IsComplete())
      NotificationService::current()->Notify(
          content::NOTIFICATION_SAVE_PACKAGE_SUCCESSFULLY_FINISHED,
          Source<DownloadManager>(this),
          Details<DownloadItem>(download));
  }
}
