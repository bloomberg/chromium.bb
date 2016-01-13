// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_manager_impl.h"

#include <iterator>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/debug/alias.h"
#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/supports_user_data.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "content/browser/byte_stream.h"
#include "content/browser/download/download_create_info.h"
#include "content/browser/download/download_file_factory.h"
#include "content/browser/download/download_item_factory.h"
#include "content/browser/download/download_item_impl.h"
#include "content/browser/download/download_stats.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
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
#include "net/base/elements_upload_data_stream.h"
#include "net/base/load_flags.h"
#include "net/base/request_priority.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/url_request/url_request_context.h"
#include "url/origin.h"

namespace content {
namespace {

scoped_ptr<UrlDownloader, BrowserThread::DeleteOnIOThread> BeginDownload(
    scoped_ptr<DownloadUrlParameters> params,
    uint32_t download_id,
    base::WeakPtr<DownloadManagerImpl> download_manager) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // ResourceDispatcherHost{Base} is-not-a URLRequest::Delegate, and
  // DownloadUrlParameters can-not include resource_dispatcher_host_impl.h, so
  // we must down cast. RDHI is the only subclass of RDH as of 2012 May 4.
  scoped_ptr<net::URLRequest> request(
      params->resource_context()->GetRequestContext()->CreateRequest(
          params->url(), net::DEFAULT_PRIORITY, NULL));
  request->set_method(params->method());
  if (!params->post_body().empty()) {
    const std::string& body = params->post_body();
    scoped_ptr<net::UploadElementReader> reader(
        net::UploadOwnedBytesElementReader::CreateWithString(body));
    request->set_upload(
        net::ElementsUploadDataStream::CreateWithReader(std::move(reader), 0));
  }
  if (params->post_id() >= 0) {
    // The POST in this case does not have an actual body, and only works
    // when retrieving data from cache. This is done because we don't want
    // to do a re-POST without user consent, and currently don't have a good
    // plan on how to display the UI for that.
    DCHECK(params->prefer_cache());
    DCHECK_EQ("POST", params->method());
    std::vector<scoped_ptr<net::UploadElementReader>> element_readers;
    request->set_upload(make_scoped_ptr(new net::ElementsUploadDataStream(
        std::move(element_readers), params->post_id())));
  }

  // If we're not at the beginning of the file, retrieve only the remaining
  // portion.
  bool has_last_modified = !params->last_modified().empty();
  bool has_etag = !params->etag().empty();

  // If we've asked for a range, we want to make sure that we only
  // get that range if our current copy of the information is good.
  // We shouldn't be asked to continue if we don't have a verifier.
  DCHECK(params->offset() == 0 || has_etag || has_last_modified);

  if (params->offset() > 0 && (has_etag || has_last_modified)) {
    request->SetExtraRequestHeaderByName(
        "Range",
        base::StringPrintf("bytes=%" PRId64 "-", params->offset()),
        true);

    // In accordance with RFC 2616 Section 14.27, use If-Range to specify that
    // the server return the entire entity if the validator doesn't match.
    // Last-Modified can be used in the absence of ETag as a validator if the
    // response headers satisfied the HttpUtil::HasStrongValidators() predicate.
    //
    // This function assumes that HasStrongValidators() was true and that the
    // ETag and Last-Modified header values supplied are valid.
    request->SetExtraRequestHeaderByName(
        "If-Range", has_etag ? params->etag() : params->last_modified(), true);
  }

  for (DownloadUrlParameters::RequestHeadersType::const_iterator iter
           = params->request_headers_begin();
       iter != params->request_headers_end();
       ++iter) {
    request->SetExtraRequestHeaderByName(
        iter->first, iter->second, false /*overwrite*/);
  }

  scoped_ptr<DownloadSaveInfo> save_info(new DownloadSaveInfo());
  save_info->file_path = params->file_path();
  save_info->suggested_name = params->suggested_name();
  save_info->offset = params->offset();
  save_info->hash_state = params->hash_state();
  save_info->prompt_for_save_location = params->prompt();
  save_info->file = params->GetFile();

