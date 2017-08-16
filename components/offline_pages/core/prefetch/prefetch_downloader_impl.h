// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_DOWNLOADER_IMPL_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_DOWNLOADER_IMPL_H_

#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/download/public/download_params.h"
#include "components/offline_pages/core/prefetch/prefetch_downloader.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/version_info/channel.h"

namespace download {
class DownloadService;
}  // namespace download

namespace offline_pages {

class PrefetchServiceTestTaco;

// Asynchronously downloads the archive.
class PrefetchDownloaderImpl : public PrefetchDownloader {
 public:
  PrefetchDownloaderImpl(download::DownloadService* download_service,
                         version_info::Channel channel);
  ~PrefetchDownloaderImpl() override;

  // PrefetchDownloader implementation:
  void SetCompletedCallback(
      const PrefetchDownloadCompletedCallback& callback) override;
  void StartDownload(const std::string& download_id,
                     const std::string& download_location) override;
  void CancelDownload(const std::string& download_id) override;
  void OnDownloadServiceReady(
      const std::vector<std::string>& outstanding_download_ids) override;
  void OnDownloadServiceUnavailable() override;
  void OnDownloadServiceShutdown() override;
  void OnDownloadSucceeded(const std::string& download_id,
                           const base::FilePath& file_path,
                           uint64_t file_size) override;
  void OnDownloadFailed(const std::string& download_id) override;

 private:
  friend class PrefetchServiceTestTaco;

  // For test only.
  explicit PrefetchDownloaderImpl(version_info::Channel channel);

  // Callback for StartDownload.
  void OnStartDownload(const std::string& download_id,
                       download::DownloadParams::StartResult result);

  // Unowned. It is valid until |this| instance is disposed.
  download::DownloadService* download_service_;

  version_info::Channel channel_;
  PrefetchDownloadCompletedCallback callback_;

  // Flag to indicate if the download service is ready to take downloads.
  bool service_started_ = false;

  // TODO(jianli): Investigate making PrefetchService waits for DownloadService
  // ready in order to avoid queueing.
  // List of downloads pending to start after the download service starts. Each
  // item is a pair of download id and download location.
  std::vector<std::pair<std::string, std::string>> pending_downloads_;
  // List of ids of downloads waiting to be cancelled after the download service
  // starts.
  std::vector<std::string> pending_cancellations_;

  base::WeakPtrFactory<PrefetchDownloaderImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PrefetchDownloaderImpl);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_DOWNLOADER_IMPL_H_
