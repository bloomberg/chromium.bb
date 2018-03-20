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
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/supports_user_data.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "components/download/downloader/in_progress/download_entry.h"
#include "components/download/downloader/in_progress/in_progress_cache_impl.h"
#include "components/download/public/common/download_create_info.h"
#include "components/download/public/common/download_file.h"
#include "components/download/public/common/download_file_factory.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/download/public/common/download_request_handle_interface.h"
#include "components/download/public/common/download_stats.h"
#include "components/download/public/common/download_task_runner.h"
#include "components/download/public/common/download_url_parameters.h"
#include "components/download/public/common/resource_downloader.h"
#include "content/browser/byte_stream.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/download/byte_stream_input_stream.h"
#include "content/browser/download/download_item_factory.h"
#include "content/browser/download/download_item_impl.h"
#include "content/browser/download/download_resource_handler.h"
#include "content/browser/download/download_utils.h"
#include "content/browser/download/url_downloader.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/throttling_url_loader.h"
#include "content/common/wrapper_shared_url_loader_factory.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/download_manager_delegate.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/previews_state.h"
#include "content/public/common/referrer.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/load_flags.h"
#include "net/base/request_priority.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/url_request/url_request_context.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/features.h"
#include "storage/browser/blob/blob_url_loader_factory.h"
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
    download::DownloadUrlParameters* params,
    download::DownloadInterruptReason reason,
    base::WeakPtr<DownloadManagerImpl> download_manager) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  std::unique_ptr<download::DownloadCreateInfo> failed_created_info(
      new download::DownloadCreateInfo(
          base::Time::Now(), base::WrapUnique(new download::DownloadSaveInfo)));
  failed_created_info->url_chain.push_back(params->url());
  failed_created_info->result = reason;
  std::unique_ptr<ByteStreamReader> empty_byte_stream;
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(
          &DownloadManager::StartDownload, download_manager,
          std::move(failed_created_info),
          std::make_unique<ByteStreamInputStream>(std::move(empty_byte_stream)),
          params->callback()));
}

// Helper functions for DownloadItem -> DownloadEntry for InProgressCache.

uint64_t GetUniqueDownloadId() {
  // Get a new UKM download_id that is not 0.
  uint64_t download_id = 0;
  do {
    download_id = base::RandUint64();
  } while (download_id == 0);
  return download_id;
}

download::DownloadEntry CreateDownloadEntryFromItemImpl(
    const DownloadItemImpl& item) {
  return download::DownloadEntry(item.GetGuid(), item.download_source(),
                                 GetUniqueDownloadId());
}

download::DownloadEntry CreateDownloadEntryFromItem(
    const download::DownloadItem& item) {
  return download::DownloadEntry(
      item.GetGuid(), download::DownloadSource::UNKNOWN, GetUniqueDownloadId());
}

DownloadManagerImpl::UniqueUrlDownloadHandlerPtr BeginDownload(
    std::unique_ptr<download::DownloadUrlParameters> params,
    std::unique_ptr<storage::BlobDataHandle> blob_data_handle,
    content::ResourceContext* resource_context,
    uint32_t download_id,
    base::WeakPtr<DownloadManagerImpl> download_manager) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  params->set_blob_storage_context_getter(
      base::BindOnce(&BlobStorageContextGetter, resource_context));
  std::unique_ptr<net::URLRequest> url_request =
      DownloadRequestCore::CreateRequestOnIOThread(download_id, params.get());
  if (blob_data_handle) {
    storage::BlobProtocolHandler::SetRequestedBlobDataHandle(
        url_request.get(), std::move(blob_data_handle));
  }

  // If there's a valid renderer process associated with the request, then the
  // request should be driven by the ResourceLoader. Pass it over to the
  // ResourceDispatcherHostImpl which will in turn pass it along to the
  // ResourceLoader.
  if (params->render_process_host_id() >= 0) {
    download::DownloadInterruptReason reason =
        DownloadManagerImpl::BeginDownloadRequest(
            std::move(url_request), resource_context, params.get());

    // If the download was accepted, the DownloadResourceHandler is now
    // responsible for driving the request to completion.
    if (reason == download::DOWNLOAD_INTERRUPT_REASON_NONE)
      return nullptr;

    // Otherwise, create an interrupted download.
    CreateInterruptedDownload(params.get(), reason, download_manager);
    return nullptr;
  }

  return DownloadManagerImpl::UniqueUrlDownloadHandlerPtr(
      UrlDownloader::BeginDownload(download_manager, std::move(url_request),
                                   params.get(), false)
          .release());
}

