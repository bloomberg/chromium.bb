// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_DRIVER_ENTRY_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_DRIVER_ENTRY_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "url/gurl.h"

namespace net {
class HttpResponseHeaders;
}  // namespace net

namespace content {
class DownloadItem;
}  // namespace content

namespace download {

// A snapshot of the states of a download. It's preferred to use the data on the
// fly and query new ones from download driver, instead of caching the states.
struct DriverEntry {
  // States of the download. Mostly maps to
  // content::DownloadItem::DownloadState.
  enum class State {
    IN_PROGRESS = 0,
    COMPLETE = 1,
    CANCELLED = 2,
    INTERRUPTED = 3,
    UNKNOWN = 4, /* Not created from a download item object. */
  };

  DriverEntry();
  DriverEntry(const DriverEntry& other);
  ~DriverEntry();

  // The unique identifier of the download.
  std::string guid;

  // The current state of the download.
  State state;

  // If the download is paused.
  bool paused;

  // The number of bytes downloaded.
  uint64_t bytes_downloaded;

  // The expected total size of the download, set to 0 if the Content-Length
  // http header is not presented.
  uint64_t expected_total_size;

  // The physical file path for the download. It can be different from the
  // target file path requested while the file is downloading, as it may
  // download to a temporary path.
  base::FilePath temporary_physical_file_path;

  // Time the download was marked as complete, base::Time() if the download is
  // not yet complete.
  base::Time completion_time;

  // The response headers for the most recent download request.
  scoped_refptr<const net::HttpResponseHeaders> response_headers;

  // The url chain of the download. Download may encounter redirects, and
  // fetches the content from the last url in the chain.
  std::vector<GURL> url_chain;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_DRIVER_ENTRY_H_
