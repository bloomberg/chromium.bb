// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_manager_impl.h"

#include <iterator>

#include "base/bind.h"
#include "base/callback.h"
#include "base/debug/alias.h"
#include "base/file_util.h"
#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/sys_string_conversions.h"
#include "build/build_config.h"
#include "content/browser/download/download_create_info.h"
#include "content/browser/download/download_file_manager.h"
#include "content/browser/download/download_item_impl.h"
#include "content/browser/download/download_stats.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/resource_dispatcher_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/download_manager_delegate.h"
#include "content/public/browser/download_persistent_store_info.h"
#include "content/public/browser/download_url_parameters.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents_delegate.h"
#include "net/base/load_flags.h"
#include "net/base/upload_data.h"

using content::BrowserThread;
using content::DownloadId;
using content::DownloadItem;
using content::DownloadPersistentStoreInfo;
using content::ResourceDispatcherHostImpl;
using content::WebContents;

namespace {

void BeginDownload(content::DownloadUrlParameters* params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // ResourceDispatcherHost{Base} is-not-a URLRequest::Delegate, and
  // DownloadUrlParameters can-not include resource_dispatcher_host_impl.h, so
  // we must down cast. RDHI is the only subclass of RDH as of 2012 May 4.
  content::ResourceDispatcherHostImpl* resource_dispatcher_host =
    static_cast<content::ResourceDispatcherHostImpl*>(
        params->resource_dispatcher_host());
  scoped_ptr<net::URLRequest> request(new net::URLRequest(
      params->url(), resource_dispatcher_host));
  request->set_referrer(params->referrer().spec());
  request->set_load_flags(request->load_flags() | params->load_flags());
  request->set_method(params->method());
  if (!params->post_body().empty())
    request->AppendBytesToUpload(params->post_body().data(),
                                 params->post_body().size());
  if (params->post_id() >= 0) {
    // The POST in this case does not have an actual body, and only works
    // when retrieving data from cache. This is done because we don't want
    // to do a re-POST without user consent, and currently don't have a good
    // plan on how to display the UI for that.
    DCHECK(params->prefer_cache());
    DCHECK(params->method() == "POST");
    scoped_refptr<net::UploadData> upload_data = new net::UploadData();
    upload_data->set_identifier(params->post_id());
    request->set_upload(upload_data);
  }
  for (content::DownloadUrlParameters::RequestHeadersType::const_iterator iter
           = params->request_headers_begin();
       iter != params->request_headers_end();
       ++iter) {
    request->SetExtraRequestHeaderByName(
        iter->first, iter->second, false/*overwrite*/);
  }
  resource_dispatcher_host->BeginDownload(
      request.Pass(),
      params->content_initiated(),
      params->resource_context(),
      params->render_process_host_id(),
      params->render_view_host_routing_id(),
      params->prefer_cache(),
      params->save_info(),
      params->callback());
}

class MapValueIteratorAdapter {
 public:
  explicit MapValueIteratorAdapter(
      base::hash_map<int64, DownloadItem*>::const_iterator iter)
    : iter_(iter) {
  }
  ~MapValueIteratorAdapter() {}

  DownloadItem* operator*() { return iter_->second; }

  MapValueIteratorAdapter& operator++() {
    ++iter_;
    return *this;
  }

  bool operator!=(const MapValueIteratorAdapter& that) const {
    return iter_ != that.iter_;
  }

 private:
  base::hash_map<int64, DownloadItem*>::const_iterator iter_;
  // Allow copy and assign.
};

void EnsureNoPendingDownloadsOnFile(scoped_refptr<DownloadFileManager> dfm,
                                    bool* result) {
  if (dfm->NumberOfActiveDownloads())
    *result = false;
  BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE, MessageLoop::QuitClosure());
}

void EnsureNoPendingDownloadJobsOnIO(bool* result) {
  scoped_refptr<DownloadFileManager> download_file_manager =
      ResourceDispatcherHostImpl::Get()->download_file_manager();
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&EnsureNoPendingDownloadsOnFile,
                 download_file_manager, result));
}

}  // namespace

