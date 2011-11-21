// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_manager_impl.h"

#include <iterator>

#include "base/bind.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/sys_string_conversions.h"
#include "build/build_config.h"
#include "content/browser/browser_context.h"
#include "content/browser/download/download_create_info.h"
#include "content/browser/download/download_file_manager.h"
#include "content/browser/download/download_id_factory.h"
#include "content/browser/download/download_item_impl.h"
#include "content/browser/download/download_persistent_store_info.h"
#include "content/browser/download/download_stats.h"
#include "content/browser/download/download_status_updater.h"
#include "content/browser/download/interrupt_reasons.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/download_manager_delegate.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"

using content::BrowserThread;

namespace {

// Param structs exist because base::Bind can only handle 6 args.
struct URLParams {
  URLParams(const GURL& url, const GURL& referrer)
    : url_(url), referrer_(referrer) {}
  GURL url_;
  GURL referrer_;
};

struct RenderParams {
  RenderParams(int rpi, int rvi)
    : render_process_id_(rpi), render_view_id_(rvi) {}
  int render_process_id_;
  int render_view_id_;
};

void BeginDownload(const URLParams& url_params,
                   const DownloadSaveInfo& save_info,
                   ResourceDispatcherHost* resource_dispatcher_host,
                   const RenderParams& render_params,
                   const content::ResourceContext* context) {
  net::URLRequest* request = new net::URLRequest(url_params.url_,
                                                 resource_dispatcher_host);
  request->set_referrer(url_params.referrer_.spec());
  resource_dispatcher_host->BeginDownload(
      request, save_info, true,
      DownloadResourceHandler::OnStartedCallback(),
      render_params.render_process_id_,
      render_params.render_view_id_,
      *context);
}

}  // namespace

DownloadManagerImpl::DownloadManagerImpl(
    content::DownloadManagerDelegate* delegate,
    DownloadIdFactory* id_factory,
    DownloadStatusUpdater* status_updater)
        : shutdown_needed_(false),
          browser_context_(NULL),
          file_manager_(NULL),
          status_updater_((status_updater != NULL)
                          ? status_updater->AsWeakPtr()
                          : base::WeakPtr<DownloadStatusUpdater>()),
          delegate_(delegate),
          id_factory_(id_factory),
          largest_db_handle_in_history_(DownloadItem::kUninitializedHandle) {
  // NOTE(benjhayden): status_updater may be NULL when using
  // TestingBrowserProcess.
  if (status_updater_.get() != NULL)
    status_updater_->AddDelegate(this);
}

DownloadManagerImpl::~DownloadManagerImpl() {
  DCHECK(!shutdown_needed_);
  if (status_updater_.get() != NULL)
    status_updater_->RemoveDelegate(this);
}

DownloadId DownloadManagerImpl::GetNextId() {
  return id_factory_->GetNextId();
}

