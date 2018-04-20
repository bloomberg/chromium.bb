// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/in_progress_download_manager.h"

#include "base/optional.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/download/internal/common/resource_downloader.h"
#include "components/download/public/common/download_task_runner.h"
#include "components/download/public/common/download_url_loader_factory_getter.h"
#include "components/download/public/common/download_url_parameters.h"
#include "components/download/public/common/download_utils.h"
#include "components/download/public/common/input_stream.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_response.h"

namespace download {

namespace {

void OnUrlDownloadHandlerCreated(
    UrlDownloadHandler::UniqueUrlDownloadHandlerPtr downloader,
    base::WeakPtr<InProgressDownloadManager> download_manager,
    const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner) {
  main_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&UrlDownloadHandler::Delegate::OnUrlDownloadHandlerCreated,
                     download_manager, std::move(downloader)));
}

void BeginResourceDownload(
    std::unique_ptr<DownloadUrlParameters> params,
    std::unique_ptr<network::ResourceRequest> request,
    scoped_refptr<DownloadURLLoaderFactoryGetter> url_loader_factory_getter,
    uint32_t download_id,
    base::WeakPtr<InProgressDownloadManager> download_manager,
    const GURL& site_url,
    const GURL& tab_url,
    const GURL& tab_referrer_url,
    const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner) {
  DCHECK(GetIOTaskRunner()->BelongsToCurrentThread());
  UrlDownloadHandler::UniqueUrlDownloadHandlerPtr downloader(
      ResourceDownloader::BeginDownload(
          download_manager, std::move(params), std::move(request),
          std::move(url_loader_factory_getter), site_url, tab_url,
          tab_referrer_url, download_id, false, main_task_runner)
          .release(),
      base::OnTaskRunnerDeleter(base::ThreadTaskRunnerHandle::Get()));

  OnUrlDownloadHandlerCreated(std::move(downloader), download_manager,
                              main_task_runner);
}

void CreateDownloadHandlerForNavigation(
    base::WeakPtr<InProgressDownloadManager> download_manager,
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
    scoped_refptr<DownloadURLLoaderFactoryGetter> url_loader_factory_getter,
    const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner) {
  DCHECK(GetIOTaskRunner()->BelongsToCurrentThread());
  download::UrlDownloadHandler::UniqueUrlDownloadHandlerPtr downloader(
      download::ResourceDownloader::InterceptNavigationResponse(
          download_manager, std::move(resource_request), render_process_id,
          render_frame_id, site_url, tab_url, tab_referrer_url,
          std::move(url_chain), suggested_filename, std::move(response),
          std::move(cert_status), std::move(url_loader_client_endpoints),
          std::move(url_loader_factory_getter), main_task_runner)
          .release(),
      base::OnTaskRunnerDeleter(base::ThreadTaskRunnerHandle::Get()));

  OnUrlDownloadHandlerCreated(std::move(downloader), download_manager,
                              main_task_runner);
}

}  // namespace

InProgressDownloadManager::InProgressDownloadManager(
    UrlDownloadHandler::Delegate* delegate)
    : delegate_(delegate), weak_factory_(this) {}

InProgressDownloadManager::~InProgressDownloadManager() = default;

void InProgressDownloadManager::OnUrlDownloadStarted(
    std::unique_ptr<DownloadCreateInfo> download_create_info,
    std::unique_ptr<InputStream> input_stream,
    scoped_refptr<DownloadURLLoaderFactoryGetter> url_loader_factory_getter,
    const DownloadUrlParameters::OnStartedCallback& callback) {
  if (delegate_) {
    delegate_->OnUrlDownloadStarted(
        std::move(download_create_info), std::move(input_stream),
        std::move(url_loader_factory_getter), callback);
  }
}

void InProgressDownloadManager::OnUrlDownloadStopped(
    UrlDownloadHandler* downloader) {
  for (auto ptr = url_download_handlers_.begin();
       ptr != url_download_handlers_.end(); ++ptr) {
    if (ptr->get() == downloader) {
      url_download_handlers_.erase(ptr);
      return;
    }
  }
}

void InProgressDownloadManager::OnUrlDownloadHandlerCreated(
    UrlDownloadHandler::UniqueUrlDownloadHandlerPtr downloader) {
  if (downloader)
    url_download_handlers_.push_back(std::move(downloader));
}

void InProgressDownloadManager::StartDownload(
    std::unique_ptr<DownloadUrlParameters> params,
    scoped_refptr<DownloadURLLoaderFactoryGetter> url_loader_factory_getter,
    uint32_t download_id,
    const GURL& site_url,
    const GURL& tab_url,
    const GURL& tab_referrer_url) {
  std::unique_ptr<network::ResourceRequest> request =
      CreateResourceRequest(params.get());
  GetIOTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&BeginResourceDownload, std::move(params),
                     std::move(request), std::move(url_loader_factory_getter),
                     download_id, weak_factory_.GetWeakPtr(), site_url, tab_url,
                     tab_referrer_url, base::ThreadTaskRunnerHandle::Get()));
}

void InProgressDownloadManager::InterceptDownloadFromNavigation(
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
    scoped_refptr<DownloadURLLoaderFactoryGetter> url_loader_factory_getter) {
  GetIOTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CreateDownloadHandlerForNavigation,
                     weak_factory_.GetWeakPtr(), std::move(resource_request),
                     render_process_id, render_frame_id, site_url, tab_url,
                     tab_referrer_url, std::move(url_chain), suggested_filename,
                     std::move(response), std::move(cert_status),
                     std::move(url_loader_client_endpoints),
                     std::move(url_loader_factory_getter),
                     base::ThreadTaskRunnerHandle::Get()));
}

void InProgressDownloadManager::ShutDown() {
  url_download_handlers_.clear();
}

void InProgressDownloadManager::ResumeInterruptedDownload(
    std::unique_ptr<DownloadUrlParameters> params,
    uint32_t id,
    const GURL& site_url) {}

}  // namespace download
