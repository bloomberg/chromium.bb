// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/test_prefetch_downloader.h"

namespace offline_pages {

TestPrefetchDownloader::TestPrefetchDownloader() = default;

TestPrefetchDownloader::~TestPrefetchDownloader() = default;

void TestPrefetchDownloader::SetPrefetchService(PrefetchService* service) {}

void TestPrefetchDownloader::StartDownload(
    const std::string& download_id,
    const std::string& download_location) {
  requested_downloads_[download_id] = download_location;
}

void TestPrefetchDownloader::CancelDownload(const std::string& download_id) {}

void TestPrefetchDownloader::OnDownloadServiceReady(
    const std::vector<std::string>& outstanding_download_ids) {}

void TestPrefetchDownloader::OnDownloadServiceUnavailable() {}

void TestPrefetchDownloader::OnDownloadServiceShutdown() {}

void TestPrefetchDownloader::OnDownloadSucceeded(
    const std::string& download_id,
    const base::FilePath& file_path,
    uint64_t file_size) {}

void TestPrefetchDownloader::OnDownloadFailed(const std::string& download_id) {}

void TestPrefetchDownloader::Reset() {
  requested_downloads_.clear();
}

}  // namespace offline_pages