void DownloadManagerImpl::Shutdown() {
  VLOG(20) << __FUNCTION__ << "()"
           << " shutdown_needed_ = " << shutdown_needed_;
  if (!shutdown_needed_)
    return;
  shutdown_needed_ = false;

  FOR_EACH_OBSERVER(Observer, observers_, ManagerGoingDown());
  // TODO(benjhayden): Consider clearing observers_.

  if (file_manager_) {
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
        base::Bind(&DownloadFileManager::OnDownloadManagerShutdown,
                   file_manager_, make_scoped_refptr(this)));
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

    if (download->GetSafetyState() == DownloadItem::DANGEROUS &&
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

void DownloadManagerImpl::GetTemporaryDownloads(
    const FilePath& dir_path, DownloadVector* result) {
  DCHECK(result);

  for (DownloadMap::iterator it = history_downloads_.begin();
       it != history_downloads_.end(); ++it) {
    if (it->second->IsTemporary() &&
        it->second->GetFullPath().DirName() == dir_path)
      result->push_back(it->second);
  }
}

void DownloadManagerImpl::GetAllDownloads(
    const FilePath& dir_path, DownloadVector* result) {
  DCHECK(result);

  for (DownloadMap::iterator it = history_downloads_.begin();
       it != history_downloads_.end(); ++it) {
    if (!it->second->IsTemporary() &&
        (dir_path.empty() || it->second->GetFullPath().DirName() == dir_path))
      result->push_back(it->second);
  }
}

void DownloadManagerImpl::SearchDownloads(const string16& query,
                                          DownloadVector* result) {
  string16 query_lower(base::i18n::ToLower(query));

  for (DownloadMap::iterator it = history_downloads_.begin();
       it != history_downloads_.end(); ++it) {
    DownloadItem* download_item = it->second;

    if (download_item->IsTemporary())
      continue;

    // Display Incognito downloads only in Incognito window, and vice versa.
    // The Incognito Downloads page will get the list of non-Incognito downloads
    // from its parent profile.
    if (browser_context_->IsOffTheRecord() != download_item->IsOtr())
      continue;

    if (download_item->MatchesQuery(query_lower))
      result->push_back(download_item);
  }
}

// Query the history service for information about all persisted downloads.
bool DownloadManagerImpl::Init(content::BrowserContext* browser_context) {
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
void DownloadManagerImpl::StartDownload(int32 download_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (delegate_->ShouldStartDownload(download_id))
    RestartDownload(download_id);
}

void DownloadManagerImpl::CheckForHistoryFilesRemoval() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  for (DownloadMap::iterator it = history_downloads_.begin();
       it != history_downloads_.end(); ++it) {
    CheckForFileRemoval(it->second);
  }
}

void DownloadManagerImpl::CheckForFileRemoval(DownloadItem* download_item) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (download_item->IsComplete() &&
      !download_item->GetFileExternallyRemoved()) {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&DownloadManagerImpl::CheckForFileRemovalOnFileThread,
                   this, download_item->GetDbHandle(),
                   download_item->GetTargetFilePath()));
  }
}

void DownloadManagerImpl::CheckForFileRemovalOnFileThread(
    int64 db_handle, const FilePath& path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  if (!file_util::PathExists(path)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&DownloadManagerImpl::OnFileRemovalDetected,
                   this,
                   db_handle));
  }
}

void DownloadManagerImpl::OnFileRemovalDetected(int64 db_handle) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DownloadMap::iterator it = history_downloads_.find(db_handle);
  if (it != history_downloads_.end()) {
    DownloadItem* download_item = it->second;
    download_item->OnDownloadedFileRemoved();
  }
}

void DownloadManagerImpl::RestartDownload(
    int32 download_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DownloadItem* download = GetActiveDownloadItem(download_id);
  if (!download)
    return;

  VLOG(20) << __FUNCTION__ << "()"
           << " download = " << download->DebugString(true);

  FilePath suggested_path = download->GetSuggestedPath();

  if (download->PromptUserForSaveLocation()) {
    // We must ask the user for the place to put the download.
    TabContents* contents = download->GetTabContents();

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

content::BrowserContext* DownloadManagerImpl::BrowserContext() {
  return browser_context_;
}

FilePath DownloadManagerImpl::LastDownloadPath() {
  return last_download_path_;
}

void DownloadManagerImpl::CreateDownloadItem(
    DownloadCreateInfo* info, const DownloadRequestHandle& request_handle) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DownloadItem* download = new DownloadItemImpl(
      this, *info, new DownloadRequestHandle(request_handle),
      browser_context_->IsOffTheRecord());
  int32 download_id = info->download_id.local();
  DCHECK(!ContainsKey(in_progress_, download_id));

  // TODO(rdsmith): Remove after http://crbug.com/85408 resolved.
  CHECK(!ContainsKey(active_downloads_, download_id));
  downloads_.insert(download);
  active_downloads_[download_id] = download;
}

void DownloadManagerImpl::ContinueDownloadWithPath(
    DownloadItem* download, const FilePath& chosen_file) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(download);

  int32 download_id = download->GetId();

  // NOTE(ahendrickson) Eventually |active_downloads_| will replace
  // |in_progress_|, but we don't want to change the semantics yet.
  DCHECK(!ContainsKey(in_progress_, download_id));
  DCHECK(ContainsKey(downloads_, download));
  DCHECK(ContainsKey(active_downloads_, download_id));

  // Make sure the initial file name is set only once.
  download->OnPathDetermined(chosen_file);

  VLOG(20) << __FUNCTION__ << "()"
           << " download = " << download->DebugString(true);

  in_progress_[download_id] = download;
  UpdateDownloadProgress();  // Reflect entry into in_progress_.

  // Rename to intermediate name.
  FilePath download_path;
  if (!delegate_->OverrideIntermediatePath(download, &download_path))
    download_path = download->GetFullPath();

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&DownloadFileManager::RenameInProgressDownloadFile,
                 file_manager_, download->GetGlobalId(), download_path));

  download->Rename(download_path);

  delegate_->AddItemToPersistentStore(download);
}

