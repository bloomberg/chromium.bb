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
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/stringprintf.h"
#include "base/supports_user_data.h"
#include "base/synchronization/lock.h"
#include "base/sys_string_conversions.h"
#include "build/build_config.h"
#include "content/browser/download/byte_stream.h"
#include "content/browser/download/download_create_info.h"
#include "content/browser/download/download_file_factory.h"
#include "content/browser/download/download_item_factory.h"
#include "content/browser/download/download_item_impl.h"
#include "content/browser/download/download_stats.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/download_manager_delegate.h"
#include "content/public/browser/download_url_parameters.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/referrer.h"
#include "net/base/load_flags.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_data_stream.h"
#include "net/url_request/url_request_context.h"
#include "webkit/glue/webkit_glue.h"

namespace content {
namespace {

void BeginDownload(scoped_ptr<DownloadUrlParameters> params,
                   DownloadId download_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // ResourceDispatcherHost{Base} is-not-a URLRequest::Delegate, and
  // DownloadUrlParameters can-not include resource_dispatcher_host_impl.h, so
  // we must down cast. RDHI is the only subclass of RDH as of 2012 May 4.
  scoped_ptr<net::URLRequest> request(
      params->resource_context()->GetRequestContext()->CreateRequest(
          params->url(), NULL));
  request->set_referrer(params->referrer().url.spec());
  webkit_glue::ConfigureURLRequestForReferrerPolicy(
      request.get(), params->referrer().policy);
  request->set_load_flags(request->load_flags() | params->load_flags());
  request->set_method(params->method());
  if (!params->post_body().empty()) {
    const std::string& body = params->post_body();
    scoped_ptr<net::UploadElementReader> reader(
        net::UploadOwnedBytesElementReader::CreateWithString(body));
    request->set_upload(make_scoped_ptr(
        net::UploadDataStream::CreateWithReader(reader.Pass(), 0)));
  }
  if (params->post_id() >= 0) {
    // The POST in this case does not have an actual body, and only works
    // when retrieving data from cache. This is done because we don't want
    // to do a re-POST without user consent, and currently don't have a good
    // plan on how to display the UI for that.
    DCHECK(params->prefer_cache());
    DCHECK(params->method() == "POST");
    ScopedVector<net::UploadElementReader> element_readers;
    request->set_upload(make_scoped_ptr(
        new net::UploadDataStream(&element_readers, params->post_id())));
  }

  // If we're not at the beginning of the file, retrieve only the remaining
  // portion.
  if (params->offset() > 0) {
    request->SetExtraRequestHeaderByName(
        "Range",
        StringPrintf("bytes=%" PRId64 "-", params->offset()),
        true);

    // If we've asked for a range, we want to make sure that we only
    // get that range if our current copy of the information is good.
    if (!params->last_modified().empty()) {
      request->SetExtraRequestHeaderByName("If-Unmodified-Since",
                                           params->last_modified(),
                                           true);
    }
    if (!params->etag().empty()) {
      request->SetExtraRequestHeaderByName("If-Match", params->etag(), true);
    }
  }

  for (DownloadUrlParameters::RequestHeadersType::const_iterator iter
           = params->request_headers_begin();
       iter != params->request_headers_end();
       ++iter) {
    request->SetExtraRequestHeaderByName(
        iter->first, iter->second, false/*overwrite*/);
  }

  scoped_ptr<DownloadSaveInfo> save_info(new DownloadSaveInfo());
  save_info->file_path = params->file_path();
  save_info->suggested_name = params->suggested_name();
  save_info->offset = params->offset();
  save_info->hash_state = params->hash_state();
  save_info->prompt_for_save_location = params->prompt();
  save_info->file_stream = params->GetFileStream();

  params->resource_dispatcher_host()->BeginDownload(
      request.Pass(),
      params->content_initiated(),
      params->resource_context(),
      params->render_process_host_id(),
      params->render_view_host_routing_id(),
      params->prefer_cache(),
      save_info.Pass(),
      download_id,
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

void EnsureNoPendingDownloadJobsOnFile(bool* result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  *result = (DownloadFile::GetNumberOfDownloadFiles() == 0);
  BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE, MessageLoop::QuitClosure());
}

class DownloadItemFactoryImpl : public DownloadItemFactory {
 public:
    DownloadItemFactoryImpl() {}
    virtual ~DownloadItemFactoryImpl() {}

    virtual DownloadItemImpl* CreatePersistedItem(
        DownloadItemImplDelegate* delegate,
        DownloadId download_id,
        const FilePath& path,
        const GURL& url,
        const GURL& referrer_url,
        const base::Time& start_time,
        const base::Time& end_time,
        int64 received_bytes,
        int64 total_bytes,
        DownloadItem::DownloadState state,
        bool opened,
        const net::BoundNetLog& bound_net_log) OVERRIDE {
      return new DownloadItemImpl(
          delegate,
          download_id,
          path,
          url,
          referrer_url,
          start_time,
          end_time,
          received_bytes,
          total_bytes,
          state,
          opened,
          bound_net_log);
    }

    virtual DownloadItemImpl* CreateActiveItem(
        DownloadItemImplDelegate* delegate,
        const DownloadCreateInfo& info,
        const net::BoundNetLog& bound_net_log) OVERRIDE {
      return new DownloadItemImpl(delegate, info, bound_net_log);
    }

    virtual DownloadItemImpl* CreateSavePageItem(
        DownloadItemImplDelegate* delegate,
        const FilePath& path,
        const GURL& url,
        DownloadId download_id,
        const std::string& mime_type,
        const net::BoundNetLog& bound_net_log) OVERRIDE {
      return new DownloadItemImpl(delegate, path, url, download_id,
                                  mime_type, bound_net_log);
    }
};

}  // namespace

DownloadManagerImpl::DownloadManagerImpl(
    net::NetLog* net_log)
    : item_factory_(new DownloadItemFactoryImpl()),
      file_factory_(new DownloadFileFactory()),
      history_size_(0),
      shutdown_needed_(false),
      browser_context_(NULL),
      delegate_(NULL),
      net_log_(net_log) {
}

DownloadManagerImpl::~DownloadManagerImpl() {
  DCHECK(!shutdown_needed_);
}

DownloadId DownloadManagerImpl::GetNextId() {
  DownloadId id;
  if (delegate_)
   id = delegate_->GetNextId();
  if (!id.IsValid()) {
    static int next_id;
    id = DownloadId(browser_context_, ++next_id);
  }

  return id;
}

void DownloadManagerImpl::DetermineDownloadTarget(
    DownloadItemImpl* item, const DownloadTargetCallback& callback) {
  // Note that this next call relies on
  // DownloadItemImplDelegate::DownloadTargetCallback and
  // DownloadManagerDelegate::DownloadTargetCallback having the same
  // type.  If the types ever diverge, gasket code will need to
  // be written here.
  if (!delegate_ || !delegate_->DetermineDownloadTarget(item, callback)) {
    FilePath target_path = item->GetForcedFilePath();
    // TODO(asanka): Determine a useful path if |target_path| is empty.
    callback.Run(target_path,
                 DownloadItem::TARGET_DISPOSITION_OVERWRITE,
                 DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
                 target_path);
  }
}

bool DownloadManagerImpl::ShouldCompleteDownload(
    DownloadItemImpl* item, const base::Closure& complete_callback) {
  if (!delegate_ ||
      delegate_->ShouldCompleteDownload(item, complete_callback)) {
    return true;
  }
  // Otherwise, the delegate has accepted responsibility to run the
  // callback when the download is ready for completion.
  return false;
}

bool DownloadManagerImpl::ShouldOpenFileBasedOnExtension(const FilePath& path) {
  if (!delegate_)
    return false;

  return delegate_->ShouldOpenFileBasedOnExtension(path);
}

bool DownloadManagerImpl::ShouldOpenDownload(
    DownloadItemImpl* item, const ShouldOpenDownloadCallback& callback) {
  if (!delegate_)
    return true;

  // Relies on DownloadItemImplDelegate::ShouldOpenDownloadCallback and
  // DownloadManagerDelegate::DownloadOpenDelayedCallback "just happening"
  // to have the same type :-}.
  return delegate_->ShouldOpenDownload(item, callback);
}

void DownloadManagerImpl::SetDelegate(DownloadManagerDelegate* delegate) {
  delegate_ = delegate;
}

DownloadManagerDelegate* DownloadManagerImpl::GetDelegate() const {
  return delegate_;
}

void DownloadManagerImpl::Shutdown() {
  VLOG(20) << __FUNCTION__ << "()"
           << " shutdown_needed_ = " << shutdown_needed_;
  if (!shutdown_needed_)
    return;
  shutdown_needed_ = false;

  FOR_EACH_OBSERVER(Observer, observers_, ManagerGoingDown(this));
  // TODO(benjhayden): Consider clearing observers_.

  // Go through all downloads in downloads_.  Dangerous ones we need to
  // remove on disk, and in progress ones we need to cancel.
  for (DownloadMap::iterator it = downloads_.begin(); it != downloads_.end();) {
    DownloadItemImpl* download = it->second;

    // Save iterator from potential erases in this set done by called code.
    // Iterators after an erasure point are still valid for lists and
    // associative containers such as sets.
    it++;

    if (download->IsDangerous() && download->IsPartialDownload()) {
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
    }
  }

  // At this point, all dangerous downloads have had their files removed
  // and all in progress downloads have been cancelled.  We can now delete
  // anything left.

  STLDeleteValues(&downloads_);
  downloads_.clear();

  // We'll have nothing more to report to the observers after this point.
  observers_.Clear();

  if (delegate_)
    delegate_->Shutdown();
  delegate_ = NULL;
}

bool DownloadManagerImpl::Init(BrowserContext* browser_context) {
  DCHECK(browser_context);
  DCHECK(!shutdown_needed_)  << "DownloadManager already initialized.";
  shutdown_needed_ = true;

  browser_context_ = browser_context;

  return true;
}

DownloadItem* DownloadManagerImpl::StartDownload(
    scoped_ptr<DownloadCreateInfo> info,
    scoped_ptr<ByteStreamReader> stream) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  FilePath default_download_directory;
  if (delegate_) {
    FilePath website_save_directory;      // Unused
    bool skip_dir_check = false;          // Unused
    delegate_->GetSaveDir(GetBrowserContext(), &website_save_directory,
                          &default_download_directory, &skip_dir_check);
  }