  if (params->render_process_host_id() != -1) {
    ResourceDispatcherHost::Get()->BeginDownload(
        std::move(request), params->referrer(), params->content_initiated(),
        params->resource_context(), params->render_process_host_id(),
        params->render_view_host_routing_id(),
        params->render_frame_host_routing_id(), params->prefer_cache(),
        params->do_not_prompt_for_login(), std::move(save_info), download_id,
        params->callback());
    return nullptr;
  }
  return scoped_ptr<UrlDownloader, BrowserThread::DeleteOnIOThread>(
      UrlDownloader::BeginDownload(download_manager, std::move(request),
                                   params->referrer(), params->prefer_cache(),
                                   std::move(save_info), download_id,
                                   params->callback())
          .release());
}

class DownloadItemFactoryImpl : public DownloadItemFactory {
 public:
  DownloadItemFactoryImpl() {}
  ~DownloadItemFactoryImpl() override {}

  DownloadItemImpl* CreatePersistedItem(
      DownloadItemImplDelegate* delegate,
      uint32_t download_id,
      const base::FilePath& current_path,
      const base::FilePath& target_path,
      const std::vector<GURL>& url_chain,
      const GURL& referrer_url,
      const std::string& mime_type,
      const std::string& original_mime_type,
      const base::Time& start_time,
      const base::Time& end_time,
      const std::string& etag,
      const std::string& last_modified,
      int64_t received_bytes,
      int64_t total_bytes,
      DownloadItem::DownloadState state,
      DownloadDangerType danger_type,
      DownloadInterruptReason interrupt_reason,
      bool opened,
      const net::BoundNetLog& bound_net_log) override {
    return new DownloadItemImpl(
        delegate,
        download_id,
        current_path,
        target_path,
        url_chain,
        referrer_url,
        mime_type,
        original_mime_type,
        start_time,
        end_time,
        etag,
        last_modified,
        received_bytes,
        total_bytes,
        state,
        danger_type,
        interrupt_reason,
        opened,
        bound_net_log);
  }

  DownloadItemImpl* CreateActiveItem(
      DownloadItemImplDelegate* delegate,
      uint32_t download_id,
      const DownloadCreateInfo& info,
      const net::BoundNetLog& bound_net_log) override {
    return new DownloadItemImpl(delegate, download_id, info, bound_net_log);
  }

  DownloadItemImpl* CreateSavePageItem(
      DownloadItemImplDelegate* delegate,
      uint32_t download_id,
      const base::FilePath& path,
      const GURL& url,
      const std::string& mime_type,
      scoped_ptr<DownloadRequestHandleInterface> request_handle,
      const net::BoundNetLog& bound_net_log) override {
    return new DownloadItemImpl(delegate, download_id, path, url, mime_type,
                                std::move(request_handle), bound_net_log);
  }
};

}  // namespace

DownloadManagerImpl::DownloadManagerImpl(
    net::NetLog* net_log,
    BrowserContext* browser_context)
    : item_factory_(new DownloadItemFactoryImpl()),
      file_factory_(new DownloadFileFactory()),
      history_size_(0),
      shutdown_needed_(true),
      browser_context_(browser_context),
      delegate_(NULL),
      net_log_(net_log),
      weak_factory_(this) {
  DCHECK(browser_context);
}

DownloadManagerImpl::~DownloadManagerImpl() {
  DCHECK(!shutdown_needed_);
}

DownloadItemImpl* DownloadManagerImpl::CreateActiveItem(
    uint32_t id,
    const DownloadCreateInfo& info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!ContainsKey(downloads_, id));
  net::BoundNetLog bound_net_log =
      net::BoundNetLog::Make(net_log_, net::NetLog::SOURCE_DOWNLOAD);
  DownloadItemImpl* download =
      item_factory_->CreateActiveItem(this, id, info, bound_net_log);
  downloads_[id] = download;
  return download;
}

