// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_DOWNLOADER_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_DOWNLOADER_H_

#include <string>

#include "base/callback.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"

namespace offline_pages {
class PrefetchService;

// Asynchronously downloads the archive.
class PrefetchDownloader {
 public:
  virtual ~PrefetchDownloader() = default;

  virtual void SetPrefetchService(PrefetchService* service) = 0;

  // Starts to download an archive from |download_location|.
  virtual void StartDownload(const std::string& download_id,
                             const std::string& download_location) = 0;

  // Cancels a previous scheduled download.
  virtual void CancelDownload(const std::string& download_id) = 0;

  // Called when the download service is initialized and can accept the
  // downloads.
  virtual void OnDownloadServiceReady(
      const std::vector<std::string>& outstanding_download_ids) = 0;

  // Called when the download service fails to initialize and should not be
  // used.
  virtual void OnDownloadServiceUnavailable() = 0;

  // Called when the download service is tearing down.
  virtual void OnDownloadServiceShutdown() = 0;

  // Called when a download is completed successfully. Note that the download
  // can be scheduled in previous sessions.
  virtual void OnDownloadSucceeded(const std::string& download_id,
                                   const base::FilePath& file_path,
                                   uint64_t file_size) = 0;

  // Called when a download fails.
  virtual void OnDownloadFailed(const std::string& download_id) = 0;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_DOWNLOADER_H_