  // We create the DownloadItem before the DownloadFile because the
  // DownloadItem already needs to handle a state in which there is
  // no associated DownloadFile (history downloads, !IN_PROGRESS downloads)
  DownloadItemImpl* download = GetOrCreateDownloadItem(info.get());
  scoped_ptr<DownloadFile> download_file(
      file_factory_->CreateFile(
          info->save_info.Pass(), default_download_directory,
          info->url(), info->referrer_url,
          delegate_->GenerateFileHash(),
          stream.Pass(), download->GetBoundNetLog(),
          download->DestinationObserverAsWeakPtr()));
  scoped_ptr<DownloadRequestHandleInterface> req_handle(
      new DownloadRequestHandle(info->request_handle));
  download->Start(download_file.Pass(), req_handle.Pass());

  // Delay notification until after Start() so that download_file is bound
  // to download and all the usual setters (e.g. Cancel) work.
  FOR_EACH_OBSERVER(Observer, observers_, OnDownloadCreated(this, download));

  return download;
}

void DownloadManagerImpl::CheckForHistoryFilesRemoval() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  for (DownloadMap::iterator it = downloads_.begin();
       it != downloads_.end(); ++it) {
    DownloadItemImpl* item = it->second;
    CheckForFileRemoval(item);
  }
}

