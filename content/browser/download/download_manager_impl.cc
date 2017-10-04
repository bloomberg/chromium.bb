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
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/supports_user_data.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "content/browser/byte_stream.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/download/download_create_info.h"
#include "content/browser/download/download_file_factory.h"
#include "content/browser/download/download_item_factory.h"
#include "content/browser/download/download_item_impl.h"
#include "content/browser/download/download_stats.h"
#include "content/browser/download/download_task_runner.h"
#include "content/browser/download/download_utils.h"
#include "content/browser/download/resource_downloader.h"
#include "content/browser/download/url_downloader.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/throttling_url_loader.h"
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
#include "content/public/common/content_features.h"
#include "content/public/common/previews_state.h"
#include "content/public/common/referrer.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/load_flags.h"
#include "net/base/request_priority.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/log/net_log_source_type.h"
#include "net/log/net_log_with_source.h"
#include "net/url_request/url_request_context.h"
#include "storage/browser/blob/blob_url_request_job_factory.h"
#include "url/origin.h"

#if defined(USE_X11)
#include "base/nix/xdg_util.h"
#endif

namespace content {
namespace {

StoragePartitionImpl* GetStoragePartition(BrowserContext* context,
                                          int render_process_id,
                                          int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  SiteInstance* site_instance = nullptr;
  if (render_process_id >= 0) {
    RenderFrameHost* render_frame_host_ =
        RenderFrameHost::FromID(render_process_id, render_frame_id);
    if (render_frame_host_)
      site_instance = render_frame_host_->GetSiteInstance();
  }
  return static_cast<StoragePartitionImpl*>(
      BrowserContext::GetStoragePartition(context, site_instance));
}

bool CanRequestURLFromRenderer(int render_process_id, GURL url) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Check if the renderer is permitted to request the requested URL.
  if (!ChildProcessSecurityPolicyImpl::GetInstance()->CanRequestURL(
          render_process_id, url)) {
    DVLOG(1) << "Denied unauthorized download request for "
             << url.possibly_invalid_spec();
    return false;
  }
  return true;
}

void CreateInterruptedDownload(
    DownloadUrlParameters* params,
    DownloadInterruptReason reason,
    base::WeakPtr<DownloadManagerImpl> download_manager) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  std::unique_ptr<DownloadCreateInfo> failed_created_info(
      new DownloadCreateInfo(base::Time::Now(), net::NetLogWithSource(),
                             base::WrapUnique(new DownloadSaveInfo)));
  failed_created_info->url_chain.push_back(params->url());
  failed_created_info->result = reason;
  std::unique_ptr<ByteStreamReader> empty_byte_stream;
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&DownloadManager::StartDownload, download_manager,
                     std::move(failed_created_info),
                     base::MakeUnique<DownloadManager::InputStream>(
                         std::move(empty_byte_stream)),
                     params->callback()));
}

DownloadManagerImpl::UniqueUrlDownloadHandlerPtr BeginDownload(
    std::unique_ptr<DownloadUrlParameters> params,
    content::ResourceContext* resource_context,
    uint32_t download_id,
    base::WeakPtr<DownloadManagerImpl> download_manager) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  std::unique_ptr<net::URLRequest> url_request =
      DownloadRequestCore::CreateRequestOnIOThread(download_id, params.get());
  std::unique_ptr<storage::BlobDataHandle> blob_data_handle =
      params->GetBlobDataHandle();
  if (blob_data_handle) {
    storage::BlobProtocolHandler::SetRequestedBlobDataHandle(
        url_request.get(), std::move(blob_data_handle));
  }

  // If there's a valid renderer process associated with the request, then the
  // request should be driven by the ResourceLoader. Pass it over to the
  // ResourceDispatcherHostImpl which will in turn pass it along to the
  // ResourceLoader.
  if (params->render_process_host_id() >= 0) {
    DownloadInterruptReason reason = DownloadManagerImpl::BeginDownloadRequest(
        std::move(url_request), params->referrer(), resource_context,
        params->content_initiated(), params->render_process_host_id(),
        params->render_view_host_routing_id(),
        params->render_frame_host_routing_id(),
        params->do_not_prompt_for_login());

    // If the download was accepted, the DownloadResourceHandler is now
    // responsible for driving the request to completion.
    if (reason == DOWNLOAD_INTERRUPT_REASON_NONE)
      return nullptr;

    // Otherwise, create an interrupted download.
    CreateInterruptedDownload(params.get(), reason, download_manager);
    return nullptr;
  }

  return DownloadManagerImpl::UniqueUrlDownloadHandlerPtr(
      UrlDownloader::BeginDownload(download_manager, std::move(url_request),
                                   params->referrer(), false)
          .release());
}

