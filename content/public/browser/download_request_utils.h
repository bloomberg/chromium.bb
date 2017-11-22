// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DOWNLOAD_REQUEST_UTILS_H_
#define CONTENT_PUBLIC_BROWSER_DOWNLOAD_REQUEST_UTILS_H_

#include <string>

#include "content/common/content_export.h"

namespace net {
class URLRequest;
}  // namespace net

namespace content {

// Utility methods for download requests.
class CONTENT_EXPORT DownloadRequestUtils {
 public:
  // Returns the identifier for origin of the download.
  static std::string GetRequestOriginFromRequest(
      const net::URLRequest* request);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DOWNLOAD_REQUEST_UTILS_H_
