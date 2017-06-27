// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_DOWNLOADER_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_DOWNLOADER_H_

#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/download/public/download_params.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/version_info/channel.h"

namespace download {
class DownloadService;
}  // namespace download

namespace offline_pages {

class PrefetchServiceTestTaco;

// Asynchronously downloads the archive.
class PrefetchDownloader {
 public:
  PrefetchDownloader(download::DownloadService* download_service,
                     version_info::Channel channel);
  ~PrefetchDownloader();

  void SetCompletedCallback(const PrefetchDownloadCompletedCallback& callback);

  // Starts to download an archive from |download_location|.
  void StartDownload(const std::string& download_id,
                     const std::string& download_location);

  // Cancels a previous scheduled download.
  void CancelDownload(const std::string& download_id);

  // Responding to download client event.

  // Called when the download service is initialized and can accept the
  // downloads.
  void OnDownloadServiceReady();

  // Called when the download service is tearing down.
  void OnDownloadServiceShutdown();

  // Called when a download is completed successfully. Note that the download
  // can be scheduled in preious sessions.
  void OnDownloadSucceeded(const std::string& download_id,
                           const base::FilePath& file_path,
                           uint64_t file_size);

  // Called when a download fails.
  void OnDownloadFailed(const std::string& download_id);

 private:
  friend class PrefetchServiceTestTaco;

  // For test only.
  explicit PrefetchDownloader(version_info::Channel channel);

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

  base::WeakPtrFactory<PrefetchDownloader> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PrefetchDownloader);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_DOWNLOADER_H_
