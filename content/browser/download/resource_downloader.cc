// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/resource_downloader.h"

#include <memory>

#include "content/browser/blob_storage/blob_url_loader_factory.h"
#include "content/browser/download/download_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "storage/browser/fileapi/file_system_context.h"

namespace content {

// This object monitors the URLLoaderCompletionStatus change when
// ResourceDownloader is asking |delegate_| whether download can proceed.
class URLLoaderStatusMonitor : public mojom::URLLoaderClient {
 public:
  using URLLoaderStatusChangeCallback =
      base::OnceCallback<void(const network::URLLoaderCompletionStatus&)>;
  explicit URLLoaderStatusMonitor(URLLoaderStatusChangeCallback callback);
  ~URLLoaderStatusMonitor() override = default;

  // mojom::URLLoaderClient
  void OnReceiveResponse(
      const ResourceResponseHead& head,
      const base::Optional<net::SSLInfo>& ssl_info,
      mojom::DownloadedTempFilePtr downloaded_file) override {}
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         const ResourceResponseHead& head) override {}
  void OnDataDownloaded(int64_t data_length, int64_t encoded_length) override {}
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback callback) override {}
  void OnReceiveCachedMetadata(const std::vector<uint8_t>& data) override {}
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override {}
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override {}
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

 private:
  URLLoaderStatusChangeCallback callback_;
  DISALLOW_COPY_AND_ASSIGN(URLLoaderStatusMonitor);
};

URLLoaderStatusMonitor::URLLoaderStatusMonitor(
    URLLoaderStatusChangeCallback callback)
    : callback_(std::move(callback)) {}

void URLLoaderStatusMonitor::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  std::move(callback_).Run(status);
}

// This class is only used for providing the WebContents to DownloadItemImpl.
class RequestHandle : public DownloadRequestHandleInterface {
 public:
  explicit RequestHandle(const ResourceRequestInfo::WebContentsGetter& getter)
      : web_contents_getter_(getter) {}
  RequestHandle(RequestHandle&& other)
      : web_contents_getter_(other.web_contents_getter_) {}

  // DownloadRequestHandleInterface
  WebContents* GetWebContents() const override {
    return web_contents_getter_.Run();
  }

  DownloadManager* GetDownloadManager() const override { return nullptr; }
  void PauseRequest() const override {}
  void ResumeRequest() const override {}
  void CancelRequest(bool user_cancel) const override {}

 private:
  ResourceRequestInfo::WebContentsGetter web_contents_getter_;

  DISALLOW_COPY_AND_ASSIGN(RequestHandle);
};

// static
std::unique_ptr<ResourceDownloader> ResourceDownloader::BeginDownload(
    base::WeakPtr<UrlDownloadHandler::Delegate> delegate,
    std::unique_ptr<DownloadUrlParameters> params,
    std::unique_ptr<ResourceRequest> request,
    scoped_refptr<URLLoaderFactoryGetter> url_loader_factory_getter,
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const ResourceRequestInfo::WebContentsGetter& web_contents_getter,
    uint32_t download_id,
    bool is_parallel_request) {
  auto downloader = std::make_unique<ResourceDownloader>(
      delegate, std::move(request), web_contents_getter, download_id);

  downloader->Start(url_loader_factory_getter, file_system_context,
                    std::move(params), is_parallel_request);
  return downloader;
}

// static
std::unique_ptr<ResourceDownloader>
ResourceDownloader::InterceptNavigationResponse(
    base::WeakPtr<UrlDownloadHandler::Delegate> delegate,
    std::unique_ptr<ResourceRequest> resource_request,
    const ResourceRequestInfo::WebContentsGetter& web_contents_getter,
    std::vector<GURL> url_chain,
    const scoped_refptr<ResourceResponse>& response,
    net::CertStatus cert_status,
    mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints) {
  auto downloader = std::make_unique<ResourceDownloader>(
      delegate, std::move(resource_request), web_contents_getter,
      DownloadItem::kInvalidId);
  downloader->InterceptResponse(std::move(response), std::move(url_chain),
                                cert_status,
                                std::move(url_loader_client_endpoints));
  return downloader;
}

