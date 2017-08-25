// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/resource_downloader.h"

#include "content/browser/download/download_utils.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/common/throttling_url_loader.h"

namespace content {

// static
std::unique_ptr<ResourceDownloader> ResourceDownloader::BeginDownload(
    base::WeakPtr<UrlDownloadHandler::Delegate> delegate,
    std::unique_ptr<DownloadUrlParameters> download_url_parameters,
    std::unique_ptr<ResourceRequest> request,
    scoped_refptr<URLLoaderFactoryGetter> url_loader_factory_getter,
    uint32_t download_id,
    bool is_parallel_request) {
  if (download_url_parameters->url().SchemeIs(url::kBlobScheme))
    return nullptr;

  auto downloader = base::MakeUnique<ResourceDownloader>(
      delegate, std::move(download_url_parameters), url_loader_factory_getter,
      download_id, is_parallel_request);
  downloader->Start(std::move(request));

  return downloader;
}

ResourceDownloader::ResourceDownloader(
    base::WeakPtr<UrlDownloadHandler::Delegate> delegate,
    std::unique_ptr<DownloadUrlParameters> download_url_parameters,
    scoped_refptr<URLLoaderFactoryGetter> url_loader_factory_getter,
    uint32_t download_id,
    bool is_parallel_request)
    : delegate_(delegate),
      download_url_parameters_(std::move(download_url_parameters)),
      url_loader_factory_getter_(url_loader_factory_getter),
      response_handler_(download_url_parameters_.get(),
                        this,
                        is_parallel_request),
      download_id_(download_id),
      weak_ptr_factory_(this) {}

ResourceDownloader::~ResourceDownloader() = default;

void ResourceDownloader::Start(std::unique_ptr<ResourceRequest> request) {
  url_loader_ = ThrottlingURLLoader::CreateLoaderAndStart(
      url_loader_factory_getter_->GetNetworkFactory()->get(),
      std::vector<std::unique_ptr<URLLoaderThrottle>>(),
      0,  // routing_id
      0,  // request_id
      mojom::kURLLoadOptionSendSSLInfo | mojom::kURLLoadOptionSniffMimeType,
      *(request.get()), &response_handler_,
      download_url_parameters_->GetNetworkTrafficAnnotation());
}

void ResourceDownloader::OnResponseStarted(
    std::unique_ptr<DownloadCreateInfo> download_create_info,
    mojo::ScopedDataPipeConsumerHandle body) {
  download_create_info->download_id = download_id_;
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(
          &UrlDownloadHandler::Delegate::OnUrlDownloadStarted, delegate_,
          std::move(download_create_info),
          base::MakeUnique<UrlDownloadHandler::InputStream>(std::move(body)),
          download_url_parameters_->callback()));
}

}  // namespace content