namespace content {

// static
DownloadManager* DownloadManager::Create(
    content::DownloadManagerDelegate* delegate,
    net::NetLog* net_log) {
  return new DownloadManagerImpl(delegate, net_log);
}

bool DownloadManager::EnsureNoPendingDownloadsForTesting() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  bool result = true;
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&EnsureNoPendingDownloadJobsOnIO, &result));
  MessageLoop::current()->Run();
  return result;
}

}  // namespace content

DownloadManagerImpl::DownloadManagerImpl(
    content::DownloadManagerDelegate* delegate,
    net::NetLog* net_log)
        : shutdown_needed_(false),
          browser_context_(NULL),
          file_manager_(NULL),
          delegate_(delegate),
          net_log_(net_log) {
}

DownloadManagerImpl::~DownloadManagerImpl() {
  DCHECK(!shutdown_needed_);
}

DownloadId DownloadManagerImpl::GetNextId() {
  return delegate_->GetNextId();
}

bool DownloadManagerImpl::ShouldOpenDownload(DownloadItem* item) {
  return delegate_->ShouldOpenDownload(item);
}

bool DownloadManagerImpl::ShouldOpenFileBasedOnExtension(const FilePath& path) {
  return delegate_->ShouldOpenFileBasedOnExtension(path);
}

void DownloadManagerImpl::Shutdown() {
  VLOG(20) << __FUNCTION__ << "()"
           << " shutdown_needed_ = " << shutdown_needed_;
  if (!shutdown_needed_)
    return;
  shutdown_needed_ = false;

  FOR_EACH_OBSERVER(Observer, observers_, ManagerGoingDown(this));
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
      // (specifically, DownloadManager::DownloadRemoved only
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

  active_downloads_.clear();
  history_downloads_.clear();
  STLDeleteElements(&downloads_to_delete);

  // We'll have nothing more to report to the observers after this point.
  observers_.Clear();

  DCHECK(save_page_downloads_.empty());

  file_manager_ = NULL;
  delegate_->Shutdown();
}

