// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_IN_PROGRESS_DOWNLOAD_ENTRY_H_
#define COMPONENTS_DOWNLOAD_IN_PROGRESS_DOWNLOAD_ENTRY_H_

#include <string>

namespace download {

// Contains various in-progress information related to a download.
struct DownloadEntry {
 public:
  DownloadEntry();
  DownloadEntry(const DownloadEntry& other);
  DownloadEntry(const std::string& guid, const std::string& request_origin);
  ~DownloadEntry();

  // A unique GUID that represents this download.
  std::string guid;

  // Represents the origin information for this download. Used by offline pages.
  std::string request_origin;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_IN_PROGRESS_DOWNLOAD_ENTRY_H_