DownloadManagerImpl::UniqueUrlDownloadHandlerPtr BeginResourceDownload(
    std::unique_ptr<DownloadUrlParameters> params,
    std::unique_ptr<ResourceRequest> request,
    scoped_refptr<URLLoaderFactoryGetter> url_loader_factory_getter,
    scoped_refptr<storage::FileSystemContext> file_system_context,
    uint32_t download_id,
    base::WeakPtr<DownloadManagerImpl> download_manager) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Check if the renderer is permitted to request the requested URL.
  if (params->render_process_host_id() >= 0 &&
      !CanRequestURLFromRenderer(params->render_process_host_id(),
                                 params->url())) {
    CreateInterruptedDownload(params.get(),
                              DOWNLOAD_INTERRUPT_REASON_NETWORK_INVALID_REQUEST,
                              download_manager);
    return nullptr;
  }

  return DownloadManagerImpl::UniqueUrlDownloadHandlerPtr(
      ResourceDownloader::BeginDownload(
          download_manager, std::move(params), std::move(request),
          url_loader_factory_getter, file_system_context, download_id, false)
          .release());
}

// Creates a ResourceDownloader to own the URLLoader and intercept the response,
// and passes it back to the DownloadManager.
void InterceptNavigationResponse(
    base::OnceCallback<void(DownloadManagerImpl::UniqueUrlDownloadHandlerPtr)>
        callback,
    base::WeakPtr<DownloadManagerImpl> download_manager,
    const scoped_refptr<ResourceResponse>& response,
    mojo::ScopedDataPipeConsumerHandle consumer_handle,
    const SSLStatus& ssl_status,
    std::unique_ptr<ResourceRequest> resource_request,
    std::unique_ptr<ThrottlingURLLoader> url_loader,
    base::Optional<ResourceRequestCompletionStatus> completion_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DownloadManagerImpl::UniqueUrlDownloadHandlerPtr resource_downloader(
      ResourceDownloader::InterceptNavigationResponse(
          download_manager, std::move(resource_request), response,
          std::move(consumer_handle), ssl_status, std::move(url_loader),
          std::move(completion_status))
          .release());

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(std::move(callback), std::move(resource_downloader)));
}

class DownloadItemFactoryImpl : public DownloadItemFactory {
 public:
  DownloadItemFactoryImpl() {}
  ~DownloadItemFactoryImpl() override {}

  DownloadItemImpl* CreatePersistedItem(
      DownloadItemImplDelegate* delegate,
      const std::string& guid,
      uint32_t download_id,
      const base::FilePath& current_path,
      const base::FilePath& target_path,
      const std::vector<GURL>& url_chain,
      const GURL& referrer_url,
      const GURL& site_url,
      const GURL& tab_url,
      const GURL& tab_refererr_url,
      const std::string& mime_type,
      const std::string& original_mime_type,
      base::Time start_time,
      base::Time end_time,
      const std::string& etag,
      const std::string& last_modified,
      int64_t received_bytes,
      int64_t total_bytes,
      const std::string& hash,
      DownloadItem::DownloadState state,
      DownloadDangerType danger_type,
      DownloadInterruptReason interrupt_reason,
      bool opened,
      base::Time last_access_time,
      bool transient,
      const std::vector<DownloadItem::ReceivedSlice>& received_slices,
      const net::NetLogWithSource& net_log) override {
    return new DownloadItemImpl(
        delegate, guid, download_id, current_path, target_path, url_chain,
        referrer_url, site_url, tab_url, tab_refererr_url, mime_type,
        original_mime_type, start_time, end_time, etag, last_modified,
        received_bytes, total_bytes, hash, state, danger_type, interrupt_reason,
        opened, last_access_time, transient, received_slices, net_log);
  }