void DownloadManagerImpl::CheckForFileRemoval(DownloadItemImpl* download_item) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (download_item->IsComplete() &&
      !download_item->GetFileExternallyRemoved()) {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&DownloadManagerImpl::CheckForFileRemovalOnFileThread,
                   this, download_item->GetId(),
                   download_item->GetTargetFilePath()));
  }
}

void DownloadManagerImpl::CheckForFileRemovalOnFileThread(
    int32 download_id, const FilePath& path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  if (!file_util::PathExists(path)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&DownloadManagerImpl::OnFileRemovalDetected,
                   this,
                   download_id));
  }
}

void DownloadManagerImpl::OnFileRemovalDetected(int32 download_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (ContainsKey(downloads_, download_id))
    downloads_[download_id]->OnDownloadedFileRemoved();
}

BrowserContext* DownloadManagerImpl::GetBrowserContext() const {
  return browser_context_;
}

DownloadItemImpl* DownloadManagerImpl::GetOrCreateDownloadItem(
    DownloadCreateInfo* info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!info->download_id.IsValid())
    info->download_id = GetNextId();

  DownloadItemImpl* download = NULL;
  if (ContainsKey(downloads_, info->download_id.local())) {
    // Resuming an existing download.
    download = downloads_[info->download_id.local()];
    DCHECK(download->IsInterrupted());
  } else {
    // New download
    net::BoundNetLog bound_net_log =
        net::BoundNetLog::Make(net_log_, net::NetLog::SOURCE_DOWNLOAD);
    download = item_factory_->CreateActiveItem(this, *info, bound_net_log);
    downloads_[download->GetId()] = download;
  }

  return download;
}