DownloadManagerImpl::UniqueUrlDownloadHandlerPtr BeginResourceDownload(
    std::unique_ptr<download::DownloadUrlParameters> params,
    std::unique_ptr<network::ResourceRequest> request,
    std::unique_ptr<storage::BlobDataHandle> blob_data_handle,
    scoped_refptr<URLLoaderFactoryGetter> url_loader_factory_getter,
    uint32_t download_id,
    base::WeakPtr<DownloadManagerImpl> download_manager,
    const GURL& site_url,
    const GURL& tab_url,
    const GURL& tab_referrer_url,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Check if the renderer is permitted to request the requested URL.
  if (params->render_process_host_id() >= 0 &&
      !CanRequestURLFromRenderer(params->render_process_host_id(),
                                 params->url())) {
    CreateInterruptedDownload(
        params.get(),
        download::DOWNLOAD_INTERRUPT_REASON_NETWORK_INVALID_REQUEST,
        download_manager);
    return nullptr;
  }

  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory;
  if (params->url().SchemeIs(url::kBlobScheme)) {
    network::mojom::URLLoaderFactoryPtrInfo url_loader_factory_ptr_info;
    storage::BlobURLLoaderFactory::Create(
        std::move(blob_data_handle), params->url(),
        mojo::MakeRequest(&url_loader_factory_ptr_info));
    shared_url_loader_factory =
        base::MakeRefCounted<WrapperSharedURLLoaderFactory>(
            std::move(url_loader_factory_ptr_info));
  } else {
    shared_url_loader_factory = url_loader_factory_getter->GetNetworkFactory();
  }
  // TODO(qinmin): Check the storage permission before creating the URLLoader.
  // This is already done for context menu download, but it is missing for
  // download service and download resumption.
  return DownloadManagerImpl::UniqueUrlDownloadHandlerPtr(
      download::ResourceDownloader::BeginDownload(
          download_manager, std::move(params), std::move(request),
          std::move(shared_url_loader_factory), site_url, tab_url,
          tab_referrer_url, download_id, false, task_runner)
          .release());
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
      download::DownloadItem::DownloadState state,
      download::DownloadDangerType danger_type,
      download::DownloadInterruptReason interrupt_reason,
      bool opened,
      base::Time last_access_time,
      bool transient,
      const std::vector<download::DownloadItem::ReceivedSlice>& received_slices)
      override {
    return new DownloadItemImpl(
        delegate, guid, download_id, current_path, target_path, url_chain,
        referrer_url, site_url, tab_url, tab_refererr_url, mime_type,
        original_mime_type, start_time, end_time, etag, last_modified,
        received_bytes, total_bytes, hash, state, danger_type, interrupt_reason,
        opened, last_access_time, transient, received_slices);
  }

  DownloadItemImpl* CreateActiveItem(
      DownloadItemImplDelegate* delegate,
      uint32_t download_id,
      const download::DownloadCreateInfo& info) override {
    return new DownloadItemImpl(delegate, download_id, info);
  }

  DownloadItemImpl* CreateSavePageItem(
      DownloadItemImplDelegate* delegate,
      uint32_t download_id,
      const base::FilePath& path,
      const GURL& url,
      const std::string& mime_type,
      std::unique_ptr<download::DownloadRequestHandleInterface> request_handle)
      override {
    return new DownloadItemImpl(delegate, download_id, path, url, mime_type,
                                std::move(request_handle));
  }
};

#if defined(USE_X11)
base::FilePath GetTemporaryDownloadDirectory() {
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  return base::nix::GetXDGDirectory(env.get(), "XDG_DATA_HOME", ".local/share");
}
#endif

}  // namespace

// Responsible for persisting the in-progress metadata associated with a
// download.
class InProgressDownloadObserver : public download::DownloadItem::Observer {
 public:
  explicit InProgressDownloadObserver(
      download::InProgressCache* in_progress_cache);
  ~InProgressDownloadObserver() override;

 private:
  // download::DownloadItem::Observer
  void OnDownloadUpdated(download::DownloadItem* download) override;
  void OnDownloadRemoved(download::DownloadItem* download) override;

  // The persistent cache to store in-progress metadata.
  download::InProgressCache* in_progress_cache_;

  DISALLOW_COPY_AND_ASSIGN(InProgressDownloadObserver);
};