  DownloadItemImpl* CreateActiveItem(
      DownloadItemImplDelegate* delegate,
      uint32_t download_id,
      const DownloadCreateInfo& info,
      const net::NetLogWithSource& net_log) override {
    return new DownloadItemImpl(delegate, download_id, info, net_log);
  }

  DownloadItemImpl* CreateSavePageItem(
      DownloadItemImplDelegate* delegate,
      uint32_t download_id,
      const base::FilePath& path,
      const GURL& url,
      const std::string& mime_type,
      std::unique_ptr<DownloadRequestHandleInterface> request_handle,
      const net::NetLogWithSource& net_log) override {
    return new DownloadItemImpl(delegate, download_id, path, url, mime_type,
                                std::move(request_handle), net_log);
  }
};

#if defined(USE_X11)
base::FilePath GetTemporaryDownloadDirectory() {
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  return base::nix::GetXDGDirectory(env.get(), "XDG_DATA_HOME", ".local/share");
}
#endif

}  // namespace

DownloadManagerImpl::DownloadManagerImpl(net::NetLog* net_log,
                                         BrowserContext* browser_context)
    : item_factory_(new DownloadItemFactoryImpl()),
      file_factory_(new DownloadFileFactory()),
      shutdown_needed_(true),
      initialized_(false),
      browser_context_(browser_context),
      delegate_(nullptr),
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
  DCHECK(!base::ContainsKey(downloads_, id));
  net::NetLogWithSource net_log =
      net::NetLogWithSource::Make(net_log_, net::NetLogSourceType::DOWNLOAD);
  DownloadItemImpl* download =
      item_factory_->CreateActiveItem(this, id, info, net_log);
  downloads_[id] = base::WrapUnique(download);
  downloads_by_guid_[download->GetGuid()] = download;
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
    callback.Run(target_path, DownloadItem::TARGET_DISPOSITION_OVERWRITE,
                 DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS, target_path,
                 DOWNLOAD_INTERRUPT_REASON_NONE);
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
  DVLOG(20) << __func__ << "() shutdown_needed_ = " << shutdown_needed_;
  if (!shutdown_needed_)
    return;
  shutdown_needed_ = false;

  for (auto& observer : observers_)
    observer.ManagerGoingDown(this);
  // TODO(benjhayden): Consider clearing observers_.

  // If there are in-progress downloads, cancel them. This also goes for
  // dangerous downloads which will remain in history if they aren't explicitly
  // accepted or discarded. Canceling will remove the intermediate download
  // file.
  for (const auto& it : downloads_) {
    DownloadItemImpl* download = it.second.get();
    if (download->GetState() == DownloadItem::IN_PROGRESS)
      download->Cancel(false);
  }
  downloads_.clear();
  downloads_by_guid_.clear();
  url_download_handlers_.clear();

  // We'll have nothing more to report to the observers after this point.
  observers_.Clear();

  if (delegate_)
    delegate_->Shutdown();
  delegate_ = nullptr;
}