void DownloadManagerImpl::GetTemporaryDownloads(
    const FilePath& dir_path, DownloadVector* result) {
  DCHECK(result);

  for (DownloadMap::iterator it = history_downloads_.begin();
       it != history_downloads_.end(); ++it) {
    if (it->second->IsTemporary() &&
        (dir_path.empty() || it->second->GetFullPath().DirName() == dir_path))
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

  // In test mode, there may be no ResourceDispatcherHostImpl.  In this case
  // it's safe to avoid setting |file_manager_| because we only call a small
  // set of functions, none of which need it.
  ResourceDispatcherHostImpl* rdh = ResourceDispatcherHostImpl::Get();
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

void DownloadManagerImpl::RestartDownload(int32 download_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DownloadItem* download = GetActiveDownloadItem(download_id);
  if (!download)
    return;

  VLOG(20) << __FUNCTION__ << "()"
           << " download = " << download->DebugString(true);

  FilePath suggested_path = download->GetSuggestedPath();

  if (download->PromptUserForSaveLocation()) {
    // We must ask the user for the place to put the download.
    WebContents* contents = download->GetWebContents();

    FilePath target_path;
    // If |download| is a potentially dangerous file, then |suggested_path|
    // contains the intermediate name instead of the final download
    // filename. The latter is GetTargetName().
    if (download->GetDangerType() !=
            content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS)
      target_path = suggested_path.DirName().Append(download->GetTargetName());
    else
      target_path = suggested_path;

    delegate_->ChooseDownloadPath(contents, target_path,
                                  download_id);
    FOR_EACH_OBSERVER(Observer, observers_,
                      SelectFileDialogDisplayed(this, download_id));
  } else {
    // No prompting for download, just continue with the suggested name.
    ContinueDownloadWithPath(download, suggested_path);
  }
}

content::BrowserContext* DownloadManagerImpl::GetBrowserContext() const {
  return browser_context_;
}

FilePath DownloadManagerImpl::LastDownloadPath() {
  return last_download_path_;
}

net::BoundNetLog DownloadManagerImpl::CreateDownloadItem(
    DownloadCreateInfo* info, const DownloadRequestHandle& request_handle) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  net::BoundNetLog bound_net_log =
      net::BoundNetLog::Make(net_log_, net::NetLog::SOURCE_DOWNLOAD);
  DownloadItem* download = new DownloadItemImpl(
      this, *info, new DownloadRequestHandle(request_handle),
      browser_context_->IsOffTheRecord(), bound_net_log);
  int32 download_id = info->download_id.local();

  DCHECK(!ContainsKey(active_downloads_, download_id));
  downloads_.insert(download);
  active_downloads_[download_id] = download;

  return bound_net_log;
}

DownloadItem* DownloadManagerImpl::CreateSavePackageDownloadItem(
    const FilePath& main_file_path,
    const GURL& page_url,
    bool is_otr,
    const std::string& mime_type,
    DownloadItem::Observer* observer) {
  net::BoundNetLog bound_net_log =
      net::BoundNetLog::Make(net_log_, net::NetLog::SOURCE_DOWNLOAD);
  DownloadItem* download = new DownloadItemImpl(
      this,
      main_file_path,
      page_url,
      is_otr,
      GetNextId(),
      mime_type,
      bound_net_log);

  download->AddObserver(observer);

  DCHECK(!ContainsKey(save_page_downloads_, download->GetId()));
  downloads_.insert(download);
  save_page_downloads_[download->GetId()] = download;

  // Will notify the observer in the callback.
  delegate_->AddItemToPersistentStore(download);

  return download;
}

// For non-safe downloads with no prompting, |chosen_file| is the intermediate
// path for saving the in-progress download. The final target filename for these
// is |download->GetTargetName()|.  For all other downloads (non-safe downloads
// for which we have prompted for a save location, and all safe downloads),
// |chosen_file| is the final target download path.
void DownloadManagerImpl::ContinueDownloadWithPath(
    DownloadItem* download, const FilePath& chosen_file) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(download);

  int32 download_id = download->GetId();

  DCHECK(ContainsKey(downloads_, download));
  DCHECK(ContainsKey(active_downloads_, download_id));

  // Make sure the initial file name is set only once.
  DCHECK(download->GetFullPath().empty());
  download->OnPathDetermined(chosen_file);

  VLOG(20) << __FUNCTION__ << "()"
           << " download = " << download->DebugString(true);

  // Rename to intermediate name.
  FilePath download_path;
  if (download->GetDangerType() !=
          content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS) {
    if (download->PromptUserForSaveLocation()) {
      // When we prompt the user, we overwrite the FullPath with what the user
      // wanted to use. Construct a file path using the previously determined
      // intermediate filename and the new path.
      // TODO(asanka): This can trample an in-progress download in the new
      // target directory if it was using the same intermediate name.
      FilePath file_name = download->GetSuggestedPath().BaseName();
      download_path = download->GetFullPath().DirName().Append(file_name);
    } else {
      // The download's name is already set to an intermediate name, so no need
      // to override.
      download_path = download->GetFullPath();
    }
  } else {
    // The download is a safe download.  We need to rename it to its
    // intermediate path.  The final name after user confirmation will be set
    // from DownloadItem::OnDownloadCompleting.
    download_path = delegate_->GetIntermediatePath(download->GetFullPath());
  }

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&DownloadFileManager::RenameInProgressDownloadFile,
                 file_manager_, download->GetGlobalId(),
                 download_path));

  download->Rename(download_path);

  delegate_->AddItemToPersistentStore(download);
}