InProgressDownloadObserver::InProgressDownloadObserver(
    download::InProgressCache* in_progress_cache)
    : in_progress_cache_(in_progress_cache) {}

InProgressDownloadObserver::~InProgressDownloadObserver() = default;

void InProgressDownloadObserver::OnDownloadUpdated(
    download::DownloadItem* download) {
  // TODO(crbug.com/778425): Properly handle fail/resume/retry for downloads
  // that are in the INTERRUPTED state for a long time.
  if (!in_progress_cache_)
    return;

  switch (download->GetState()) {
    case download::DownloadItem::DownloadState::COMPLETE:
    // Intentional fallthrough.
    case download::DownloadItem::DownloadState::CANCELLED:
      if (in_progress_cache_)
        in_progress_cache_->RemoveEntry(download->GetGuid());
      break;

    case download::DownloadItem::DownloadState::INTERRUPTED:
    // Intentional fallthrough.
    case download::DownloadItem::DownloadState::IN_PROGRESS: {
      // Make sure the entry exists in the cache.
      base::Optional<download::DownloadEntry> entry_opt =
          in_progress_cache_->RetrieveEntry(download->GetGuid());
      download::DownloadEntry entry;
      if (!entry_opt.has_value()) {
        entry = CreateDownloadEntryFromItem(*download);
        in_progress_cache_->AddOrReplaceEntry(entry);
        break;
      }
      entry = entry_opt.value();
      break;
    }

    default:
      break;
  }
}

void InProgressDownloadObserver::OnDownloadRemoved(
    download::DownloadItem* download) {
  if (!in_progress_cache_)
    return;

  in_progress_cache_->RemoveEntry(download->GetGuid());
}

DownloadManagerImpl::DownloadManagerImpl(BrowserContext* browser_context)
    : item_factory_(new DownloadItemFactoryImpl()),
      file_factory_(new download::DownloadFileFactory()),
      shutdown_needed_(true),
      initialized_(false),
      history_db_initialized_(false),
      in_progress_cache_initialized_(false),
      browser_context_(browser_context),
      delegate_(nullptr),
      weak_factory_(this) {
  DCHECK(browser_context);
}

DownloadManagerImpl::~DownloadManagerImpl() {
  DCHECK(!shutdown_needed_);
}

DownloadItemImpl* DownloadManagerImpl::CreateActiveItem(
    uint32_t id,
    const download::DownloadCreateInfo& info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!base::ContainsKey(downloads_, id));
  DownloadItemImpl* download = item_factory_->CreateActiveItem(this, id, info);

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
  static uint32_t next_id = download::DownloadItem::kInvalidId + 1;
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
                 download::DownloadItem::TARGET_DISPOSITION_OVERWRITE,
                 download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS, target_path,
                 download::DOWNLOAD_INTERRUPT_REASON_NONE);
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

  // If initialization has not occurred and there's a delegate and cache,
  // initialize and indicate initialization is complete. Else, just indicate it
  // is ok to continue.
  if (initialized_ || in_progress_cache_initialized_)
    return;

  base::RepeatingClosure post_init_callback = base::BindRepeating(
      &DownloadManagerImpl::PostInitialization, weak_factory_.GetWeakPtr(),
      DOWNLOAD_INITIALIZATION_DEPENDENCY_IN_PROGRESS_CACHE);

  if (delegate_) {
    download::InProgressCache* in_progress_cache =
        delegate_->GetInProgressCache();
    if (in_progress_cache) {
      in_progress_cache->Initialize(std::move(post_init_callback));
      return;
    }
  }

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          std::move(post_init_callback));
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
    if (download->GetState() == download::DownloadItem::IN_PROGRESS)
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

bool DownloadManagerImpl::InterceptDownload(
    const download::DownloadCreateInfo& info) {
  WebContents* web_contents = WebContentsImpl::FromRenderFrameHostID(
      info.render_process_id, info.render_frame_id);
  if (!delegate_ ||
      !delegate_->InterceptDownloadIfApplicable(
          info.url(), info.mime_type, info.request_origin, web_contents)) {
    return false;
  }
  if (info.request_handle)
    info.request_handle->CancelRequest(false);
  return true;
}