void DownloadManagerImpl::GetNextId(const DownloadIdCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (delegate_) {
    delegate_->GetNextId(callback);
    return;
  }
  static uint32_t next_id = content::DownloadItem::kInvalidId + 1;
  callback.Run(next_id++);
}

void DownloadManagerImpl::DetermineDownloadTarget(
    DownloadItemImpl* item, const DownloadTargetCallback& callback) {
  // Note that this next call relies on
  // DownloadItemImplDelegate::DownloadTargetCallback and
  // DownloadManagerDelegate::DownloadTargetCallback having the same
  // type.  If the types ever diverge, gasket code will need to
  // be written here.
  if (!delegate_ || !delegate_->DetermineDownloadTarget(item, callback)) {
    base::FilePath target_path = item->GetForcedFilePath();
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

bool DownloadManagerImpl::ShouldOpenFileBasedOnExtension(
    const base::FilePath& path) {
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
  DVLOG(20) << __FUNCTION__ << "()"
            << " shutdown_needed_ = " << shutdown_needed_;
  if (!shutdown_needed_)
    return;
  shutdown_needed_ = false;

  FOR_EACH_OBSERVER(Observer, observers_, ManagerGoingDown(this));
  // TODO(benjhayden): Consider clearing observers_.

  // If there are in-progress downloads, cancel them. This also goes for
  // dangerous downloads which will remain in history if they aren't explicitly
  // accepted or discarded. Canceling will remove the intermediate download
  // file.
  for (DownloadMap::iterator it = downloads_.begin(); it != downloads_.end();
       ++it) {
    DownloadItemImpl* download = it->second;
    if (download->GetState() == DownloadItem::IN_PROGRESS)
      download->Cancel(false);
  }
  STLDeleteValues(&downloads_);
  url_downloaders_.clear();

  // We'll have nothing more to report to the observers after this point.
  observers_.Clear();

  if (delegate_)
    delegate_->Shutdown();
  delegate_ = NULL;
}

void DownloadManagerImpl::StartDownload(
    scoped_ptr<DownloadCreateInfo> info,
    scoped_ptr<ByteStreamReader> stream,
    const DownloadUrlParameters::OnStartedCallback& on_started) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(info);
  uint32_t download_id = info->download_id;
  const bool new_download = (download_id == content::DownloadItem::kInvalidId);
  base::Callback<void(uint32_t)> got_id(base::Bind(
      &DownloadManagerImpl::StartDownloadWithId, weak_factory_.GetWeakPtr(),
      base::Passed(&info), base::Passed(&stream), on_started, new_download));
  if (new_download) {
    GetNextId(got_id);
  } else {
    got_id.Run(download_id);
  }
}

void DownloadManagerImpl::StartDownloadWithId(
    scoped_ptr<DownloadCreateInfo> info,
    scoped_ptr<ByteStreamReader> stream,
    const DownloadUrlParameters::OnStartedCallback& on_started,
    bool new_download,
    uint32_t id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_NE(content::DownloadItem::kInvalidId, id);
  DownloadItemImpl* download = NULL;
  if (new_download) {
    download = CreateActiveItem(id, *info);
  } else {
    DownloadMap::iterator item_iterator = downloads_.find(id);
    // Trying to resume an interrupted download.
    if (item_iterator == downloads_.end() ||
        (item_iterator->second->GetState() == DownloadItem::CANCELLED)) {
      // If the download is no longer known to the DownloadManager, then it was
      // removed after it was resumed. Ignore. If the download is cancelled
      // while resuming, then also ignore the request.
      info->request_handle->CancelRequest();
      if (!on_started.is_null())
        on_started.Run(NULL, DOWNLOAD_INTERRUPT_REASON_USER_CANCELED);
      // The ByteStreamReader lives and dies on the FILE thread.
      BrowserThread::DeleteSoon(BrowserThread::FILE, FROM_HERE,
                                stream.release());
      return;
    }
    download = item_iterator->second;
    DCHECK_EQ(download->GetState(), DownloadItem::IN_PROGRESS);
    download->MergeOriginInfoOnResume(*info);
  }

  base::FilePath default_download_directory;
  if (delegate_) {
    base::FilePath website_save_directory;  // Unused
    bool skip_dir_check = false;            // Unused
    delegate_->GetSaveDir(GetBrowserContext(), &website_save_directory,
                          &default_download_directory, &skip_dir_check);
  }

  // Create the download file and start the download.
  scoped_ptr<DownloadFile> download_file(file_factory_->CreateFile(
      std::move(info->save_info), default_download_directory, info->url(),
      info->referrer_url, delegate_ && delegate_->GenerateFileHash(),
      std::move(stream), download->GetBoundNetLog(),
      download->DestinationObserverAsWeakPtr()));

  // Attach the client ID identifying the app to the AV system.
  if (download_file.get() && delegate_) {
    download_file->SetClientGuid(
        delegate_->ApplicationClientIdForFileScanning());
  }

  download->Start(std::move(download_file), std::move(info->request_handle));

  // For interrupted downloads, Start() will transition the state to
  // IN_PROGRESS and consumers will be notified via OnDownloadUpdated().
  // For new downloads, we notify here, rather than earlier, so that
  // the download_file is bound to download and all the usual
  // setters (e.g. Cancel) work.
  if (new_download)
    FOR_EACH_OBSERVER(Observer, observers_, OnDownloadCreated(this, download));

  if (!on_started.is_null())
    on_started.Run(download, DOWNLOAD_INTERRUPT_REASON_NONE);
}

void DownloadManagerImpl::CheckForHistoryFilesRemoval() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (DownloadMap::iterator it = downloads_.begin();
       it != downloads_.end(); ++it) {
    DownloadItemImpl* item = it->second;
    CheckForFileRemoval(item);
  }
}