ResourceDownloader::ResourceDownloader(
    base::WeakPtr<UrlDownloadHandler::Delegate> delegate,
    std::unique_ptr<ResourceRequest> resource_request,
    const ResourceRequestInfo::WebContentsGetter& web_contents_getter,
    uint32_t download_id)
    : delegate_(delegate),
      resource_request_(std::move(resource_request)),
      download_id_(download_id),
      web_contents_getter_(web_contents_getter),
      weak_ptr_factory_(this) {}

ResourceDownloader::~ResourceDownloader() = default;

void ResourceDownloader::Start(
    scoped_refptr<URLLoaderFactoryGetter> url_loader_factory_getter,
    scoped_refptr<storage::FileSystemContext> file_system_context,
    std::unique_ptr<DownloadUrlParameters> download_url_parameters,
    bool is_parallel_request) {
  callback_ = download_url_parameters->callback();
  guid_ = download_url_parameters->guid();

  // Set up the URLLoaderClient.
  url_loader_client_ = std::make_unique<DownloadResponseHandler>(
      resource_request_.get(), this,
      std::make_unique<DownloadSaveInfo>(
          download_url_parameters->GetSaveInfo()),
      is_parallel_request, download_url_parameters->is_transient(),
      download_url_parameters->fetch_error_body(),
      std::vector<GURL>(1, resource_request_->url));
  mojom::URLLoaderClientPtr url_loader_client_ptr;
  url_loader_client_binding_ =
      std::make_unique<mojo::Binding<mojom::URLLoaderClient>>(
          url_loader_client_.get(), mojo::MakeRequest(&url_loader_client_ptr));

  // Set up the URLLoader
  mojom::URLLoaderRequest url_loader_request = mojo::MakeRequest(&url_loader_);
  if (download_url_parameters->url().SchemeIs(url::kBlobScheme)) {
    BlobURLLoaderFactory::CreateLoaderAndStart(
        std::move(url_loader_request), *(resource_request_.get()),
        std::move(url_loader_client_ptr),
        download_url_parameters->GetBlobDataHandle());
  } else {
    url_loader_factory_getter->GetNetworkFactory()->CreateLoaderAndStart(
        std::move(url_loader_request),
        0,  // routing_id
        0,  // request_id
        mojom::kURLLoadOptionSendSSLInfoWithResponse |
            mojom::kURLLoadOptionSniffMimeType,
        *(resource_request_.get()), std::move(url_loader_client_ptr),
        net::MutableNetworkTrafficAnnotationTag(
            download_url_parameters->GetNetworkTrafficAnnotation()));
    url_loader_->SetPriority(net::RequestPriority::IDLE,
                             0 /* intra_priority_value */);
  }
}

void ResourceDownloader::InterceptResponse(
    const scoped_refptr<ResourceResponse>& response,
    std::vector<GURL> url_chain,
    net::CertStatus cert_status,
    mojom::URLLoaderClientEndpointsPtr endpoints) {
  // Set the URLLoader.
  url_loader_.Bind(std::move(endpoints->url_loader));

  // Create the new URLLoaderClient that will intercept the navigation.
  url_loader_client_ = std::make_unique<DownloadResponseHandler>(
      resource_request_.get(), this, std::make_unique<DownloadSaveInfo>(),
      false, false, false, std::move(url_chain));

  // Simulate on the new URLLoaderClient calls that happened on the old client.
  net::SSLInfo info;
  info.cert_status = cert_status;
  url_loader_client_->OnReceiveResponse(response->head,
                                        base::Optional<net::SSLInfo>(info),
                                        mojom::DownloadedTempFilePtr());

  // Bind the new client.
  url_loader_client_binding_ =
      base::MakeUnique<mojo::Binding<mojom::URLLoaderClient>>(
          url_loader_client_.get(), std::move(endpoints->url_loader_client));
}

void ResourceDownloader::OnResponseStarted(
    std::unique_ptr<DownloadCreateInfo> download_create_info,
    mojom::DownloadStreamHandlePtr stream_handle) {
  download_create_info->download_id = download_id_;
  download_create_info->guid = guid_;
  download_create_info->request_handle.reset(
      new RequestHandle(web_contents_getter_));

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&UrlDownloadHandler::Delegate::OnUrlDownloadStarted,
                     delegate_, std::move(download_create_info),
                     std::make_unique<DownloadManager::InputStream>(
                         std::move(stream_handle)),
                     callback_));
}

void ResourceDownloader::OnReceiveRedirect() {
  url_loader_->FollowRedirect();
}

}  // namespace content