void DownloadManagerImpl::StartDownload(
    std::unique_ptr<download::DownloadCreateInfo> info,
    std::unique_ptr<download::InputStream> stream,
    const download::DownloadUrlParameters::OnStartedCallback& on_started) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(info);

  uint32_t download_id = info->download_id;
  bool new_download = (download_id == download::DownloadItem::kInvalidId);
  if (new_download &&
      info->result == download::DOWNLOAD_INTERRUPT_REASON_NONE &&
      InterceptDownload(*info)) {
    download::GetDownloadTaskRunner()->DeleteSoon(FROM_HERE, stream.release());
    return;
  }

  // |stream| is only non-nil if the download request was successful.
  DCHECK((info->result == download::DOWNLOAD_INTERRUPT_REASON_NONE &&
          !stream->IsEmpty()) ||
         (info->result != download::DOWNLOAD_INTERRUPT_REASON_NONE &&
          stream->IsEmpty()));
  DVLOG(20) << __func__ << "() result="
            << download::DownloadInterruptReasonToString(info->result);
  if (new_download)
    download::RecordDownloadConnectionSecurity(info->url(), info->url_chain);
  base::Callback<void(uint32_t)> got_id(base::Bind(
      &DownloadManagerImpl::StartDownloadWithId, weak_factory_.GetWeakPtr(),
      base::Passed(&info), base::Passed(&stream), on_started, new_download));
  if (new_download) {
    GetNextId(std::move(got_id));
  } else {
    std::move(got_id).Run(download_id);
  }
}

void DownloadManagerImpl::StartDownloadWithId(
    std::unique_ptr<download::DownloadCreateInfo> info,
    std::unique_ptr<download::InputStream> stream,
    const download::DownloadUrlParameters::OnStartedCallback& on_started,
    bool new_download,
    uint32_t id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_NE(download::DownloadItem::kInvalidId, id);
  DownloadItemImpl* download = nullptr;
  if (new_download) {
    download = CreateActiveItem(id, *info);
  } else {
    auto item_iterator = downloads_.find(id);
    // Trying to resume an interrupted download.
    if (item_iterator == downloads_.end() ||
        (item_iterator->second->GetState() ==
         download::DownloadItem::CANCELLED)) {
      // If the download is no longer known to the DownloadManager, then it was
      // removed after it was resumed. Ignore. If the download is cancelled
      // while resuming, then also ignore the request.
      if (info->request_handle)
        info->request_handle->CancelRequest(true);
      if (!on_started.is_null())
        on_started.Run(nullptr,
                       download::DOWNLOAD_INTERRUPT_REASON_USER_CANCELED);
      // The ByteStreamReader lives and dies on the download sequence.
      if (info->result == download::DOWNLOAD_INTERRUPT_REASON_NONE)
        download::GetDownloadTaskRunner()->DeleteSoon(FROM_HERE,
                                                      stream.release());
      return;
    }
    download = item_iterator->second.get();
  }
  DownloadItemUtils::AttachInfo(
      download, GetBrowserContext(),
      WebContentsImpl::FromRenderFrameHostID(info->render_process_id,
                                             info->render_frame_id));

  base::FilePath default_download_directory;
#if defined(USE_X11)
  // TODO(thomasanderson,crbug.com/784010): Remove this when all Linux
  // distros with versions of GTK lower than 3.14.7 are no longer
  // supported.  This should happen when support for Ubuntu Trusty and
  // Debian Jessie are removed.
  default_download_directory = GetTemporaryDownloadDirectory();
#else
  if (delegate_) {
    base::FilePath website_save_directory;  // Unused
    bool skip_dir_check = false;            // Unused
    delegate_->GetSaveDir(GetBrowserContext(), &website_save_directory,
                          &default_download_directory, &skip_dir_check);
  }
#endif
  if (default_download_directory.empty()) {
    // |default_download_directory| can still be empty if ContentBrowserClient
    // returned an empty path for the downloads directory.
    default_download_directory =
        GetContentClient()->browser()->GetDefaultDownloadDirectory();
  }

  if (delegate_) {
    download::InProgressCache* in_progress_cache =
        delegate_->GetInProgressCache();
    if (in_progress_cache) {
      base::Optional<download::DownloadEntry> entry_opt =
          in_progress_cache->RetrieveEntry(download->GetGuid());
      if (!entry_opt.has_value()) {
        in_progress_cache->AddOrReplaceEntry(
            CreateDownloadEntryFromItemImpl(*download));
      }
    }

    if (!in_progress_download_observer_) {
      in_progress_download_observer_.reset(
          new InProgressDownloadObserver(delegate_->GetInProgressCache()));
    }
    // May already observe this item, remove observer first.
    download->RemoveObserver(in_progress_download_observer_.get());
    download->AddObserver(in_progress_download_observer_.get());
  }

  std::unique_ptr<download::DownloadFile> download_file;

  if (info->result == download::DOWNLOAD_INTERRUPT_REASON_NONE) {
    DCHECK(stream.get());
    download_file.reset(file_factory_->CreateFile(
        std::move(info->save_info), default_download_directory,
        std::move(stream), id, download->DestinationObserverAsWeakPtr()));
  }
  // It is important to leave info->save_info intact in the case of an interrupt
  // so that the download::DownloadItem can salvage what it can out of a failed
  // resumption attempt.

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
    on_started.Run(download, download::DOWNLOAD_INTERRUPT_REASON_NONE);
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
  if ((download_item->GetState() == download::DownloadItem::COMPLETE) &&
      !download_item->GetFileExternallyRemoved() && delegate_) {
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
    int render_process_id,
    int render_frame_id,
    std::unique_ptr<download::DownloadRequestHandleInterface> request_handle,
    const ukm::SourceId ukm_source_id,
    const DownloadItemImplCreated& item_created) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetNextId(base::Bind(
      &DownloadManagerImpl::CreateSavePackageDownloadItemWithId,
      weak_factory_.GetWeakPtr(), main_file_path, page_url, mime_type,
      render_process_id, render_frame_id,
      base::Passed(std::move(request_handle)), ukm_source_id, item_created));
}