void DownloadManagerImpl::StartDownload(
    std::unique_ptr<DownloadCreateInfo> info,
    std::unique_ptr<DownloadManager::InputStream> stream,
    const DownloadUrlParameters::OnStartedCallback& on_started) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(info);

  // |stream| is only non-nil if the download request was successful.
  DCHECK(
      (info->result == DOWNLOAD_INTERRUPT_REASON_NONE && !stream->IsEmpty()) ||
      (info->result != DOWNLOAD_INTERRUPT_REASON_NONE && stream->IsEmpty()));
  DVLOG(20) << __func__
            << "() result=" << DownloadInterruptReasonToString(info->result);
  uint32_t download_id = info->download_id;
  const bool new_download = (download_id == content::DownloadItem::kInvalidId);
  if (new_download)
    RecordDownloadConnectionSecurity(info->url(), info->url_chain);
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
    std::unique_ptr<DownloadCreateInfo> info,
    std::unique_ptr<DownloadManager::InputStream> stream,
    const DownloadUrlParameters::OnStartedCallback& on_started,
    bool new_download,
    uint32_t id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_NE(content::DownloadItem::kInvalidId, id);
  DownloadItemImpl* download = nullptr;
  if (new_download) {
    download = CreateActiveItem(id, *info);
  } else {
    auto item_iterator = downloads_.find(id);
    // Trying to resume an interrupted download.
    if (item_iterator == downloads_.end() ||
        (item_iterator->second->GetState() == DownloadItem::CANCELLED)) {
      // If the download is no longer known to the DownloadManager, then it was
      // removed after it was resumed. Ignore. If the download is cancelled
      // while resuming, then also ignore the request.
      info->request_handle->CancelRequest(true);
      if (!on_started.is_null())
        on_started.Run(nullptr, DOWNLOAD_INTERRUPT_REASON_USER_CANCELED);
      // The ByteStreamReader lives and dies on the download sequence.
      if (info->result == DOWNLOAD_INTERRUPT_REASON_NONE)
        GetDownloadTaskRunner()->DeleteSoon(FROM_HERE, stream.release());
      return;
    }
    download = item_iterator->second.get();
  }

  base::FilePath default_download_directory;
#if defined(USE_X11)
  // TODO(thomasanderson): Remove this when all Linux distros with
  // versions of GTK lower than 3.14.7 are no longer supported.  This
  // should happen when support for Ubuntu Trusty and Debian Jessie
  // are removed.
  default_download_directory = GetTemporaryDownloadDirectory();
#else
  if (delegate_) {
    base::FilePath website_save_directory;  // Unused
    bool skip_dir_check = false;            // Unused
    delegate_->GetSaveDir(GetBrowserContext(), &website_save_directory,
                          &default_download_directory, &skip_dir_check);
  }
#endif

  std::unique_ptr<DownloadFile> download_file;

  if (info->result == DOWNLOAD_INTERRUPT_REASON_NONE) {
    DCHECK(stream.get());
    download_file.reset(file_factory_->CreateFile(
        std::move(info->save_info), default_download_directory,
        std::move(stream), download->GetNetLogWithSource(),
        download->DestinationObserverAsWeakPtr()));
  }
  // It is important to leave info->save_info intact in the case of an interrupt
  // so that the DownloadItem can salvage what it can out of a failed resumption
  // attempt.

  download->Start(std::move(download_file), std::move(info->request_handle),
                  *info);

  // For interrupted downloads, Start() will transition the state to
  // IN_PROGRESS and consumers will be notified via OnDownloadUpdated().
  // For new downloads, we notify here, rather than earlier, so that
  // the download_file is bound to download and all the usual
  // setters (e.g. Cancel) work.
  if (new_download) {
    for (auto& observer : observers_)
      observer.OnDownloadCreated(this, download);
  }

  if (!on_started.is_null())
    on_started.Run(download, DOWNLOAD_INTERRUPT_REASON_NONE);
}

void DownloadManagerImpl::CheckForHistoryFilesRemoval() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (const auto& it : downloads_) {
    DownloadItemImpl* item = it.second.get();
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
        base::BindOnce(&DownloadManagerImpl::OnFileExistenceChecked,
                       weak_factory_.GetWeakPtr(), download_item->GetId()));
  }
}

void DownloadManagerImpl::OnFileExistenceChecked(uint32_t download_id,
                                                 bool result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!result) {  // File does not exist.
    if (base::ContainsKey(downloads_, download_id))
      downloads_[download_id]->OnDownloadedFileRemoved();
  }
}

std::string DownloadManagerImpl::GetApplicationClientIdForFileScanning() const {
  if (delegate_)
    return delegate_->ApplicationClientIdForFileScanning();
  return std::string();
}

BrowserContext* DownloadManagerImpl::GetBrowserContext() const {
  return browser_context_;
}

