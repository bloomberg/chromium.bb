// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/resource_downloader.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "components/download/public/common/stream_handle_input_stream.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace network {
struct ResourceResponseHead;
}

namespace download {

// This object monitors the URLLoaderCompletionStatus change when
// ResourceDownloader is asking |delegate_| whether download can proceed.
class URLLoaderStatusMonitor : public network::mojom::URLLoaderClient {
 public:
  using URLLoaderStatusChangeCallback =
      base::OnceCallback<void(const network::URLLoaderCompletionStatus&)>;
  explicit URLLoaderStatusMonitor(URLLoaderStatusChangeCallback callback);
  ~URLLoaderStatusMonitor() override = default;

  // network::mojom::URLLoaderClient
  void OnReceiveResponse(
      const network::ResourceResponseHead& head,
      const base::Optional<net::SSLInfo>& ssl_info,
      network::mojom::DownloadedTempFilePtr downloaded_file) override {}
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         const network::ResourceResponseHead& head) override {}
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

// static
std::unique_ptr<ResourceDownloader> ResourceDownloader::BeginDownload(
    base::WeakPtr<UrlDownloadHandler::Delegate> delegate,
    std::unique_ptr<DownloadUrlParameters> params,
    std::unique_ptr<network::ResourceRequest> request,
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
    const GURL& site_url,
    const GURL& tab_url,
    const GURL& tab_referrer_url,
    uint32_t download_id,
    bool is_parallel_request,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) {
  auto downloader = std::make_unique<ResourceDownloader>(
      delegate, std::move(request), params->render_process_host_id(),
      params->render_frame_host_routing_id(), site_url, tab_url,
      tab_referrer_url, download_id, task_runner);

  downloader->Start(std::move(shared_url_loader_factory), std::move(params),
                    is_parallel_request);
  return downloader;
}

// static
std::unique_ptr<ResourceDownloader>
ResourceDownloader::InterceptNavigationResponse(
    base::WeakPtr<UrlDownloadHandler::Delegate> delegate,
    std::unique_ptr<network::ResourceRequest> resource_request,
    int render_process_id,
    int render_frame_id,
    const GURL& site_url,
    const GURL& tab_url,
    const GURL& tab_referrer_url,
    std::vector<GURL> url_chain,
    const base::Optional<std::string>& suggested_filename,
    const scoped_refptr<network::ResourceResponse>& response,
    net::CertStatus cert_status,
    network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) {
  auto downloader = std::make_unique<ResourceDownloader>(
      delegate, std::move(resource_request), render_process_id, render_frame_id,
      site_url, tab_url, tab_referrer_url, download::DownloadItem::kInvalidId,
      task_runner);
  downloader->InterceptResponse(std::move(response), std::move(url_chain),
                                suggested_filename, cert_status,
                                std::move(url_loader_client_endpoints));
  return downloader;
}

ResourceDownloader::ResourceDownloader(
    base::WeakPtr<UrlDownloadHandler::Delegate> delegate,
    std::unique_ptr<network::ResourceRequest> resource_request,
    int render_process_id,
    int render_frame_id,
    const GURL& site_url,
    const GURL& tab_url,
    const GURL& tab_referrer_url,
    uint32_t download_id,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
    : delegate_(delegate),
      resource_request_(std::move(resource_request)),
      download_id_(download_id),
      render_process_id_(render_process_id),
      render_frame_id_(render_frame_id),
      site_url_(site_url),
      tab_url_(tab_url),
      tab_referrer_url_(tab_referrer_url),
      delegate_task_runner_(task_runner),
      weak_ptr_factory_(this) {}

ResourceDownloader::~ResourceDownloader() = default;

void ResourceDownloader::Start(
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
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
      download_url_parameters->request_origin(),
      download_url_parameters->download_source(),
      std::vector<GURL>(1, resource_request_->url));
  network::mojom::URLLoaderClientPtr url_loader_client_ptr;
  url_loader_client_binding_ =
      std::make_unique<mojo::Binding<network::mojom::URLLoaderClient>>(
          url_loader_client_.get(), mojo::MakeRequest(&url_loader_client_ptr));

  // Set up the URLLoader
  network::mojom::URLLoaderRequest url_loader_request =
      mojo::MakeRequest(&url_loader_);
  shared_url_loader_factory->CreateLoaderAndStart(
      std::move(url_loader_request),
      0,  // routing_id
      0,  // request_id
      network::mojom::kURLLoadOptionSendSSLInfoWithResponse,
      *(resource_request_.get()), std::move(url_loader_client_ptr),
      net::MutableNetworkTrafficAnnotationTag(
          download_url_parameters->GetNetworkTrafficAnnotation()));
  url_loader_->SetPriority(net::RequestPriority::IDLE,
                           0 /* intra_priority_value */);
}

void ResourceDownloader::InterceptResponse(
    const scoped_refptr<network::ResourceResponse>& response,
    std::vector<GURL> url_chain,
    const base::Optional<std::string>& suggested_filename,
    net::CertStatus cert_status,
    network::mojom::URLLoaderClientEndpointsPtr endpoints) {
  // Set the URLLoader.
  url_loader_.Bind(std::move(endpoints->url_loader));

  // Create the new URLLoaderClient that will intercept the navigation.
  auto save_info = std::make_unique<DownloadSaveInfo>();
  if (suggested_filename.has_value())
    save_info->suggested_name = base::UTF8ToUTF16(suggested_filename.value());
  url_loader_client_ = std::make_unique<DownloadResponseHandler>(
      resource_request_.get(), this, std::move(save_info), false, false, false,
      std::string(), DownloadSource::NAVIGATION, std::move(url_chain));

  // Simulate on the new URLLoaderClient calls that happened on the old client.
  net::SSLInfo info;
  info.cert_status = cert_status;
  url_loader_client_->OnReceiveResponse(
      response->head, base::Optional<net::SSLInfo>(info),
      network::mojom::DownloadedTempFilePtr());

  // Bind the new client.
  url_loader_client_binding_ =
      std::make_unique<mojo::Binding<network::mojom::URLLoaderClient>>(
          url_loader_client_.get(), std::move(endpoints->url_loader_client));
}

void ResourceDownloader::OnResponseStarted(
    std::unique_ptr<DownloadCreateInfo> download_create_info,
    mojom::DownloadStreamHandlePtr stream_handle) {
  download_create_info->download_id = download_id_;
  download_create_info->guid = guid_;
  download_create_info->site_url = site_url_;
  download_create_info->tab_url = tab_url_;
  download_create_info->tab_referrer_url = tab_referrer_url_;
  download_create_info->render_process_id = render_process_id_;
  download_create_info->render_frame_id = render_frame_id_;

  delegate_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &UrlDownloadHandler::Delegate::OnUrlDownloadStarted, delegate_,
          std::move(download_create_info),
          std::make_unique<StreamHandleInputStream>(std::move(stream_handle)),
          callback_));
}

void ResourceDownloader::OnReceiveRedirect() {
  url_loader_->FollowRedirect();
}

}  // namespace download
