// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/prefetch_downloader_impl.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "components/download/public/download_params.h"
#include "components/download/public/download_service.h"
#include "components/offline_pages/core/offline_event_logger.h"
#include "components/offline_pages/core/prefetch/prefetch_dispatcher.h"
#include "components/offline_pages/core/prefetch/prefetch_server_urls.h"
#include "components/offline_pages/core/prefetch/prefetch_service.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace offline_pages {
namespace {

void NotifyDispatcher(PrefetchService* service, PrefetchDownloadResult result) {
  if (service) {
    PrefetchDispatcher* dispatcher = service->GetPrefetchDispatcher();
    if (dispatcher)
      dispatcher->DownloadCompleted(result);
  }
}

}  // namespace

PrefetchDownloaderImpl::PrefetchDownloaderImpl(
    download::DownloadService* download_service,
    version_info::Channel channel)
    : download_service_(download_service),
      channel_(channel),
      weak_ptr_factory_(this) {
  DCHECK(download_service);
  service_started_ = download_service->GetStatus() ==
                     download::DownloadService::ServiceStatus::READY;
}

PrefetchDownloaderImpl::PrefetchDownloaderImpl(version_info::Channel channel)
    : download_service_(nullptr), channel_(channel), weak_ptr_factory_(this) {}

PrefetchDownloaderImpl::~PrefetchDownloaderImpl() = default;

void PrefetchDownloaderImpl::SetPrefetchService(PrefetchService* service) {
  prefetch_service_ = service;
}

void PrefetchDownloaderImpl::StartDownload(
    const std::string& download_id,
    const std::string& download_location) {
  if (!service_started_) {
    pending_downloads_.push_back(
        std::make_pair(download_id, download_location));
    return;
  }

  prefetch_service_->GetLogger()->RecordActivity(
      "Downloader: Start download of '" + download_location +
      "', download_id=" + download_id);

  download::DownloadParams params;
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("prefetch_download", R"(
        semantics {
          sender: "Prefetch Downloader"
          description:
            "Chromium interacts with Offline Page Service to prefetch "
            "suggested website resources."
          trigger:
            "When there are suggested website resources to fetch."
          data:
            "The link to the contents of the suggested website resources to "
            "fetch."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can enable or disable the offline prefetch on desktop by "
            "toggling 'Use a prediction service to load pages more quickly' in "
            "settings under Privacy and security, or on Android by toggling "
            "chrome://flags#offline-prefetch."
          chrome_policy {
            NetworkPredictionOptions {
              NetworkPredictionOptions: 2
            }
          }
        })");
  params.traffic_annotation =
      net::MutableNetworkTrafficAnnotationTag(traffic_annotation);
  params.client = download::DownloadClient::OFFLINE_PAGE_PREFETCH;
  params.guid = download_id;
  params.callback = base::Bind(&PrefetchDownloaderImpl::OnStartDownload,
                               weak_ptr_factory_.GetWeakPtr());
  params.scheduling_params.network_requirements =
      download::SchedulingParams::NetworkRequirements::UNMETERED;
  params.scheduling_params.battery_requirements =
      download::SchedulingParams::BatteryRequirements::BATTERY_SENSITIVE;
  params.request_params.url = PrefetchDownloadURL(download_location, channel_);
  download_service_->StartDownload(params);
}

void PrefetchDownloaderImpl::CancelDownload(const std::string& download_id) {
  if (service_started_) {
    download_service_->CancelDownload(download_id);
    return;
  }
  for (auto iter = pending_downloads_.begin(); iter != pending_downloads_.end();
       ++iter) {
    if (iter->first == download_id) {
      pending_downloads_.erase(iter);
      return;
    }
  }
  pending_cancellations_.push_back(download_id);
}

void PrefetchDownloaderImpl::OnDownloadServiceReady(
    const std::set<std::string>& outstanding_download_ids,
    const std::map<std::string, std::pair<base::FilePath, int64_t>>&
        success_downloads) {
  prefetch_service_->GetLogger()->RecordActivity("Downloader: Service ready.");
  DCHECK_EQ(download::DownloadService::ServiceStatus::READY,
            download_service_->GetStatus());
  service_started_ = true;

  PrefetchDispatcher* dispatcher = prefetch_service_->GetPrefetchDispatcher();
  if (dispatcher)
    dispatcher->CleanupDownloads(outstanding_download_ids, success_downloads);

  for (const auto& entry : pending_downloads_)
    StartDownload(entry.first, entry.second);
  pending_downloads_.clear();

  for (const auto& entry : pending_cancellations_)
    download_service_->CancelDownload(entry);
  pending_cancellations_.clear();
}

void PrefetchDownloaderImpl::OnDownloadServiceUnavailable() {
  prefetch_service_->GetLogger()->RecordActivity(
      "Downloader: Service unavailable.");
  // TODO(jianli): Report UMA.
}

void PrefetchDownloaderImpl::OnDownloadServiceShutdown() {
  prefetch_service_->GetLogger()->RecordActivity(
      "Downloader: Service shutdown.");
  service_started_ = false;
}

void PrefetchDownloaderImpl::OnDownloadSucceeded(
    const std::string& download_id,
    const base::FilePath& file_path,
    int64_t file_size) {
  prefetch_service_->GetLogger()->RecordActivity(
      "Downloader: Download succeeded, download_id=" + download_id);
  NotifyDispatcher(prefetch_service_,
                   PrefetchDownloadResult(download_id, file_path, file_size));
}

void PrefetchDownloaderImpl::OnDownloadFailed(const std::string& download_id) {
  PrefetchDownloadResult result;
  result.download_id = download_id;
  prefetch_service_->GetLogger()->RecordActivity(
      "Downloader: Download failed, download_id=" + download_id);
  NotifyDispatcher(prefetch_service_, result);
}

void PrefetchDownloaderImpl::OnStartDownload(
    const std::string& download_id,
    download::DownloadParams::StartResult result) {
  prefetch_service_->GetLogger()->RecordActivity(
      "Downloader: Download started, download_id=" + download_id +
      ", result=" + std::to_string(static_cast<int>(result)));
  if (result != download::DownloadParams::StartResult::ACCEPTED)
    OnDownloadFailed(download_id);
}

}  // namespace offline_pages