void DownloadManagerImpl::UpdateDownload(int32 download_id, int64 size) {
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

void DownloadManagerImpl::OnResponseCompleted(int32 download_id,
                                              int64 size,
                                              const std::string& hash) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  VLOG(20) << __FUNCTION__ << "()" << " download_id = " << download_id
           << " size = " << size;
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // If it's not in active_downloads_, that means it was cancelled; just
  // ignore the notification.
  if (active_downloads_.count(download_id) == 0)
    return;

  DownloadItem* download = active_downloads_[download_id];
  download->OnAllDataSaved(size, hash);
  delegate_->OnResponseCompleted(download);

  MaybeCompleteDownload(download);
}

void DownloadManagerImpl::AssertQueueStateConsistent(DownloadItem* download) {
  // TODO(rdsmith): Change to DCHECK after http://crbug.com/85408 resolved.
  if (download->GetState() == DownloadItem::REMOVING) {
    CHECK(!ContainsKey(downloads_, download));
    CHECK(!ContainsKey(active_downloads_, download->GetId()));
    CHECK(!ContainsKey(in_progress_, download->GetId()));
    CHECK(!ContainsKey(history_downloads_, download->GetDbHandle()));
    return;
  }

  // Should be in downloads_ if we're not REMOVING.
  CHECK(ContainsKey(downloads_, download));

  // Check history_downloads_ consistency.
  if (download->GetDbHandle() != DownloadItem::kUninitializedHandle) {
    CHECK(ContainsKey(history_downloads_, download->GetDbHandle()));
  } else {
    // TODO(rdsmith): Somewhat painful; make sure to disable in
    // release builds after resolution of http://crbug.com/85408.
    for (DownloadMap::iterator it = history_downloads_.begin();
         it != history_downloads_.end(); ++it) {
      CHECK(it->second != download);
    }
  }

  int64 state = download->GetState();
  base::debug::Alias(&state);
  if (ContainsKey(active_downloads_, download->GetId())) {
    if (download->GetDbHandle() != DownloadItem::kUninitializedHandle)
      CHECK_EQ(DownloadItem::IN_PROGRESS, download->GetState());
    if (DownloadItem::IN_PROGRESS != download->GetState())
      CHECK_EQ(DownloadItem::kUninitializedHandle, download->GetDbHandle());
  }
  if (DownloadItem::IN_PROGRESS == download->GetState())
    CHECK(ContainsKey(active_downloads_, download->GetId()));
}

bool DownloadManagerImpl::IsDownloadReadyForCompletion(DownloadItem* download) {
  // If we don't have all the data, the download is not ready for
  // completion.
  if (!download->AllDataSaved())
    return false;

  // If the download is dangerous, but not yet validated, it's not ready for
  // completion.
  if (download->GetSafetyState() == DownloadItem::DANGEROUS)
    return false;

  // If the download isn't active (e.g. has been cancelled) it's not
  // ready for completion.
  if (active_downloads_.count(download->GetId()) == 0)
    return false;

  // If the download hasn't been inserted into the history system
  // (which occurs strictly after file name determination, intermediate
  // file rename, and UI display) then it's not ready for completion.
  if (download->GetDbHandle() == DownloadItem::kUninitializedHandle)
    return false;

  return true;
}