void DownloadManagerImpl::CreateSavePackageDownloadItemWithId(
    const base::FilePath& main_file_path,
    const GURL& page_url,
    const std::string& mime_type,
    int render_process_id,
    int render_frame_id,
    std::unique_ptr<download::DownloadRequestHandleInterface> request_handle,
    const ukm::SourceId ukm_source_id,
    const DownloadItemImplCreated& item_created,
    uint32_t id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_NE(download::DownloadItem::kInvalidId, id);
  DCHECK(!base::ContainsKey(downloads_, id));
  DownloadItemImpl* download_item = item_factory_->CreateSavePageItem(
      this, id, main_file_path, page_url, mime_type, std::move(request_handle));
  DownloadItemUtils::AttachInfo(download_item, GetBrowserContext(),
                                WebContentsImpl::FromRenderFrameHostID(
                                    render_process_id, render_frame_id));
  downloads_[download_item->GetId()] = base::WrapUnique(download_item);
  DCHECK(!base::ContainsKey(downloads_by_guid_, download_item->GetGuid()));
  downloads_by_guid_[download_item->GetGuid()] = download_item;
  for (auto& observer : observers_)
    observer.OnDownloadCreated(this, download_item);
  if (!item_created.is_null())
    item_created.Run(download_item);

  // Add download_id and source_id for UKM.
  auto* delegate = GetDelegate();
  if (delegate) {
    download::InProgressCache* in_progress_cache =
        delegate_->GetInProgressCache();
    if (in_progress_cache) {
      base::Optional<download::DownloadEntry> entry_opt =
          in_progress_cache->RetrieveEntry(download_item->GetGuid());
      if (!entry_opt.has_value()) {
        in_progress_cache->AddOrReplaceEntry(
            CreateDownloadEntryFromItemImpl(*download_item));
      }
    }
  }
}

// Resume a download of a specific URL. We send the request to the
// ResourceDispatcherHost, and let it send us responses like a regular
// download.
void DownloadManagerImpl::ResumeInterruptedDownload(
    std::unique_ptr<download::DownloadUrlParameters> params,
    uint32_t id,
    StoragePartitionImpl* storage_partition) {
  BeginDownloadInternal(std::move(params), nullptr, id, storage_partition);
}


void DownloadManagerImpl::SetDownloadItemFactoryForTesting(
    std::unique_ptr<DownloadItemFactory> item_factory) {
  item_factory_ = std::move(item_factory);
}

void DownloadManagerImpl::SetDownloadFileFactoryForTesting(
    std::unique_ptr<download::DownloadFileFactory> file_factory) {
  file_factory_ = std::move(file_factory);
}

download::DownloadFileFactory*
DownloadManagerImpl::GetDownloadFileFactoryForTesting() {
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
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (downloader)
    url_download_handlers_.push_back(std::move(downloader));
}

