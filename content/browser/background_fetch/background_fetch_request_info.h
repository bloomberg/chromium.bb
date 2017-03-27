// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_REQUEST_INFO_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_REQUEST_INFO_H_

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "content/common/content_export.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/download_item.h"
#include "url/gurl.h"

namespace content {

// Simple class to encapsulate the components of a fetch request.
class CONTENT_EXPORT BackgroundFetchRequestInfo {
 public:
  BackgroundFetchRequestInfo();
  BackgroundFetchRequestInfo(const GURL& url, const std::string& tag);
  // TODO(harkness): Remove copy constructor once the final (non-map-based)
  // state management is in place.
  BackgroundFetchRequestInfo(const BackgroundFetchRequestInfo& request);
  ~BackgroundFetchRequestInfo();

  const std::string& guid() const { return guid_; }
  const GURL& url() const { return url_; }
  const std::string& tag() const { return tag_; }

  DownloadItem::DownloadState state() const { return state_; }
  void set_state(DownloadItem::DownloadState state) { state_ = state; }

  const std::string& download_guid() const { return download_guid_; }
  void set_download_guid(const std::string& download_guid) {
    download_guid_ = download_guid;
  }

  DownloadInterruptReason interrupt_reason() const { return interrupt_reason_; }
  void set_interrupt_reason(DownloadInterruptReason reason) {
    interrupt_reason_ = reason;
  }

  const base::FilePath& file_path() const { return file_path_; }
  void set_file_path(const base::FilePath& file_path) {
    file_path_ = file_path;
  }

  int64_t received_bytes() const { return received_bytes_; }
  void set_received_bytes(int64_t received_bytes) {
    received_bytes_ = received_bytes;
  }

  bool IsComplete() const;

 private:
  std::string guid_;
  GURL url_;
  std::string tag_;
  std::string download_guid_;

  // The following members do not need to be persisted, they can be reset after
  // a chrome restart.
  DownloadItem::DownloadState state_ =
      DownloadItem::DownloadState::MAX_DOWNLOAD_STATE;
  DownloadInterruptReason interrupt_reason_ =
      DownloadInterruptReason::DOWNLOAD_INTERRUPT_REASON_NONE;
  base::FilePath file_path_;
  int64_t received_bytes_ = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_REQUEST_INFO_H_
