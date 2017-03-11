// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_REQUEST_INFO_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_REQUEST_INFO_H_

#include <vector>

#include "content/common/content_export.h"
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

  bool complete() const { return complete_; }
  void set_complete(bool complete) { complete_ = complete; }

  const std::string& download_guid() const { return download_guid_; }
  void set_download_guid(const std::string& download_guid) {
    download_guid_ = download_guid;
  }

 private:
  std::string guid_;
  GURL url_;
  std::string tag_;
  bool complete_ = false;
  std::string download_guid_;
};

using BackgroundFetchRequestInfos = std::vector<BackgroundFetchRequestInfo>;

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_REQUEST_INFO_H_