void DownloadManagerImpl::UpdateDownload(int32 download_id,
                                         int64 bytes_so_far,
                                         int64 bytes_per_sec,
                                         const std::string& hash_state) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DownloadMap::iterator it = active_downloads_.find(download_id);
  if (it != active_downloads_.end()) {
    DownloadItem* download = it->second;
    if (download->IsInProgress()) {
      download->UpdateProgress(bytes_so_far, bytes_per_sec, hash_state);
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
  MaybeCompleteDownload(download);
}

void DownloadManagerImpl::AssertStateConsistent(DownloadItem* download) const {
  if (download->GetState() == DownloadItem::REMOVING) {
    DCHECK(!ContainsKey(downloads_, download));
    DCHECK(!ContainsKey(active_downloads_, download->GetId()));
    DCHECK(!ContainsKey(history_downloads_, download->GetDbHandle()));
    return;
  }

  // Should be in downloads_ if we're not REMOVING.
  CHECK(ContainsKey(downloads_, download));

  // Check history_downloads_ consistency.
  if (download->IsPersisted()) {
    CHECK(ContainsKey(history_downloads_, download->GetDbHandle()));
  } else {
    for (DownloadMap::const_iterator it = history_downloads_.begin();
         it != history_downloads_.end(); ++it) {
      DCHECK(it->second != download);
    }
  }

  int64 state = download->GetState();
  base::debug::Alias(&state);
  if (ContainsKey(active_downloads_, download->GetId())) {
    if (download->IsPersisted())
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
  if (!download->IsPersisted())
    return false;

  return true;
}

// When SavePackage downloads MHTML to GData (see
// SavePackageFilePickerChromeOS), GData calls MaybeCompleteDownload() like it
// does for non-SavePackage downloads, but SavePackage downloads never satisfy
// IsDownloadReadyForCompletion(). GDataDownloadObserver manually calls
// DownloadItem::UpdateObservers() when the upload completes so that SavePackage
// notices that the upload has completed and runs its normal Finish() pathway.
// MaybeCompleteDownload() is never the mechanism by which SavePackage completes
// downloads. SavePackage always uses its own Finish() to mark downloads
// complete.

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
  // have all data, have a history handle, (validated or safe).
  DCHECK(download->IsInProgress());
  DCHECK_NE(DownloadItem::DANGEROUS, download->GetSafetyState());
  DCHECK(download->AllDataSaved());
  DCHECK(download->IsPersisted());
  DCHECK_EQ(1u, history_downloads_.count(download->GetDbHandle()));

  // Give the delegate a chance to override.  It's ok to keep re-setting the
  // delegate's |complete_callback| cb as long as there isn't another call-point
  // trying to set it to a different cb.  TODO(benjhayden): Change the callback
  // to point directly to the item instead of |this| when DownloadItem supports
  // weak-ptrs.
  if (!delegate_->ShouldCompleteDownload(download, base::Bind(
          &DownloadManagerImpl::MaybeCompleteDownloadById,
          this, download->GetId())))
    return;

  VLOG(20) << __FUNCTION__ << "()" << " executing: download = "
           << download->DebugString(false);

  delegate_->UpdateItemInPersistentStore(download);

  // Finish the download.
  download->OnDownloadCompleting(file_manager_);
}

void DownloadManagerImpl::MaybeCompleteDownloadById(int download_id) {
  DownloadItem* download_item = GetActiveDownload(download_id);
  if (download_item != NULL)
    MaybeCompleteDownload(download_item);
}

void DownloadManagerImpl::DownloadCompleted(DownloadItem* download) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(download);
  delegate_->UpdateItemInPersistentStore(download);
  active_downloads_.erase(download->GetId());
  AssertStateConsistent(download);
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

  if (item->GetDangerType() == content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS ||
      item->PromptUserForSaveLocation()) {
    DCHECK_EQ(0, uniquifier)
        << "We should not uniquify user supplied filenames or safe filenames "
           "that have already been uniquified.";
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

void DownloadManagerImpl::DownloadCancelled(DownloadItem* download) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  VLOG(20) << __FUNCTION__ << "()"
           << " download = " << download->DebugString(true);

  RemoveFromActiveList(download);
  // This function is called from the DownloadItem, so DI state
  // should already have been updated.
  AssertStateConsistent(download);

  if (file_manager_)
    download->OffThreadCancel(file_manager_);
}

void DownloadManagerImpl::OnDownloadInterrupted(
    int32 download_id,
    int64 size,
    const std::string& hash_state,
    content::DownloadInterruptReason reason) {
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
  download->Interrupted(size, hash_state, reason);
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
  if (download->IsPersisted()) {
    active_downloads_.erase(download->GetId());
    delegate_->UpdateItemInPersistentStore(download);
  }
}

bool DownloadManagerImpl::GenerateFileHash() {
  return delegate_->GenerateFileHash();
}

content::DownloadManagerDelegate* DownloadManagerImpl::delegate() const {
  return delegate_;
}

void DownloadManagerImpl::SetDownloadManagerDelegate(
    content::DownloadManagerDelegate* delegate) {
  delegate_ = delegate;
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

void DownloadManagerImpl::DownloadRemoved(DownloadItem* download) {
  if (history_downloads_.find(download->GetDbHandle()) ==
      history_downloads_.end())
    return;

  // Make history update.
  delegate_->RemoveItemFromPersistentStore(download);

  // Remove from our tables and delete.
  int downloads_count = RemoveDownloadItems(DownloadVector(1, download));
  DCHECK_EQ(1, downloads_count);
}

int DownloadManagerImpl::RemoveDownloadsBetween(base::Time remove_begin,
                                                base::Time remove_end) {
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
      AssertStateConsistent(download);
      pending_deletes.push_back(download);
    }
  }
  return RemoveDownloadItems(pending_deletes);
}