DownloadItemImpl* DownloadManagerImpl::CreateSavePackageDownloadItem(
    const FilePath& main_file_path,
    const GURL& page_url,
    const std::string& mime_type,
    DownloadItem::Observer* observer) {
  net::BoundNetLog bound_net_log =
      net::BoundNetLog::Make(net_log_, net::NetLog::SOURCE_DOWNLOAD);
  DownloadItemImpl* download_item = item_factory_->CreateSavePageItem(
      this,
      main_file_path,
      page_url,
      GetNextId(),
      mime_type,
      bound_net_log);

  download_item->AddObserver(observer);
  DCHECK(!ContainsKey(downloads_, download_item->GetId()));
  downloads_[download_item->GetId()] = download_item;
  FOR_EACH_OBSERVER(Observer, observers_, OnDownloadCreated(
      this, download_item));

  // TODO(asanka): Make the ui an observer.
  ShowDownloadInBrowser(download_item);

  return download_item;
}

void DownloadManagerImpl::CancelDownload(int32 download_id) {
  DownloadItem* download = GetDownload(download_id);
  if (!download || !download->IsInProgress())
    return;
  download->Cancel(true);
}

// Resume a download of a specific URL. We send the request to the
// ResourceDispatcherHost, and let it send us responses like a regular
// download.
void DownloadManagerImpl::ResumeInterruptedDownload(
    scoped_ptr<content::DownloadUrlParameters> params,
    content::DownloadId id) {
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&BeginDownload, base::Passed(params.Pass()), id));
}

void DownloadManagerImpl::SetDownloadItemFactoryForTesting(
    scoped_ptr<DownloadItemFactory> item_factory) {
  item_factory_ = item_factory.Pass();
}

void DownloadManagerImpl::SetDownloadFileFactoryForTesting(
    scoped_ptr<DownloadFileFactory> file_factory) {
  file_factory_ = file_factory.Pass();
}

DownloadFileFactory* DownloadManagerImpl::GetDownloadFileFactoryForTesting() {
  return file_factory_.get();
}

int DownloadManagerImpl::RemoveDownloadItems(
    const DownloadItemImplVector& pending_deletes) {
  if (pending_deletes.empty())
    return 0;

  // Delete from internal maps.
  for (DownloadItemImplVector::const_iterator it = pending_deletes.begin();
       it != pending_deletes.end();
       ++it) {
    DownloadItemImpl* download = *it;
    DCHECK(download);
    int32 download_id = download->GetId();
    delete download;
    downloads_.erase(download_id);
  }
  return static_cast<int>(pending_deletes.size());
}

void DownloadManagerImpl::DownloadRemoved(DownloadItemImpl* download) {
  if (!download ||
      downloads_.find(download->GetId()) == downloads_.end())
    return;

  // Remove from our tables and delete.
  int downloads_count =
      RemoveDownloadItems(DownloadItemImplVector(1, download));
  DCHECK_EQ(1, downloads_count);
}

