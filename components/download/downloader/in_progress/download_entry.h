// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_IN_PROGRESS_DOWNLOAD_ENTRY_H_
#define COMPONENTS_DOWNLOAD_IN_PROGRESS_DOWNLOAD_ENTRY_H_

#include <string>

#include "components/download/downloader/in_progress/download_source.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace download {

// Contains various in-progress information related to a download.
struct DownloadEntry {
 public:
  DownloadEntry();
  DownloadEntry(const DownloadEntry& other);
  DownloadEntry(const std::string& guid,
                DownloadSource download_source,
                int64_t ukm_id);
  DownloadEntry(const std::string& guid,
                const std::string& request_origin,
                DownloadSource download_source,
                int64_t ukm_id);
  ~DownloadEntry();

  bool operator==(const DownloadEntry& other) const;

  bool operator!=(const DownloadEntry& other) const;

  // A unique GUID that represents this download.
  std::string guid;

  // Represents the origin information for this download. Used by offline pages.
  std::string request_origin;

  // The source that triggered the download.
  DownloadSource download_source = DownloadSource::UNKNOWN;

  // Unique ID that tracks the download UKM entry, where 0 means the
  // download_id is not yet initialized.
  uint64_t ukm_download_id = 0;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_IN_PROGRESS_DOWNLOAD_ENTRY_H_