// static
download::DownloadInterruptReason DownloadManagerImpl::BeginDownloadRequest(
    std::unique_ptr<net::URLRequest> url_request,
    ResourceContext* resource_context,
    download::DownloadUrlParameters* params) {
  if (ResourceDispatcherHostImpl::Get()->is_shutdown())
    return download::DOWNLOAD_INTERRUPT_REASON_USER_SHUTDOWN;

  // The URLRequest needs to be initialized with the referrer and other
  // information prior to issuing it.
  ResourceDispatcherHostImpl::Get()->InitializeURLRequest(
      url_request.get(),
      Referrer(params->referrer(),
               Referrer::NetReferrerPolicyToBlinkReferrerPolicy(
                   params->referrer_policy())),
      true,  // download.
      params->render_process_host_id(), params->render_view_host_routing_id(),
      params->render_frame_host_routing_id(), PREVIEWS_OFF, resource_context);

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
  if (!CanRequestURLFromRenderer(params->render_process_host_id(), url))
    return download::DOWNLOAD_INTERRUPT_REASON_NETWORK_INVALID_REQUEST;

  const net::URLRequestContext* request_context = url_request->context();
  if (!request_context->job_factory()->IsHandledProtocol(url.scheme())) {
    DVLOG(1) << "Download request for unsupported protocol: "
             << url.possibly_invalid_spec();
    return download::DOWNLOAD_INTERRUPT_REASON_NETWORK_INVALID_REQUEST;
  }

  // From this point forward, the |DownloadResourceHandler| is responsible for
  // |started_callback|.
  // TODO(ananta)
  // Find a better way to create the DownloadResourceHandler instance.
  std::unique_ptr<ResourceHandler> handler(
      DownloadResourceHandler::CreateForNewRequest(url_request.get(),
                                                   params->request_origin(),
                                                   params->download_source()));

  ResourceDispatcherHostImpl::Get()->BeginURLRequest(
      std::move(url_request), std::move(handler), true,  // download
      params->content_initiated(), params->do_not_prompt_for_login(),
      resource_context);
  return download::DOWNLOAD_INTERRUPT_REASON_NONE;
}

void DownloadManagerImpl::InterceptNavigation(
    std::unique_ptr<network::ResourceRequest> resource_request,
    std::vector<GURL> url_chain,
    const base::Optional<std::string>& suggested_filename,
    scoped_refptr<network::ResourceResponse> response,
    network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
    net::CertStatus cert_status,
    int frame_tree_node_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!delegate_) {
    for (auto& observer : observers_)
      observer.OnDownloadDropped(this);
    return;
  }

  const GURL& url = resource_request->url;
  const std::string& method = resource_request->method;

  ResourceRequestInfo::WebContentsGetter web_contents_getter =
      base::BindRepeating(WebContents::FromFrameTreeNodeId, frame_tree_node_id);

  base::OnceCallback<void(bool /* download allowed */)>
      on_download_checks_done = base::BindOnce(
          &DownloadManagerImpl::InterceptNavigationOnChecksComplete,
          weak_factory_.GetWeakPtr(), web_contents_getter,
          std::move(resource_request), std::move(url_chain), suggested_filename,
          std::move(response), cert_status,
          std::move(url_loader_client_endpoints));

  delegate_->CheckDownloadAllowed(std::move(web_contents_getter), url, method,
                                  std::move(on_download_checks_done));
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

    if (download->GetState() != download::DownloadItem::IN_PROGRESS &&
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
    std::unique_ptr<download::DownloadUrlParameters> params) {
  DownloadUrl(std::move(params), nullptr);
}

void DownloadManagerImpl::DownloadUrl(
    std::unique_ptr<download::DownloadUrlParameters> params,
    std::unique_ptr<storage::BlobDataHandle> blob_data_handle) {
  if (params->post_id() >= 0) {
    // Check this here so that the traceback is more useful.
    DCHECK(params->prefer_cache());
    DCHECK_EQ("POST", params->method());
  }

  download::RecordDownloadCountWithSource(
      download::DownloadCountTypes::DOWNLOAD_TRIGGERED_COUNT,
      params->download_source());
  StoragePartitionImpl* storage_partition =
      GetStoragePartition(browser_context_, params->render_process_host_id(),
                          params->render_frame_host_routing_id());
  BeginDownloadInternal(std::move(params), std::move(blob_data_handle),
                        download::DownloadItem::kInvalidId, storage_partition);
}

void DownloadManagerImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DownloadManagerImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