void DownloadManagerImpl::CreateSavePackageDownloadItem(
    const base::FilePath& main_file_path,
    const GURL& page_url,
    const std::string& mime_type,
    std::unique_ptr<DownloadRequestHandleInterface> request_handle,
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
    std::unique_ptr<DownloadRequestHandleInterface> request_handle,
    const DownloadItemImplCreated& item_created,
    uint32_t id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_NE(content::DownloadItem::kInvalidId, id);
  DCHECK(!base::ContainsKey(downloads_, id));
  net::NetLogWithSource net_log =
      net::NetLogWithSource::Make(net_log_, net::NetLogSourceType::DOWNLOAD);
  DownloadItemImpl* download_item = item_factory_->CreateSavePageItem(
      this, id, main_file_path, page_url, mime_type, std::move(request_handle),
      net_log);
  downloads_[download_item->GetId()] = base::WrapUnique(download_item);
  DCHECK(!base::ContainsKey(downloads_by_guid_, download_item->GetGuid()));
  downloads_by_guid_[download_item->GetGuid()] = download_item;
  for (auto& observer : observers_)
    observer.OnDownloadCreated(this, download_item);
  if (!item_created.is_null())
    item_created.Run(download_item);
}

// Resume a download of a specific URL. We send the request to the
// ResourceDispatcherHost, and let it send us responses like a regular
// download.
void DownloadManagerImpl::ResumeInterruptedDownload(
    std::unique_ptr<content::DownloadUrlParameters> params,
    uint32_t id) {
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&BeginDownload, base::Passed(&params),
                 browser_context_->GetResourceContext(), id,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&DownloadManagerImpl::AddUrlDownloadHandler,
                 weak_factory_.GetWeakPtr()));
}

void DownloadManagerImpl::SetDownloadItemFactoryForTesting(
    std::unique_ptr<DownloadItemFactory> item_factory) {
  item_factory_ = std::move(item_factory);
}

void DownloadManagerImpl::SetDownloadFileFactoryForTesting(
    std::unique_ptr<DownloadFileFactory> file_factory) {
  file_factory_ = std::move(file_factory);
}

DownloadFileFactory* DownloadManagerImpl::GetDownloadFileFactoryForTesting() {
  return file_factory_.get();
}

void DownloadManagerImpl::DownloadRemoved(DownloadItemImpl* download) {
  if (!download)
    return;

  downloads_by_guid_.erase(download->GetGuid());
  downloads_.erase(download->GetId());
}

void DownloadManagerImpl::AddUrlDownloadHandler(
    UniqueUrlDownloadHandlerPtr downloader) {
  if (downloader)
    url_download_handlers_.push_back(std::move(downloader));
}

// static
DownloadInterruptReason DownloadManagerImpl::BeginDownloadRequest(
    std::unique_ptr<net::URLRequest> url_request,
    const Referrer& referrer,
    ResourceContext* resource_context,
    bool is_content_initiated,
    int render_process_id,
    int render_view_route_id,
    int render_frame_route_id,
    bool do_not_prompt_for_login) {
  if (ResourceDispatcherHostImpl::Get()->is_shutdown())
    return DOWNLOAD_INTERRUPT_REASON_USER_SHUTDOWN;

  // The URLRequest needs to be initialized with the referrer and other
  // information prior to issuing it.
  ResourceDispatcherHostImpl::Get()->InitializeURLRequest(
      url_request.get(), referrer,
      true,  // download.
      render_process_id, render_view_route_id, render_frame_route_id,
      PREVIEWS_OFF, resource_context);

  // We treat a download as a main frame load, and thus update the policy URL on
  // redirects.
  //
  // TODO(davidben): Is this correct? If this came from a
  // ViewHostMsg_DownloadUrl in a frame, should it have first-party URL set
  // appropriately?
  url_request->set_first_party_url_policy(
      net::URLRequest::UPDATE_FIRST_PARTY_URL_ON_REDIRECT);

  const GURL& url = url_request->original_url();

  // Check if the renderer is permitted to request the requested URL.
  if (!CanRequestURLFromRenderer(render_process_id, url))
    return DOWNLOAD_INTERRUPT_REASON_NETWORK_INVALID_REQUEST;

  const net::URLRequestContext* request_context = url_request->context();
  if (!request_context->job_factory()->IsHandledProtocol(url.scheme())) {
    DVLOG(1) << "Download request for unsupported protocol: "
             << url.possibly_invalid_spec();
    return DOWNLOAD_INTERRUPT_REASON_NETWORK_INVALID_REQUEST;
  }

  // From this point forward, the |DownloadResourceHandler| is responsible for
  // |started_callback|.
  // TODO(ananta)
  // Find a better way to create the DownloadResourceHandler instance.
  std::unique_ptr<ResourceHandler> handler(
      DownloadResourceHandler::Create(url_request.get()));

  ResourceDispatcherHostImpl::Get()->BeginURLRequest(
      std::move(url_request), std::move(handler), true,  // download
      is_content_initiated, do_not_prompt_for_login, resource_context);
  return DOWNLOAD_INTERRUPT_REASON_NONE;
}