void DownloadManagerImpl::CheckForFileRemoval(DownloadItemImpl* download_item) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if ((download_item->GetState() == DownloadItem::COMPLETE) &&
      !download_item->GetFileExternallyRemoved() &&
      delegate_) {
    delegate_->CheckForFileExistence(
        download_item,
        base::Bind(&DownloadManagerImpl::OnFileExistenceChecked,
                   weak_factory_.GetWeakPtr(), download_item->GetId()));
  }
}

void DownloadManagerImpl::OnFileExistenceChecked(uint32_t download_id,
                                                 bool result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!result) {  // File does not exist.
    if (ContainsKey(downloads_, download_id))
      downloads_[download_id]->OnDownloadedFileRemoved();
  }
}

BrowserContext* DownloadManagerImpl::GetBrowserContext() const {
  return browser_context_;
}

void DownloadManagerImpl::CreateSavePackageDownloadItem(
    const base::FilePath& main_file_path,
    const GURL& page_url,
    const std::string& mime_type,
    scoped_ptr<DownloadRequestHandleInterface> request_handle,
    const DownloadItemImplCreated& item_created) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetNextId(base::Bind(
      &DownloadManagerImpl::CreateSavePackageDownloadItemWithId,
      weak_factory_.GetWeakPtr(), main_file_path, page_url, mime_type,
      base::Passed(std::move(request_handle)), item_created));
}

void DownloadManagerImpl::CreateSavePackageDownloadItemWithId(
    const base::FilePath& main_file_path,
    const GURL& page_url,
    const std::string& mime_type,
    scoped_ptr<DownloadRequestHandleInterface> request_handle,
    const DownloadItemImplCreated& item_created,
    uint32_t id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_NE(content::DownloadItem::kInvalidId, id);
  DCHECK(!ContainsKey(downloads_, id));
  net::BoundNetLog bound_net_log =
      net::BoundNetLog::Make(net_log_, net::NetLog::SOURCE_DOWNLOAD);
  DownloadItemImpl* download_item = item_factory_->CreateSavePageItem(
      this, id, main_file_path, page_url, mime_type, std::move(request_handle),
      bound_net_log);
  downloads_[download_item->GetId()] = download_item;
  FOR_EACH_OBSERVER(Observer, observers_, OnDownloadCreated(
      this, download_item));
  if (!item_created.is_null())
    item_created.Run(download_item);
}