download::DownloadItem* DownloadManagerImpl::CreateDownloadItem(
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
    download::DownloadItem::DownloadState state,
    download::DownloadDangerType danger_type,
    download::DownloadInterruptReason interrupt_reason,
    bool opened,
    base::Time last_access_time,
    bool transient,
    const std::vector<download::DownloadItem::ReceivedSlice>& received_slices) {
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
      transient, received_slices);
  DownloadItemUtils::AttachInfo(item, GetBrowserContext(), nullptr);
  downloads_[id] = base::WrapUnique(item);
  downloads_by_guid_[guid] = item;
  for (auto& observer : observers_)
    observer.OnDownloadCreated(this, item);
  DVLOG(20) << __func__ << "() download = " << item->DebugString(true);
  return item;
}

void DownloadManagerImpl::PostInitialization(
    DownloadInitializationDependency dependency) {
  // If initialization has occurred (ie. in tests), skip post init steps.
  if (initialized_)
    return;

  switch (dependency) {
    case DOWNLOAD_INITIALIZATION_DEPENDENCY_HISTORY_DB:
      history_db_initialized_ = true;
      break;
    case DOWNLOAD_INITIALIZATION_DEPENDENCY_IN_PROGRESS_CACHE:
      in_progress_cache_initialized_ = true;
      break;
    case DOWNLOAD_INITIALIZATION_DEPENDENCY_NONE:
    default:
      NOTREACHED();
      break;
  }

  // Download manager is only initialized if both history db and in progress
  // cache are initialized.
  initialized_ = history_db_initialized_ && in_progress_cache_initialized_;

  if (initialized_) {
    for (auto& observer : observers_)
      observer.OnManagerInitialized();
  }
}

bool DownloadManagerImpl::IsManagerInitialized() const {
  return initialized_;
}

int DownloadManagerImpl::InProgressCount() const {
  int count = 0;
  for (const auto& it : downloads_) {
    if (it.second->GetState() == download::DownloadItem::IN_PROGRESS)
      ++count;
  }
  return count;
}

int DownloadManagerImpl::NonMaliciousInProgressCount() const {
  int count = 0;
  for (const auto& it : downloads_) {
    if (it.second->GetState() == download::DownloadItem::IN_PROGRESS &&
        it.second->GetDangerType() !=
            download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL &&
        it.second->GetDangerType() !=
            download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT &&
        it.second->GetDangerType() !=
            download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST &&
        it.second->GetDangerType() !=
            download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED) {
      ++count;
    }
  }
  return count;
}

download::DownloadItem* DownloadManagerImpl::GetDownload(uint32_t download_id) {
  return base::ContainsKey(downloads_, download_id)
             ? downloads_[download_id].get()
             : nullptr;
}

download::DownloadItem* DownloadManagerImpl::GetDownloadByGuid(
    const std::string& guid) {
  return base::ContainsKey(downloads_by_guid_, guid) ? downloads_by_guid_[guid]
                                                     : nullptr;
}

void DownloadManagerImpl::OnUrlDownloadStarted(
    std::unique_ptr<download::DownloadCreateInfo> download_create_info,
    std::unique_ptr<download::InputStream> stream,
    const download::DownloadUrlParameters::OnStartedCallback& callback) {
  StartDownload(std::move(download_create_info), std::move(stream), callback);
}

void DownloadManagerImpl::OnUrlDownloadStopped(
    download::UrlDownloadHandler* downloader) {
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
    if ((item->GetState() == download::DownloadItem::COMPLETE) &&
        !item->GetOpened())
      ++num_unopened;
  }
  download::RecordOpensOutstanding(num_unopened);

  if (delegate_)
    delegate_->OpenDownload(download);
}

bool DownloadManagerImpl::IsMostRecentDownloadItemAtFilePath(
    DownloadItemImpl* download) {
  return delegate_ ? delegate_->IsMostRecentDownloadItemAtFilePath(download)
                   : false;
}

void DownloadManagerImpl::ShowDownloadInShell(DownloadItemImpl* download) {
  if (delegate_)
    delegate_->ShowDownloadInShell(download);
}