int DownloadManagerImpl::RemoveDownloads(base::Time remove_begin) {
  return RemoveDownloadsBetween(remove_begin, base::Time());
}

int DownloadManagerImpl::RemoveAllDownloads() {
  download_stats::RecordClearAllSize(history_downloads_.size());
  // The null times make the date range unbounded.
  return RemoveDownloadsBetween(base::Time(), base::Time());
}

void DownloadManagerImpl::DownloadUrl(
    scoped_ptr<content::DownloadUrlParameters> params) {
  if (params->post_id() >= 0) {
    // Check this here so that the traceback is more useful.
    DCHECK(params->prefer_cache());
    DCHECK(params->method() == "POST");
  }
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, base::Bind(
      &BeginDownload, base::Owned(params.release())));
}

void DownloadManagerImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
  // TODO: It is the responsibility of the observers to query the
  // DownloadManager. Remove the following call from here and update all
  // observers.
  observer->ModelChanged(this);
}

void DownloadManagerImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void DownloadManagerImpl::FileSelected(const FilePath& path,
                                       int32 download_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DownloadItem* download = GetActiveDownloadItem(download_id);
  if (!download)
    return;
  VLOG(20) << __FUNCTION__ << "()" << " path = \"" << path.value() << "\""
            << " download = " << download->DebugString(true);

  // Retain the last directory that was picked by the user. Exclude temporary
  // downloads since the path likely points at the location of a temporary file.
  if (download->PromptUserForSaveLocation() && !download->IsTemporary())
    last_download_path_ = path.DirName();

  // Make sure the initial file name is set only once.
  ContinueDownloadWithPath(download, path);
}

void DownloadManagerImpl::FileSelectionCanceled(int32 download_id) {
  // The user didn't pick a place to save the file, so need to cancel the
  // download that's already in progress to the temporary location.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DownloadItem* download = GetActiveDownloadItem(download_id);
  if (!download)
    return;

  VLOG(20) << __FUNCTION__ << "()"
           << " download = " << download->DebugString(true);

  download->Cancel(true);
}

// Operations posted to us from the history service ----------------------------