int DownloadManagerImpl::RemoveDownloadsBetween(base::Time remove_begin,
                                                base::Time remove_end) {
  DownloadItemImplVector pending_deletes;
  for (DownloadMap::const_iterator it = downloads_.begin();
      it != downloads_.end();
      ++it) {
    DownloadItemImpl* download = it->second;
    if (download->GetStartTime() >= remove_begin &&
        (remove_end.is_null() || download->GetStartTime() < remove_end) &&
        (download->IsComplete() || download->IsCancelled())) {
      download->NotifyRemoved();
      pending_deletes.push_back(download);
    }
  }
  return RemoveDownloadItems(pending_deletes);
}

int DownloadManagerImpl::RemoveDownloads(base::Time remove_begin) {
  return RemoveDownloadsBetween(remove_begin, base::Time());
}

int DownloadManagerImpl::RemoveAllDownloads() {
  // The null times make the date range unbounded.
  int num_deleted = RemoveDownloadsBetween(base::Time(), base::Time());
  RecordClearAllSize(num_deleted);
  return num_deleted;
}

void DownloadManagerImpl::DownloadUrl(
    scoped_ptr<DownloadUrlParameters> params) {
  if (params->post_id() >= 0) {
    // Check this here so that the traceback is more useful.
    DCHECK(params->prefer_cache());
    DCHECK(params->method() == "POST");
  }
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, base::Bind(
      &BeginDownload, base::Passed(&params), DownloadId()));
}

void DownloadManagerImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DownloadManagerImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

DownloadItem* DownloadManagerImpl::CreateDownloadItem(
    const FilePath& path,
    const GURL& url,
    const GURL& referrer_url,
    const base::Time& start_time,
    const base::Time& end_time,
    int64 received_bytes,
    int64 total_bytes,
    DownloadItem::DownloadState state,
    bool opened) {
  DownloadItemImpl* item = item_factory_->CreatePersistedItem(
      this,
      GetNextId(),
      path,
      url,
      referrer_url,
      start_time,
      end_time,
      received_bytes,
      total_bytes,
      state,
      opened,
      net::BoundNetLog::Make(net_log_, net::NetLog::SOURCE_DOWNLOAD));
  DCHECK(!ContainsKey(downloads_, item->GetId()));
  downloads_[item->GetId()] = item;
  FOR_EACH_OBSERVER(Observer, observers_, OnDownloadCreated(this, item));
  VLOG(20) << __FUNCTION__ << "() download = " << item->DebugString(true);
  return item;
}

// TODO(asanka) Move into an observer.
void DownloadManagerImpl::ShowDownloadInBrowser(DownloadItemImpl* download) {
  // The 'contents' may no longer exist if the user closed the contents before
  // we get this start completion event.
  WebContents* content = download->GetWebContents();

  // If the contents no longer exists, we ask the embedder to suggest another
  // contents.
  if (!content && delegate_)
    content = delegate_->GetAlternativeWebContentsToNotifyForDownload();

  if (content && content->GetDelegate())
    content->GetDelegate()->OnStartDownload(content, download);
}

int DownloadManagerImpl::InProgressCount() const {
  int count = 0;
  for (DownloadMap::const_iterator it = downloads_.begin();
       it != downloads_.end(); ++it) {
    if (it->second->IsInProgress())
      ++count;
  }
  return count;
}

DownloadItem* DownloadManagerImpl::GetDownload(int download_id) {
  return ContainsKey(downloads_, download_id) ? downloads_[download_id] : NULL;
}

void DownloadManagerImpl::GetAllDownloads(DownloadVector* downloads) {
  for (DownloadMap::iterator it = downloads_.begin();
       it != downloads_.end(); ++it) {
    downloads->push_back(it->second);
  }
}

void DownloadManagerImpl::DownloadOpened(DownloadItemImpl* download) {
  int num_unopened = 0;
  for (DownloadMap::iterator it = downloads_.begin();
       it != downloads_.end(); ++it) {
    DownloadItemImpl* item = it->second;
    if (item->IsComplete() &&
        !item->GetOpened())
      ++num_unopened;
  }
  RecordOpensOutstanding(num_unopened);
}

}  // namespace content