void DownloadManagerImpl::MaybeCompleteDownload(DownloadItem* download) {
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
  DCHECK_NE(DownloadItem::DANGEROUS, download->GetSafetyState());
  DCHECK_EQ(1u, in_progress_.count(download->GetId()));
  DCHECK(download->AllDataSaved());
  DCHECK(download->GetDbHandle() != DownloadItem::kUninitializedHandle);
  DCHECK_EQ(1u, history_downloads_.count(download->GetDbHandle()));

  // Give the delegate a chance to override.
  if (!delegate_->ShouldCompleteDownload(download))
    return;

  VLOG(20) << __FUNCTION__ << "()" << " executing: download = "
           << download->DebugString(false);

  // Remove the id from in_progress
  in_progress_.erase(download->GetId());
  UpdateDownloadProgress();  // Reflect removal from in_progress_.

  delegate_->UpdateItemInPersistentStore(download);

  // Finish the download.
  download->OnDownloadCompleting(file_manager_);
}

void DownloadManagerImpl::DownloadCompleted(int32 download_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DownloadItem* download = GetDownloadItem(download_id);
  DCHECK(download);
  delegate_->UpdateItemInPersistentStore(download);
  active_downloads_.erase(download_id);
  AssertQueueStateConsistent(download);
}

void DownloadManagerImpl::OnDownloadRenamedToFinalName(
    int download_id,
    const FilePath& full_path,
    int uniquifier) {
  VLOG(20) << __FUNCTION__ << "()" << " download_id = " << download_id
           << " full_path = \"" << full_path.value() << "\""
           << " uniquifier = " << uniquifier;
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DownloadItem* item = GetDownloadItem(download_id);
  if (!item)
    return;

  if (item->GetSafetyState() == DownloadItem::SAFE) {
    DCHECK_EQ(0, uniquifier) << "We should not uniquify SAFE downloads twice";
  }

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&DownloadFileManager::CompleteDownload,
                 file_manager_, item->GetGlobalId()));

  if (uniquifier)
    item->SetPathUniquifier(uniquifier);

  item->OnDownloadRenamedToFinalName(full_path);
  delegate_->UpdatePathForItemInPersistentStore(item, full_path);
}

void DownloadManagerImpl::CancelDownload(int32 download_id) {
  DownloadItem* download = GetActiveDownload(download_id);
  // A cancel at the right time could remove the download from the
  // |active_downloads_| map before we get here.
  if (!download)
    return;

  download->Cancel(true);
}

void DownloadManagerImpl::DownloadCancelledInternal(DownloadItem* download) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  VLOG(20) << __FUNCTION__ << "()"
           << " download = " << download->DebugString(true);

  RemoveFromActiveList(download);
  // This function is called from the DownloadItem, so DI state
  // should already have been updated.
  AssertQueueStateConsistent(download);

  if (file_manager_)
    download->OffThreadCancel(file_manager_);
}

void DownloadManagerImpl::OnDownloadInterrupted(int32 download_id,
                                                int64 size,
                                                InterruptReason reason) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DownloadItem* download = GetActiveDownload(download_id);
  if (!download)
    return;

  VLOG(20) << __FUNCTION__ << "()"
           << " reason " << InterruptReasonDebugString(reason)
           << " at offset " << download->GetReceivedBytes()
           << " size = " << size
           << " download = " << download->DebugString(true);

  RemoveFromActiveList(download);
  download->Interrupted(size, reason);
  download->OffThreadCancel(file_manager_);
}

DownloadItem* DownloadManagerImpl::GetActiveDownload(int32 download_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DownloadMap::iterator it = active_downloads_.find(download_id);
  if (it == active_downloads_.end())
    return NULL;

  DownloadItem* download = it->second;

  DCHECK(download);
  DCHECK_EQ(download_id, download->GetId());

  return download;
}

void DownloadManagerImpl::RemoveFromActiveList(DownloadItem* download) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(download);

  // Clean up will happen when the history system create callback runs if we
  // don't have a valid db_handle yet.
  if (download->GetDbHandle() != DownloadItem::kUninitializedHandle) {
    in_progress_.erase(download->GetId());
    active_downloads_.erase(download->GetId());
    UpdateDownloadProgress();  // Reflect removal from in_progress_.
    delegate_->UpdateItemInPersistentStore(download);
  }
}

content::DownloadManagerDelegate* DownloadManagerImpl::delegate() const {
  return delegate_;
}

void DownloadManagerImpl::SetDownloadManagerDelegate(
    content::DownloadManagerDelegate* delegate) {
  delegate_ = delegate;
}