void DownloadManagerImpl::InterceptNavigationOnChecksComplete(
    ResourceRequestInfo::WebContentsGetter web_contents_getter,
    std::unique_ptr<network::ResourceRequest> resource_request,
    std::vector<GURL> url_chain,
    const base::Optional<std::string>& suggested_filename,
    scoped_refptr<network::ResourceResponse> response,
    net::CertStatus cert_status,
    network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
    bool is_download_allowed) {
  if (!is_download_allowed) {
    for (auto& observer : observers_)
      observer.OnDownloadDropped(this);
    return;
  }

  int render_process_id = -1;
  int render_frame_id = -1;
  GURL site_url, tab_url, tab_referrer_url;
  WebContents* web_contents = std::move(web_contents_getter).Run();
  if (web_contents) {
    RenderFrameHost* render_frame_host = web_contents->GetMainFrame();
    if (render_frame_host) {
      render_process_id = render_frame_host->GetProcess()->GetID();
      render_frame_id = render_frame_host->GetRoutingID();
      site_url = render_frame_host->GetSiteInstance()->GetSiteURL();
    }
    NavigationEntry* entry = web_contents->GetController().GetVisibleEntry();
    if (entry) {
      tab_url = entry->GetURL();
      tab_referrer_url = entry->GetReferrer().url;
    }
  }
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&DownloadManagerImpl::CreateDownloadHandlerForNavigation,
                     weak_factory_.GetWeakPtr(), std::move(resource_request),
                     render_process_id, render_frame_id, site_url, tab_url,
                     tab_referrer_url, std::move(url_chain), suggested_filename,
                     std::move(response), std::move(cert_status),
                     std::move(url_loader_client_endpoints),
                     base::MessageLoop::current()->task_runner()));
}

// static
void DownloadManagerImpl::CreateDownloadHandlerForNavigation(
    base::WeakPtr<DownloadManagerImpl> download_manager,
    std::unique_ptr<network::ResourceRequest> resource_request,
    int render_process_id,
    int render_frame_id,
    const GURL& site_url,
    const GURL& tab_url,
    const GURL& tab_referrer_url,
    std::vector<GURL> url_chain,
    const base::Optional<std::string>& suggested_filename,
    scoped_refptr<network::ResourceResponse> response,
    net::CertStatus cert_status,
    network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  std::unique_ptr<download::ResourceDownloader> resource_downloader =
      download::ResourceDownloader::InterceptNavigationResponse(
          download_manager, std::move(resource_request), render_process_id,
          render_frame_id, site_url, tab_url, tab_referrer_url,
          std::move(url_chain), suggested_filename, std::move(response),
          std::move(cert_status), std::move(url_loader_client_endpoints),
          task_runner);

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&DownloadManagerImpl::AddUrlDownloadHandler,
                     download_manager,
                     DownloadManagerImpl::UniqueUrlDownloadHandlerPtr(
                         resource_downloader.release())));
}

void DownloadManagerImpl::BeginDownloadInternal(
    std::unique_ptr<download::DownloadUrlParameters> params,
    std::unique_ptr<storage::BlobDataHandle> blob_data_handle,
    uint32_t id,
    StoragePartitionImpl* storage_partition) {
  if (base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    std::unique_ptr<network::ResourceRequest> request =
        CreateResourceRequest(params.get());
    GURL site_url, tab_url, tab_referrer_url;
    auto* rfh = RenderFrameHost::FromID(params->render_process_host_id(),
                                        params->render_frame_host_routing_id());
    if (rfh) {
      site_url = rfh->GetSiteInstance()->GetSiteURL();
      auto* web_contents = WebContents::FromRenderFrameHost(rfh);
      NavigationEntry* entry = web_contents->GetController().GetVisibleEntry();
      if (entry) {
        tab_url = entry->GetURL();
        tab_referrer_url = entry->GetReferrer().url;
      }
    }

    BrowserThread::PostTaskAndReplyWithResult(
        BrowserThread::IO, FROM_HERE,
        base::BindOnce(&BeginResourceDownload, std::move(params),
                       std::move(request), std::move(blob_data_handle),
                       storage_partition->url_loader_factory_getter(), id,
                       weak_factory_.GetWeakPtr(), site_url, tab_url,
                       tab_referrer_url,
                       base::MessageLoop::current()->task_runner()),
        base::BindOnce(&DownloadManagerImpl::AddUrlDownloadHandler,
                       weak_factory_.GetWeakPtr()));
  } else {
    BrowserThread::PostTaskAndReplyWithResult(
        BrowserThread::IO, FROM_HERE,
        base::BindOnce(&BeginDownload, std::move(params),
                       std::move(blob_data_handle),
                       browser_context_->GetResourceContext(), id,
                       weak_factory_.GetWeakPtr()),
        base::BindOnce(&DownloadManagerImpl::AddUrlDownloadHandler,
                       weak_factory_.GetWeakPtr()));
   }
}

}  // namespace content
