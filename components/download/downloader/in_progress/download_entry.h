// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_IN_PROGRESS_DOWNLOAD_ENTRY_H_
#define COMPONENTS_DOWNLOAD_IN_PROGRESS_DOWNLOAD_ENTRY_H_

#include <string>

#include "components/download/downloader/in_progress/download_source.h"

namespace download {

// Contains various in-progress information related to a download.
struct DownloadEntry {
 public:
  DownloadEntry();
  DownloadEntry(const DownloadEntry& other);
  DownloadEntry(const std::string& guid,
                const std::string& request_origin,
                DownloadSource download_source);
  ~DownloadEntry();

  bool operator==(const DownloadEntry& other) const;

  // A unique GUID that represents this download.
  std::string guid;

  // Represents the origin information for this download. Used by offline pages.
  std::string request_origin;

  // The source that triggered the download.
  DownloadSource download_source = DownloadSource::UNKNOWN;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_IN_PROGRESS_DOWNLOAD_ENTRY_H_
