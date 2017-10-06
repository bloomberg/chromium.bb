// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/prefetch_downloader_impl.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
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
    : clock_(new base::DefaultClock()),
      download_service_(download_service),
      channel_(channel),
      weak_ptr_factory_(this) {
  DCHECK(download_service);
}

PrefetchDownloaderImpl::PrefetchDownloaderImpl(version_info::Channel channel)
    : download_service_(nullptr), channel_(channel), weak_ptr_factory_(this) {}

PrefetchDownloaderImpl::~PrefetchDownloaderImpl() = default;

void PrefetchDownloaderImpl::SetPrefetchService(PrefetchService* service) {
  prefetch_service_ = service;
}

bool PrefetchDownloaderImpl::IsDownloadServiceUnavailable() const {
  return download_service_status_ == DownloadServiceStatus::UNAVAILABLE;
}

void PrefetchDownloaderImpl::CleanupDownloadsWhenReady() {
  // Trigger the download cleanup if the download service has already started.
  if (download_service_status_ == DownloadServiceStatus::STARTED) {
    CleanupDownloads(outstanding_download_ids_, success_downloads_);
    return;
  }

  can_do_download_cleanup_ = true;
}

void PrefetchDownloaderImpl::StartDownload(
    const std::string& download_id,
    const std::string& download_location) {
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
  params.scheduling_params.cancel_time =
      clock_->Now() + kPrefetchDownloadLifetime;
  params.request_params.url = PrefetchDownloadURL(download_location, channel_);

  std::string experiment_header = PrefetchExperimentHeader();
  if (!experiment_header.empty()) {
    params.request_params.request_headers.AddHeaderFromString(
        experiment_header);
  }

  // The download service can queue the download even if it is not fully up yet.
  download_service_->StartDownload(params);
}

void PrefetchDownloaderImpl::OnDownloadServiceReady(
    const std::set<std::string>& outstanding_download_ids,
    const std::map<std::string, std::pair<base::FilePath, int64_t>>&
        success_downloads) {
  DCHECK_EQ(DownloadServiceStatus::INITIALIZING, download_service_status_);
  download_service_status_ = DownloadServiceStatus::STARTED;

  prefetch_service_->GetLogger()->RecordActivity("Downloader: Service ready.");

  // If the prefetch service has requested the download cleanup, do it now.
  if (can_do_download_cleanup_) {
    CleanupDownloads(outstanding_download_ids, success_downloads);
    can_do_download_cleanup_ = false;
    return;
  }

  // Otherwise, cache the download cleanup data until told by the prefetch
  // service.
  outstanding_download_ids_ = outstanding_download_ids;
  success_downloads_ = success_downloads;
}

void PrefetchDownloaderImpl::OnDownloadServiceUnavailable() {
  DCHECK_EQ(DownloadServiceStatus::INITIALIZING, download_service_status_);
  download_service_status_ = DownloadServiceStatus::UNAVAILABLE;

  prefetch_service_->GetLogger()->RecordActivity(
      "Downloader: Service unavailable.");

  // The download service is unavailable to use for the whole lifetime of
  // Chrome. PrefetchService can't schedule any downloads. Next time when Chrome
  // restarts, the download service might be back to operational.
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

void PrefetchDownloaderImpl::SetClockForTest(
    std::unique_ptr<base::Clock> clock) {
  clock_ = std::move(clock);
}

void PrefetchDownloaderImpl::OnStartDownload(
    const std::string& download_id,
    download::DownloadParams::StartResult result) {
  prefetch_service_->GetLogger()->RecordActivity(
      "Downloader: Download started, download_id=" + download_id +
      ", result=" + std::to_string(static_cast<int>(result)));
  // Treat the non-accepted request to start a download as an ordinary failure
  // to simplify the control flow since this situation should rarely happen. The
  // Download.Service.Request.StartResult.OfflinePage histogram tracks these
  // cases and would signal the need to revisit this decision.
  if (result != download::DownloadParams::StartResult::ACCEPTED)
    OnDownloadFailed(download_id);
}

void PrefetchDownloaderImpl::CleanupDownloads(
    const std::set<std::string>& outstanding_download_ids,
    const std::map<std::string, std::pair<base::FilePath, int64_t>>&
        success_downloads) {
  PrefetchDispatcher* dispatcher = prefetch_service_->GetPrefetchDispatcher();
  if (dispatcher)
    dispatcher->CleanupDownloads(outstanding_download_ids, success_downloads);
}

}  // namespace offline_pages