void DownloadManagerImpl::OnSavePackageSuccessfullyFinished(
    DownloadItem* download_item) {
  FOR_EACH_OBSERVER(Observer, observers_,
                    OnSavePackageSuccessfullyFinished(this, download_item));
}

// Resume a download of a specific URL. We send the request to the
// ResourceDispatcherHost, and let it send us responses like a regular
// download.
void DownloadManagerImpl::ResumeInterruptedDownload(
    scoped_ptr<content::DownloadUrlParameters> params,
    uint32_t id) {
  RecordDownloadSource(INITIATED_BY_RESUMPTION);
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&BeginDownload, base::Passed(&params), id,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&DownloadManagerImpl::AddUrlDownloader,
                 weak_factory_.GetWeakPtr()));
}

void DownloadManagerImpl::SetDownloadItemFactoryForTesting(
    scoped_ptr<DownloadItemFactory> item_factory) {
  item_factory_ = std::move(item_factory);
}

void DownloadManagerImpl::SetDownloadFileFactoryForTesting(
    scoped_ptr<DownloadFileFactory> file_factory) {
  file_factory_ = std::move(file_factory);
}

DownloadFileFactory* DownloadManagerImpl::GetDownloadFileFactoryForTesting() {
  return file_factory_.get();
}

void DownloadManagerImpl::DownloadRemoved(DownloadItemImpl* download) {
  if (!download)
    return;

  uint32_t download_id = download->GetId();
  if (downloads_.erase(download_id) == 0)
    return;
  delete download;
}

void DownloadManagerImpl::AddUrlDownloader(
    scoped_ptr<UrlDownloader, BrowserThread::DeleteOnIOThread> downloader) {
  if (downloader)
    url_downloaders_.push_back(std::move(downloader));
}

void DownloadManagerImpl::RemoveUrlDownloader(UrlDownloader* downloader) {
  for (auto ptr = url_downloaders_.begin(); ptr != url_downloaders_.end();
       ++ptr) {
    if (ptr->get() == downloader) {
      url_downloaders_.erase(ptr);
      return;
    }
  }
}

namespace {

bool RemoveDownloadBetween(base::Time remove_begin,
                           base::Time remove_end,
                           const DownloadItemImpl* download_item) {
  return download_item->GetStartTime() >= remove_begin &&
         (remove_end.is_null() || download_item->GetStartTime() < remove_end);
}

bool RemoveDownloadByOriginAndTime(const url::Origin& origin,
                                   base::Time remove_begin,
                                   base::Time remove_end,
                                   const DownloadItemImpl* download_item) {
  return origin.IsSameOriginWith(url::Origin(download_item->GetURL())) &&
         RemoveDownloadBetween(remove_begin, remove_end, download_item);
}

}  // namespace

int DownloadManagerImpl::RemoveDownloads(const DownloadRemover& remover) {
  int count = 0;
  DownloadMap::const_iterator it = downloads_.begin();
  while (it != downloads_.end()) {
    DownloadItemImpl* download = it->second;

    // Increment done here to protect against invalidation below.
    ++it;

    if (download->GetState() != DownloadItem::IN_PROGRESS &&
        remover.Run(download)) {
      download->Remove();
      count++;
    }
  }
  return count;
}

int DownloadManagerImpl::RemoveDownloadsByOriginAndTime(
    const url::Origin& origin,
    base::Time remove_begin,
    base::Time remove_end) {
  return RemoveDownloads(base::Bind(&RemoveDownloadByOriginAndTime,
                                    base::ConstRef(origin), remove_begin,
                                    remove_end));
}