void DownloadManagerImpl::UpdateDownloadProgress() {
  delegate_->DownloadProgressUpdated();
}

int DownloadManagerImpl::RemoveDownloadItems(
    const DownloadVector& pending_deletes) {
  if (pending_deletes.empty())
    return 0;

  // Delete from internal maps.
  for (DownloadVector::const_iterator it = pending_deletes.begin();
      it != pending_deletes.end();
      ++it) {
    DownloadItem* download = *it;
    DCHECK(download);
    history_downloads_.erase(download->GetDbHandle());
    save_page_downloads_.erase(download->GetId());
    downloads_.erase(download);
  }

  // Tell observers to refresh their views.
  NotifyModelChanged();

  // Delete the download items themselves.
  const int num_deleted = static_cast<int>(pending_deletes.size());
  STLDeleteContainerPointers(pending_deletes.begin(), pending_deletes.end());
  return num_deleted;
}

void DownloadManagerImpl::RemoveDownload(int64 download_handle) {
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

int DownloadManagerImpl::RemoveDownloadsBetween(const base::Time remove_begin,
                                                const base::Time remove_end) {
  delegate_->RemoveItemsFromPersistentStoreBetween(remove_begin, remove_end);

  // All downloads visible to the user will be in the history,
  // so scan that map.
  DownloadVector pending_deletes;
  for (DownloadMap::const_iterator it = history_downloads_.begin();
      it != history_downloads_.end();
      ++it) {
    DownloadItem* download = it->second;
    if (download->GetStartTime() >= remove_begin &&
        (remove_end.is_null() || download->GetStartTime() < remove_end) &&
        (download->IsComplete() || download->IsCancelled())) {
      AssertQueueStateConsistent(download);
      pending_deletes.push_back(download);
    }
  }
  return RemoveDownloadItems(pending_deletes);
}

int DownloadManagerImpl::RemoveDownloads(const base::Time remove_begin) {
  return RemoveDownloadsBetween(remove_begin, base::Time());
}

int DownloadManagerImpl::RemoveAllDownloads() {
  download_stats::RecordClearAllSize(history_downloads_.size());
  // The null times make the date range unbounded.
  return RemoveDownloadsBetween(base::Time(), base::Time());
}

// Initiate a download of a specific URL. We send the request to the
// ResourceDispatcherHost, and let it send us responses like a regular
// download.
void DownloadManagerImpl::DownloadUrl(const GURL& url,
                                      const GURL& referrer,
                                      const std::string& referrer_charset,
                                      TabContents* tab_contents) {
  DownloadUrlToFile(url, referrer, referrer_charset, DownloadSaveInfo(),
                    tab_contents);
}

void DownloadManagerImpl::DownloadUrlToFile(const GURL& url,
                                            const GURL& referrer,
                                            const std::string& referrer_charset,
                                            const DownloadSaveInfo& save_info,
                                            TabContents* tab_contents) {
  DCHECK(tab_contents);
  ResourceDispatcherHost* resource_dispatcher_host =
      content::GetContentClient()->browser()->GetResourceDispatcherHost();
  // We send a pointer to content::ResourceContext, instead of the usual
  // reference, so that a copy of the object isn't made.
  // base::Bind can't handle 7 args, so we use URLParams and RenderParams.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&BeginDownload,
          URLParams(url, referrer), save_info, resource_dispatcher_host,
          RenderParams(tab_contents->GetRenderProcessHost()->GetID(),
                       tab_contents->render_view_host()->routing_id()),
          &tab_contents->browser_context()->GetResourceContext()));
}

void DownloadManagerImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
  observer->ModelChanged();
}

void DownloadManagerImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool DownloadManagerImpl::IsDownloadProgressKnown() const {
  for (DownloadMap::const_iterator i = in_progress_.begin();
       i != in_progress_.end(); ++i) {
    if (i->second->GetTotalBytes() <= 0)
      return false;
  }

  return true;
}

int64 DownloadManagerImpl::GetInProgressDownloadCount() const {
  return in_progress_.size();
}