NavigationURLLoader::NavigationInterceptionCB
DownloadManagerImpl::GetNavigationInterceptionCB(
    const scoped_refptr<ResourceResponse>& response,
    mojo::ScopedDataPipeConsumerHandle consumer_handle,
    const SSLStatus& ssl_status) {
  return base::BindOnce(
      &InterceptNavigationResponse,
      base::BindOnce(&DownloadManagerImpl::AddUrlDownloadHandler,
                     weak_factory_.GetWeakPtr()),
      weak_factory_.GetWeakPtr(), response, std::move(consumer_handle),
      ssl_status);
}

int DownloadManagerImpl::RemoveDownloadsByURLAndTime(
    const base::Callback<bool(const GURL&)>& url_filter,
    base::Time remove_begin,
    base::Time remove_end) {
  int count = 0;
  auto it = downloads_.begin();
  while (it != downloads_.end()) {
    DownloadItemImpl* download = it->second.get();

    // Increment done here to protect against invalidation below.
    ++it;

    if (download->GetState() != DownloadItem::IN_PROGRESS &&
        url_filter.Run(download->GetURL()) &&
        download->GetStartTime() >= remove_begin &&
        (remove_end.is_null() || download->GetStartTime() < remove_end)) {
      download->Remove();
      count++;
    }
  }
  return count;
}

void DownloadManagerImpl::DownloadUrl(
    std::unique_ptr<DownloadUrlParameters> params) {
  if (params->post_id() >= 0) {
    // Check this here so that the traceback is more useful.
    DCHECK(params->prefer_cache());
    DCHECK_EQ("POST", params->method());
  }

  // TODO(qinmin): remove false from the if statement once download works when
  // network service is enabled, or once we disable the tests that are currently
  // passing with the URLRequest code path.
  if (base::FeatureList::IsEnabled(features::kNetworkService)) {
    std::unique_ptr<ResourceRequest> request = CreateResourceRequest(
        params.get());
    StoragePartitionImpl* storage_partition =
        GetStoragePartition(browser_context_, params->render_process_host_id(),
                            params->render_frame_host_routing_id());
    BrowserThread::PostTaskAndReplyWithResult(
        BrowserThread::IO, FROM_HERE,
        base::BindOnce(
            &BeginResourceDownload, std::move(params), std::move(request),
            storage_partition->url_loader_factory_getter(),
            base::WrapRefCounted(storage_partition->GetFileSystemContext()),
            content::DownloadItem::kInvalidId, weak_factory_.GetWeakPtr()),
        base::BindOnce(&DownloadManagerImpl::AddUrlDownloadHandler,
                       weak_factory_.GetWeakPtr()));
  } else {
    BrowserThread::PostTaskAndReplyWithResult(
        BrowserThread::IO, FROM_HERE,
        base::BindOnce(&BeginDownload, std::move(params),
                       browser_context_->GetResourceContext(),
                       content::DownloadItem::kInvalidId,
                       weak_factory_.GetWeakPtr()),
        base::BindOnce(&DownloadManagerImpl::AddUrlDownloadHandler,
                       weak_factory_.GetWeakPtr()));
  }
}

void DownloadManagerImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DownloadManagerImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

