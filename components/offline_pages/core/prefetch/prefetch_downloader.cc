// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/prefetch_downloader.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "components/download/public/download_service.h"
#include "components/offline_pages/core/prefetch/prefetch_server_urls.h"
#include "url/gurl.h"

namespace offline_pages {

PrefetchDownloader::PrefetchDownloader(
    download::DownloadService* download_service,
    version_info::Channel channel)
    : download_service_(download_service),
      channel_(channel),
      weak_ptr_factory_(this) {
  DCHECK(download_service);
  service_started_ = download_service->GetStatus() ==
                     download::DownloadService::ServiceStatus::READY;
}

PrefetchDownloader::PrefetchDownloader(version_info::Channel channel)
    : channel_(channel), weak_ptr_factory_(this) {}

PrefetchDownloader::~PrefetchDownloader() = default;

void PrefetchDownloader::SetCompletedCallback(
    const PrefetchDownloadCompletedCallback& callback) {
  callback_ = callback;
}

void PrefetchDownloader::StartDownload(const std::string& download_id,
                                       const std::string& download_location) {
  if (!service_started_) {
    pending_downloads_.push_back(
        std::make_pair(download_id, download_location));
    return;
  }

  // TODO(jianli): Specify scheduling parameters, i.e. battery, network and etc.
  // http://crbug.com/736156
  download::DownloadParams params;
  params.client = download::DownloadClient::OFFLINE_PAGE_PREFETCH;
  // TODO(jianli): Remove the uppercase after the download service fixes
  // this issue.
  params.guid = base::ToUpperASCII(download_id);
  params.callback = base::Bind(&PrefetchDownloader::OnStartDownload,
                               weak_ptr_factory_.GetWeakPtr());
  params.request_params.url = PrefetchDownloadURL(download_location, channel_);
  download_service_->StartDownload(params);
}

void PrefetchDownloader::CancelDownload(const std::string& download_id) {
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

void PrefetchDownloader::OnDownloadServiceReady() {
  DCHECK_EQ(download::DownloadService::ServiceStatus::READY,
            download_service_->GetStatus());
  service_started_ = true;

  for (const auto& entry : pending_downloads_)
    StartDownload(entry.first, entry.second);
  pending_downloads_.clear();

  for (const auto& entry : pending_cancellations_)
    download_service_->CancelDownload(entry);
  pending_cancellations_.clear();
}

void PrefetchDownloader::OnDownloadServiceShutdown() {
  service_started_ = false;
}

void PrefetchDownloader::OnDownloadSucceeded(const std::string& download_id,
                                             const base::FilePath& file_path,
                                             uint64_t file_size) {
  if (callback_)
    callback_.Run(PrefetchDownloadResult(download_id, file_path, file_size));
}

void PrefetchDownloader::OnDownloadFailed(const std::string& download_id) {
  if (callback_) {
    PrefetchDownloadResult result;
    result.download_id = download_id;
    callback_.Run(result);
  }
}

void PrefetchDownloader::OnStartDownload(
    const std::string& download_id,
    download::DownloadParams::StartResult result) {
  if (result != download::DownloadParams::StartResult::ACCEPTED)
    OnDownloadFailed(download_id);
}

}  // namespace offline_pages