int64 DownloadManagerImpl::GetReceivedDownloadBytes() const {
  DCHECK(IsDownloadProgressKnown());
  int64 received_bytes = 0;
  for (DownloadMap::const_iterator i = in_progress_.begin();
       i != in_progress_.end(); ++i) {
    received_bytes += i->second->GetReceivedBytes();
  }
  return received_bytes;
}

int64 DownloadManagerImpl::GetTotalDownloadBytes() const {
  DCHECK(IsDownloadProgressKnown());
  int64 total_bytes = 0;
  for (DownloadMap::const_iterator i = in_progress_.begin();
       i != in_progress_.end(); ++i) {
    total_bytes += i->second->GetTotalBytes();
  }
  return total_bytes;
}

void DownloadManagerImpl::FileSelected(const FilePath& path, void* params) {
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

  if (download->PromptUserForSaveLocation())
    last_download_path_ = path.DirName();

  // Make sure the initial file name is set only once.
  ContinueDownloadWithPath(download, path);
}

void DownloadManagerImpl::FileSelectionCanceled(void* params) {
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

  // TODO(ahendrickson) -- This currently has no effect, as the download is
  // not put on the active list until the file selection is complete.  Need
  // to put it on the active list earlier in the process.
  RemoveFromActiveList(download);

  download->OffThreadCancel(file_manager_);
}

// Operations posted to us from the history service ----------------------------

// The history service has retrieved all download entries. 'entries' contains
// 'DownloadPersistentStoreInfo's in sorted order (by ascending start_time).
void DownloadManagerImpl::OnPersistentStoreQueryComplete(
    std::vector<DownloadPersistentStoreInfo>* entries) {
  // TODO(rdsmith): Remove this and related logic when
  // http://crbug.com/85408 is fixed.
  largest_db_handle_in_history_ = 0;

  for (size_t i = 0; i < entries->size(); ++i) {
    DownloadItem* download = new DownloadItemImpl(this, entries->at(i));
    // TODO(rdsmith): Remove after http://crbug.com/85408 resolved.
    CHECK(!ContainsKey(history_downloads_, download->GetDbHandle()));
    downloads_.insert(download);
    history_downloads_[download->GetDbHandle()] = download;
    VLOG(20) << __FUNCTION__ << "()" << i << ">"
             << " download = " << download->DebugString(true);

    if (download->GetDbHandle() > largest_db_handle_in_history_)
      largest_db_handle_in_history_ = download->GetDbHandle();
  }
  NotifyModelChanged();
  CheckForHistoryFilesRemoval();
}

void DownloadManagerImpl::AddDownloadItemToHistory(DownloadItem* download,
                                                   int64 db_handle) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // TODO(rdsmith): Convert to DCHECK() when http://crbug.com/85408
  // is fixed.
  CHECK_NE(DownloadItem::kUninitializedHandle, db_handle);

  download_stats::RecordHistorySize(history_downloads_.size());

  DCHECK(download->GetDbHandle() == DownloadItem::kUninitializedHandle);
  download->SetDbHandle(db_handle);

  // TODO(rdsmith): Convert to DCHECK() when http://crbug.com/85408
  // is fixed.
  CHECK(!ContainsKey(history_downloads_, download->GetDbHandle()));
  history_downloads_[download->GetDbHandle()] = download;

  // Show in the appropriate browser UI.
  // This includes buttons to save or cancel, for a dangerous download.
  ShowDownloadInBrowser(download);

  // Inform interested objects about the new download.
  NotifyModelChanged();
}


void DownloadManagerImpl::OnItemAddedToPersistentStore(int32 download_id,
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
void DownloadManagerImpl::OnDownloadItemAddedToPersistentStore(
    int32 download_id, int64 db_handle) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DownloadItem* download = GetActiveDownloadItem(download_id);
  if (!download)
    return;

  VLOG(20) << __FUNCTION__ << "()" << " db_handle = " << db_handle
           << " download_id = " << download_id
           << " download = " << download->DebugString(true);

  // TODO(rdsmith): Remove after http://crbug.com/85408 resolved.
  int64 largest_handle = largest_db_handle_in_history_;
  base::debug::Alias(&largest_handle);
  int32 matching_item_download_id
      = (ContainsKey(history_downloads_, db_handle) ?
         history_downloads_[db_handle]->GetId() : 0);
  base::debug::Alias(&matching_item_download_id);

  CHECK(!ContainsKey(history_downloads_, db_handle));

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
    // TODO(rdsmith): Convert to DCHECK() when http://crbug.com/85408
    // is fixed.
    CHECK(download->IsCancelled())
        << " download = " << download->DebugString(true);
    in_progress_.erase(download_id);
    active_downloads_.erase(download_id);
    delegate_->UpdateItemInPersistentStore(download);
    download->UpdateObservers();
  }
}