DownloadItem* DownloadManagerImpl::CreateDownloadItem(
    const std::string& guid,
    uint32_t id,
    const base::FilePath& current_path,
    const base::FilePath& target_path,
    const std::vector<GURL>& url_chain,
    const GURL& referrer_url,
    const GURL& site_url,
    const GURL& tab_url,
    const GURL& tab_refererr_url,
    const std::string& mime_type,
    const std::string& original_mime_type,
    base::Time start_time,
    base::Time end_time,
    const std::string& etag,
    const std::string& last_modified,
    int64_t received_bytes,
    int64_t total_bytes,
    const std::string& hash,
    DownloadItem::DownloadState state,
    DownloadDangerType danger_type,
    DownloadInterruptReason interrupt_reason,
    bool opened,
    base::Time last_access_time,
    bool transient,
    const std::vector<DownloadItem::ReceivedSlice>& received_slices) {
  if (base::ContainsKey(downloads_, id)) {
    NOTREACHED();
    return nullptr;
  }
  DCHECK(!base::ContainsKey(downloads_by_guid_, guid));
  DownloadItemImpl* item = item_factory_->CreatePersistedItem(
      this, guid, id, current_path, target_path, url_chain, referrer_url,
      site_url, tab_url, tab_refererr_url, mime_type, original_mime_type,
      start_time, end_time, etag, last_modified, received_bytes, total_bytes,
      hash, state, danger_type, interrupt_reason, opened, last_access_time,
      transient, received_slices,
      net::NetLogWithSource::Make(net_log_, net::NetLogSourceType::DOWNLOAD));
  downloads_[id] = base::WrapUnique(item);
  downloads_by_guid_[guid] = item;
  for (auto& observer : observers_)
    observer.OnDownloadCreated(this, item);
  DVLOG(20) << __func__ << "() download = " << item->DebugString(true);
  return item;
}

void DownloadManagerImpl::PostInitialization() {
  DCHECK(!initialized_);
  initialized_ = true;
  for (auto& observer : observers_)
    observer.OnManagerInitialized();
}

bool DownloadManagerImpl::IsManagerInitialized() const {
  return initialized_;
}

int DownloadManagerImpl::InProgressCount() const {
  int count = 0;
  for (const auto& it : downloads_) {
    if (it.second->GetState() == DownloadItem::IN_PROGRESS)
      ++count;
  }
  return count;
}

int DownloadManagerImpl::NonMaliciousInProgressCount() const {
  int count = 0;
  for (const auto& it : downloads_) {
    if (it.second->GetState() == DownloadItem::IN_PROGRESS &&
        it.second->GetDangerType() != DOWNLOAD_DANGER_TYPE_DANGEROUS_URL &&
        it.second->GetDangerType() != DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT &&
        it.second->GetDangerType() != DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST &&
        it.second->GetDangerType() !=
            DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED) {
      ++count;
    }
  }
  return count;
}

DownloadItem* DownloadManagerImpl::GetDownload(uint32_t download_id) {
  return base::ContainsKey(downloads_, download_id)
             ? downloads_[download_id].get()
             : nullptr;
}

DownloadItem* DownloadManagerImpl::GetDownloadByGuid(const std::string& guid) {
  return base::ContainsKey(downloads_by_guid_, guid) ? downloads_by_guid_[guid]
                                                     : nullptr;
}

void DownloadManagerImpl::OnUrlDownloadStarted(
    std::unique_ptr<DownloadCreateInfo> download_create_info,
    std::unique_ptr<DownloadManager::InputStream> stream,
    const DownloadUrlParameters::OnStartedCallback& callback) {
  StartDownload(std::move(download_create_info), std::move(stream), callback);
}

void DownloadManagerImpl::OnUrlDownloadStopped(UrlDownloadHandler* downloader) {
  for (auto ptr = url_download_handlers_.begin();
       ptr != url_download_handlers_.end(); ++ptr) {
    if (ptr->get() == downloader) {
      url_download_handlers_.erase(ptr);
      return;
    }
  }
}

void DownloadManagerImpl::GetAllDownloads(DownloadVector* downloads) {
  for (const auto& it : downloads_) {
    downloads->push_back(it.second.get());
  }
}

void DownloadManagerImpl::OpenDownload(DownloadItemImpl* download) {
  int num_unopened = 0;
  for (const auto& it : downloads_) {
    DownloadItemImpl* item = it.second.get();
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