// The history service has retrieved all download entries. 'entries' contains
// 'DownloadPersistentStoreInfo's in sorted order (by ascending start_time).
void DownloadManagerImpl::OnPersistentStoreQueryComplete(
    std::vector<DownloadPersistentStoreInfo>* entries) {
  for (size_t i = 0; i < entries->size(); ++i) {
    int64 db_handle = entries->at(i).db_handle;
    base::debug::Alias(&db_handle);
    DCHECK(!ContainsKey(history_downloads_, db_handle));

    net::BoundNetLog bound_net_log =
        net::BoundNetLog::Make(net_log_, net::NetLog::SOURCE_DOWNLOAD);
    DownloadItem* download = new DownloadItemImpl(
        this, GetNextId(), entries->at(i), bound_net_log);
    downloads_.insert(download);
    history_downloads_[download->GetDbHandle()] = download;
    VLOG(20) << __FUNCTION__ << "()" << i << ">"
             << " download = " << download->DebugString(true);
  }
  NotifyModelChanged();
  CheckForHistoryFilesRemoval();
}

void DownloadManagerImpl::AddDownloadItemToHistory(DownloadItem* download,
                                                   int64 db_handle) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_NE(DownloadItem::kUninitializedHandle, db_handle);

  download_stats::RecordHistorySize(history_downloads_.size());

  DCHECK(!download->IsPersisted());
  download->SetDbHandle(db_handle);
  download->SetIsPersisted();

  DCHECK(!ContainsKey(history_downloads_, download->GetDbHandle()));
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

  int32 matching_item_download_id
      = (ContainsKey(history_downloads_, db_handle) ?
         history_downloads_[db_handle]->GetId() : 0);
  base::debug::Alias(&matching_item_download_id);

  DCHECK(!ContainsKey(history_downloads_, db_handle));

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
    DCHECK(download->IsCancelled());
    active_downloads_.erase(download_id);
    delegate_->UpdateItemInPersistentStore(download);
    download->UpdateObservers();
  }
}

void DownloadManagerImpl::ShowDownloadInBrowser(DownloadItem* download) {
  // The 'contents' may no longer exist if the user closed the contents before
  // we get this start completion event.
  WebContents* content = download->GetWebContents();

  // If the contents no longer exists, we ask the embedder to suggest another
  // contents.
  if (!content)
    content = delegate_->GetAlternativeWebContentsToNotifyForDownload();

  if (content && content->GetDelegate())
    content->GetDelegate()->OnStartDownload(content, download);
}

int DownloadManagerImpl::InProgressCount() const {
  // Don't use active_downloads_.count() because Cancel() leaves items in
  // active_downloads_ if they haven't made it into the persistent store yet.
  // Need to actually look at each item's state.
  int count = 0;
  for (DownloadMap::const_iterator it = active_downloads_.begin();
       it != active_downloads_.end(); ++it) {
    DownloadItem* item = it->second;
    if (item->IsInProgress())
      ++count;
  }
  return count;
}

// Clears the last download path, used to initialize "save as" dialogs.
void DownloadManagerImpl::ClearLastDownloadPath() {
  last_download_path_ = FilePath();
}

void DownloadManagerImpl::NotifyModelChanged() {
  FOR_EACH_OBSERVER(Observer, observers_, ModelChanged(this));
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
  if (ContainsKey(active_downloads_, download_id))
    return active_downloads_[download_id];
  return NULL;
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

  DCHECK(!ContainsKey(history_downloads_, db_handle));

  AddDownloadItemToHistory(download, db_handle);

  // Finalize this download if it finished before the history callback.
  if (!download->IsInProgress())
    SavePageDownloadFinished(download);
}

void DownloadManagerImpl::SavePageDownloadFinished(DownloadItem* download) {
  if (download->IsPersisted()) {
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

void DownloadManagerImpl::DownloadOpened(DownloadItem* download) {
  delegate_->UpdateItemInPersistentStore(download);
  int num_unopened = 0;
  for (DownloadMap::iterator it = history_downloads_.begin();
       it != history_downloads_.end(); ++it) {
    if (it->second->IsComplete() && !it->second->GetOpened())
      ++num_unopened;
  }
  download_stats::RecordOpensOutstanding(num_unopened);
}

void DownloadManagerImpl::SetFileManagerForTesting(
      DownloadFileManager* file_manager) {
  file_manager_ = file_manager;
}
