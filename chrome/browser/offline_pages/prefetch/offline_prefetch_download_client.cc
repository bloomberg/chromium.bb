// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/prefetch/offline_prefetch_download_client.h"

#include "base/logging.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/offline_pages/prefetch/prefetch_service_factory.h"
#include "components/download/public/download_metadata.h"
#include "components/offline_pages/core/prefetch/prefetch_downloader.h"
#include "components/offline_pages/core/prefetch/prefetch_service.h"

namespace offline_pages {

OfflinePrefetchDownloadClient::OfflinePrefetchDownloadClient(
    content::BrowserContext* context)
    : context_(context) {}

OfflinePrefetchDownloadClient::~OfflinePrefetchDownloadClient() = default;

void OfflinePrefetchDownloadClient::OnServiceInitialized(
    bool state_lost,
    const std::vector<download::DownloadMetaData>& downloads) {
  std::vector<std::string> outstanding_download_guids;
  for (const auto& download : downloads)
    outstanding_download_guids.emplace_back(download.guid);

  PrefetchDownloader* downloader = GetPrefetchDownloader();
  if (downloader)
    downloader->OnDownloadServiceReady(outstanding_download_guids);
}

void OfflinePrefetchDownloadClient::OnServiceUnavailable() {
  // TODO(dtrainor, jianli): Handle service initialization failures.  This could
  // potentially just drop all pending start requests.
}

download::Client::ShouldDownload
OfflinePrefetchDownloadClient::OnDownloadStarted(
    const std::string& guid,
    const std::vector<GURL>& url_chain,
    const scoped_refptr<const net::HttpResponseHeaders>& headers) {
  return download::Client::ShouldDownload::CONTINUE;
}

void OfflinePrefetchDownloadClient::OnDownloadUpdated(
    const std::string& guid,
    uint64_t bytes_downloaded) {}

void OfflinePrefetchDownloadClient::OnDownloadFailed(
    const std::string& guid,
    download::Client::FailureReason reason) {
  PrefetchDownloader* downloader = GetPrefetchDownloader();
  if (downloader)
    downloader->OnDownloadFailed(guid);
}

void OfflinePrefetchDownloadClient::OnDownloadSucceeded(
    const std::string& guid,
    const download::CompletionInfo& completion_info) {
  PrefetchDownloader* downloader = GetPrefetchDownloader();
  if (downloader)
    downloader->OnDownloadSucceeded(guid, completion_info.path,
                                    completion_info.bytes_downloaded);
}

PrefetchDownloader* OfflinePrefetchDownloadClient::GetPrefetchDownloader()
    const {
  PrefetchService* prefetch_service =
      PrefetchServiceFactory::GetForBrowserContext(context_);
  if (!prefetch_service)
    return nullptr;
  return prefetch_service->GetPrefetchDownloader();
}

}  // namespace offline_pages