void DownloadManagerImpl::ShowDownloadInBrowser(DownloadItem* download) {
  // The 'contents' may no longer exist if the user closed the tab before we
  // get this start completion event.
  TabContents* content = download->GetTabContents();

  // If the contents no longer exists, we ask the embedder to suggest another
  // tab.
  if (!content)
    content = delegate_->GetAlternativeTabContentsToNotifyForDownload();

  if (content)
    content->OnStartDownload(download);
}

int DownloadManagerImpl::InProgressCount() const {
  return static_cast<int>(in_progress_.size());
}

// Clears the last download path, used to initialize "save as" dialogs.
void DownloadManagerImpl::ClearLastDownloadPath() {
  last_download_path_ = FilePath();
}

void DownloadManagerImpl::NotifyModelChanged() {
  FOR_EACH_OBSERVER(Observer, observers_, ModelChanged());
}

DownloadItem* DownloadManagerImpl::GetDownloadItem(int download_id) {
  // The |history_downloads_| map is indexed by the download's db_handle,
  // not its id, so we have to iterate.
  for (DownloadMap::iterator it = history_downloads_.begin();
       it != history_downloads_.end(); ++it) {
    DownloadItem* item = it->second;
    if (item->GetId() == download_id)
      return item;
  }
  return NULL;
}

DownloadItem* DownloadManagerImpl::GetActiveDownloadItem(int download_id) {
  DCHECK(ContainsKey(active_downloads_, download_id));
  DownloadItem* download = active_downloads_[download_id];
  DCHECK(download != NULL);
  return download;
}

// Confirm that everything in all maps is also in |downloads_|, and that
// everything in |downloads_| is also in some other map.
void DownloadManagerImpl::AssertContainersConsistent() const {
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

void DownloadManagerImpl::SavePageDownloadStarted(DownloadItem* download) {
  DCHECK(!ContainsKey(save_page_downloads_, download->GetId()));
  downloads_.insert(download);
  save_page_downloads_[download->GetId()] = download;

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

void DownloadManagerImpl::OnSavePageItemAddedToPersistentStore(
    int32 download_id, int64 db_handle) {
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
  int64 largest_handle = largest_db_handle_in_history_;
  base::debug::Alias(&largest_handle);
  CHECK(!ContainsKey(history_downloads_, db_handle));

  AddDownloadItemToHistory(download, db_handle);

  // Finalize this download if it finished before the history callback.
  if (!download->IsInProgress())
    SavePageDownloadFinished(download);
}

void DownloadManagerImpl::SavePageDownloadFinished(DownloadItem* download) {
  if (download->GetDbHandle() != DownloadItem::kUninitializedHandle) {
    delegate_->UpdateItemInPersistentStore(download);
    DCHECK(ContainsKey(save_page_downloads_, download->GetId()));
    save_page_downloads_.erase(download->GetId());

    if (download->IsComplete())
      content::NotificationService::current()->Notify(
          content::NOTIFICATION_SAVE_PACKAGE_SUCCESSFULLY_FINISHED,
          content::Source<DownloadManager>(this),
          content::Details<DownloadItem>(download));
  }
}

void DownloadManagerImpl::MarkDownloadOpened(DownloadItem* download) {
  delegate_->UpdateItemInPersistentStore(download);
  int num_unopened = 0;
  for (DownloadMap::iterator it = history_downloads_.begin();
       it != history_downloads_.end(); ++it) {
    if (it->second->IsComplete() && !it->second->GetOpened())
      ++num_unopened;
  }
  download_stats::RecordOpensOutstanding(num_unopened);
}

void DownloadManagerImpl::SetFileManager(DownloadFileManager* file_manager) {
  file_manager_ = file_manager;
}