int DownloadManagerImpl::RemoveDownloadsBetween(base::Time remove_begin,
                                                base::Time remove_end) {
  return RemoveDownloads(
      base::Bind(&RemoveDownloadBetween, remove_begin, remove_end));
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
    DCHECK_EQ("POST", params->method());
  }
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&BeginDownload, base::Passed(&params),
                 content::DownloadItem::kInvalidId, weak_factory_.GetWeakPtr()),
      base::Bind(&DownloadManagerImpl::AddUrlDownloader,
                 weak_factory_.GetWeakPtr()));
}

void DownloadManagerImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DownloadManagerImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

DownloadItem* DownloadManagerImpl::CreateDownloadItem(
    uint32_t id,
    const base::FilePath& current_path,
    const base::FilePath& target_path,
    const std::vector<GURL>& url_chain,
    const GURL& referrer_url,
    const std::string& mime_type,
    const std::string& original_mime_type,
    const base::Time& start_time,
    const base::Time& end_time,
    const std::string& etag,
    const std::string& last_modified,
    int64_t received_bytes,
    int64_t total_bytes,
    DownloadItem::DownloadState state,
    DownloadDangerType danger_type,
    DownloadInterruptReason interrupt_reason,
    bool opened) {
  if (ContainsKey(downloads_, id)) {
    NOTREACHED();
    return NULL;
  }
  DownloadItemImpl* item = item_factory_->CreatePersistedItem(
      this,
      id,
      current_path,
      target_path,
      url_chain,
      referrer_url,
      mime_type,
      original_mime_type,
      start_time,
      end_time,
      etag,
      last_modified,
      received_bytes,
      total_bytes,
      state,
      danger_type,
      interrupt_reason,
      opened,
      net::BoundNetLog::Make(net_log_, net::NetLog::SOURCE_DOWNLOAD));
  downloads_[id] = item;
  FOR_EACH_OBSERVER(Observer, observers_, OnDownloadCreated(this, item));
  DVLOG(20) << __FUNCTION__ << "() download = " << item->DebugString(true);
  return item;
}

int DownloadManagerImpl::InProgressCount() const {
  int count = 0;
  for (DownloadMap::const_iterator it = downloads_.begin();
       it != downloads_.end(); ++it) {
    if (it->second->GetState() == DownloadItem::IN_PROGRESS)
      ++count;
  }
  return count;
}

int DownloadManagerImpl::NonMaliciousInProgressCount() const {
  int count = 0;
  for (DownloadMap::const_iterator it = downloads_.begin();
       it != downloads_.end(); ++it) {
    if (it->second->GetState() == DownloadItem::IN_PROGRESS &&
        it->second->GetDangerType() != DOWNLOAD_DANGER_TYPE_DANGEROUS_URL &&
        it->second->GetDangerType() != DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT &&
        it->second->GetDangerType() != DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST &&
        it->second->GetDangerType() !=
            DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED) {
      ++count;
    }
  }
  return count;
}

DownloadItem* DownloadManagerImpl::GetDownload(uint32_t download_id) {
  return ContainsKey(downloads_, download_id) ? downloads_[download_id] : NULL;
}

void DownloadManagerImpl::GetAllDownloads(DownloadVector* downloads) {
  for (DownloadMap::iterator it = downloads_.begin();
       it != downloads_.end(); ++it) {
    downloads->push_back(it->second);
  }
}

void DownloadManagerImpl::OpenDownload(DownloadItemImpl* download) {
  int num_unopened = 0;
  for (DownloadMap::iterator it = downloads_.begin();
       it != downloads_.end(); ++it) {
    DownloadItemImpl* item = it->second;
    if ((item->GetState() == DownloadItem::COMPLETE) &&
        !item->GetOpened())
      ++num_unopened;
  }
  RecordOpensOutstanding(num_unopened);

  if (delegate_)
    delegate_->OpenDownload(download);
}

void DownloadManagerImpl::ShowDownloadInShell(DownloadItemImpl* download) {
  if (delegate_)
    delegate_->ShowDownloadInShell(download);
}

}  // namespace content
