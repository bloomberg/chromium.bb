// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_DATABASE_IN_PROGRESS_IN_PROGRESS_INFO_H_
#define COMPONENTS_DOWNLOAD_DATABASE_IN_PROGRESS_IN_PROGRESS_INFO_H_

#include <string>
#include <vector>

#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_url_parameters.h"
#include "url/gurl.h"

namespace download {

// Contains information to reconstruct an interrupted download item for
// resumption.
struct InProgressInfo {
 public:
  InProgressInfo();
  InProgressInfo(const InProgressInfo& other);
  ~InProgressInfo();

  bool operator==(const InProgressInfo& other) const;

  //  request info  ------------------------------------------------------------

  // The url chain.
  std::vector<GURL> url_chain;

  // Referrer url.
  GURL referrer_url;

  // Site url.
  GURL site_url;

  // Tab url.
  GURL tab_url;

  // Tab referrer url.
  GURL tab_referrer_url;

  // If the entity body of unsuccessful HTTP response, like HTTP 404, will be
  // downloaded.
  bool fetch_error_body = false;

  // Request header key/value pairs that will be added to the download HTTP
  // request.
  DownloadUrlParameters::RequestHeadersType request_headers;

  //  response info  -----------------------------------------------------------

  // Contents of most recently seen ETag header.
  std::string etag;

  // Contents of most recently seen Last-Modified header.
  std::string last_modified;

  // The total number of bytes in the download.
  int64_t total_bytes = 0;

  // Mime type.
  std::string mime_type;

  // Original mime type before all redirections.
  std::string original_mime_type;

  //  destination info  --------------------------------------------------------

  // The current path to the download (potentially different from final if
  // download is in progress or interrupted).
  base::FilePath current_path;

  // The target path where the download will go when it's complete.
  base::FilePath target_path;

  // The number of bytes received (so far).
  int64_t received_bytes = 0;

  // The time when the download started.
  base::Time start_time;

  // The time when the download completed.
  base::Time end_time;

  // Data slices that have been downloaded so far. The slices must be ordered
  // by their offset.
  std::vector<DownloadItem::ReceivedSlice> received_slices;

  // Hash of the downloaded content.
  std::string hash;

  //  state info  --------------------------------------------------------------

  // Whether this download is transient. Transient items are cleaned up after
  // completion and not shown in the UI.
  bool transient = false;

  // The current state of the download.
  DownloadItem::DownloadState state = DownloadItem::DownloadState::IN_PROGRESS;

  // Whether and how the download is dangerous.
  DownloadDangerType danger_type =
      DownloadDangerType::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS;

  // The reason the download was interrupted, if state == kStateInterrupted.
  DownloadInterruptReason interrupt_reason = DOWNLOAD_INTERRUPT_REASON_NONE;

  // Whether this download is paused.
  bool paused = false;

  // Count for how many (extra) bytes were used (including resumption).
  int64_t bytes_wasted = 0;

  // The number of times the download has been auto-resumed since last user
  // triggered resumption.
  int32_t auto_resume_count = 0;

  // Whether the download is initiated on a metered network
  bool metered = false;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_DATABASE_IN_PROGRESS_IN_PROGRESS_INFO_H_
