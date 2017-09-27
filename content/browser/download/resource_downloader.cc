// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/resource_downloader.h"

#include "content/browser/download/download_utils.h"
#include "content/common/throttling_url_loader.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace content {

// This class is only used for providing the WebContents to DownloadItemImpl.
class RequestHandle : public DownloadRequestHandleInterface {
 public:
  RequestHandle(int render_process_id, int render_frame_id)
      : render_process_id_(render_process_id),
        render_frame_id_(render_frame_id) {}
  RequestHandle(RequestHandle&& other)
      : render_process_id_(other.render_process_id_),
        render_frame_id_(other.render_frame_id_) {}

  // DownloadRequestHandleInterface
  WebContents* GetWebContents() const override {
    RenderFrameHost* host =
        RenderFrameHost::FromID(render_process_id_, render_frame_id_);
    if (host)
      return WebContents::FromRenderFrameHost(host);
    return nullptr;
  }
  DownloadManager* GetDownloadManager() const override { return nullptr; }
  void PauseRequest() const override {}
  void ResumeRequest() const override {}
  void CancelRequest(bool user_cancel) const override {}

 private:
  int render_process_id_;
  int render_frame_id_;

  DISALLOW_COPY_AND_ASSIGN(RequestHandle);
};

// static
std::unique_ptr<ResourceDownloader> ResourceDownloader::BeginDownload(
    base::WeakPtr<UrlDownloadHandler::Delegate> delegate,
    std::unique_ptr<DownloadUrlParameters> params,
    std::unique_ptr<ResourceRequest> request,
    scoped_refptr<URLLoaderFactoryGetter> url_loader_factory_getter,
    uint32_t download_id,
    bool is_parallel_request) {
  mojom::URLLoaderFactoryPtr* factory =
      params->url().SchemeIs(url::kBlobScheme)
          ? url_loader_factory_getter->GetBlobFactory()
          : url_loader_factory_getter->GetNetworkFactory();
  auto downloader = base::MakeUnique<ResourceDownloader>(
      delegate, std::move(request),
      base::MakeUnique<DownloadSaveInfo>(params->GetSaveInfo()), download_id,
      params->guid(), is_parallel_request, params->is_transient());
  downloader->Start(factory, std::move(params));

  return downloader;
}

// static
std::unique_ptr<ResourceDownloader>
ResourceDownloader::InterceptNavigationResponse(
    base::WeakPtr<UrlDownloadHandler::Delegate> delegate,
    std::unique_ptr<ResourceRequest> resource_request,
    const scoped_refptr<ResourceResponse>& response,
    mojo::ScopedDataPipeConsumerHandle consumer_handle,
    const SSLStatus& ssl_status,
    std::unique_ptr<ThrottlingURLLoader> url_loader,
    base::Optional<ResourceRequestCompletionStatus> completion_status) {
  auto downloader = base::MakeUnique<ResourceDownloader>(
      delegate, std::move(resource_request),
      base::MakeUnique<DownloadSaveInfo>(), content::DownloadItem::kInvalidId,
      std::string(), false, false);
  downloader->InterceptResponse(std::move(url_loader), response,
                                std::move(consumer_handle), ssl_status,
                                std::move(completion_status));
  return downloader;
}

ResourceDownloader::ResourceDownloader(
    base::WeakPtr<UrlDownloadHandler::Delegate> delegate,
    std::unique_ptr<ResourceRequest> resource_request,
    std::unique_ptr<DownloadSaveInfo> save_info,
    uint32_t download_id,
    std::string guid,
    bool is_parallel_request,
    bool is_transient)
    : delegate_(delegate),
      resource_request_(std::move(resource_request)),
      response_handler_(resource_request_.get(),
                        this,
                        std::move(save_info),
                        is_parallel_request,
                        is_transient),
      download_id_(download_id),
      guid_(guid),
      weak_ptr_factory_(this) {}

ResourceDownloader::~ResourceDownloader() = default;

void ResourceDownloader::Start(
    mojom::URLLoaderFactoryPtr* factory,
    std::unique_ptr<DownloadUrlParameters> download_url_parameters) {
  callback_ = download_url_parameters->callback();
  url_loader_ = ThrottlingURLLoader::CreateLoaderAndStart(
      factory->get(), std::vector<std::unique_ptr<URLLoaderThrottle>>(),
      0,  // routing_id
      0,  // request_id
      mojom::kURLLoadOptionSendSSLInfo | mojom::kURLLoadOptionSniffMimeType,
      *(resource_request_.get()), &response_handler_,
      download_url_parameters->GetNetworkTrafficAnnotation());
  url_loader_->SetPriority(net::RequestPriority::IDLE,
                           0 /* intra_priority_value */);
}

void ResourceDownloader::InterceptResponse(
    std::unique_ptr<ThrottlingURLLoader> url_loader,
    const scoped_refptr<ResourceResponse>& response,
    mojo::ScopedDataPipeConsumerHandle consumer_handle,
    const SSLStatus& ssl_status,
    base::Optional<ResourceRequestCompletionStatus> completion_status) {
  url_loader_ = std::move(url_loader);
  url_loader_->set_forwarding_client(&response_handler_);
  net::SSLInfo info;
  info.cert_status = ssl_status.cert_status;
  response_handler_.OnReceiveResponse(response->head,
                                      base::Optional<net::SSLInfo>(info),
                                      mojom::DownloadedTempFilePtr());
  response_handler_.OnStartLoadingResponseBody(std::move(consumer_handle));
  if (completion_status.has_value())
    response_handler_.OnComplete(completion_status.value());
}

void ResourceDownloader::OnResponseStarted(
    std::unique_ptr<DownloadCreateInfo> download_create_info,
    mojom::DownloadStreamHandlePtr stream_handle) {
  download_create_info->download_id = download_id_;
  download_create_info->guid = guid_;
  if (resource_request_->origin_pid >= 0) {
    download_create_info->request_handle.reset(new RequestHandle(
        resource_request_->origin_pid, resource_request_->render_frame_id));
  }
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&UrlDownloadHandler::Delegate::OnUrlDownloadStarted,
                     delegate_, std::move(download_create_info),
                     base::MakeUnique<DownloadManager::InputStream>(
                         std::move(stream_handle)),
                     callback_));
}

void ResourceDownloader::OnReceiveRedirect() {
  url_loader_->FollowRedirect();
}

}  // namespace content
