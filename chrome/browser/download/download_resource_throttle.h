// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_RESOURCE_THROTTLE_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_RESOURCE_THROTTLE_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/download/download_request_limiter.h"
#include "content/public/browser/resource_throttle.h"

class GURL;

// DownloadResourceThrottle is used to determine if a download should be
// allowed. When a DownloadResourceThrottle is created it pauses the download
// and asks the DownloadRequestLimiter if the download should be allowed. The
// DownloadRequestLimiter notifies us asynchronously as to whether the download
// is allowed or not. If the download is allowed the request is resumed.  If
// the download is not allowed the request is canceled.

class DownloadResourceThrottle
    : public content::ResourceThrottle,
      public base::SupportsWeakPtr<DownloadResourceThrottle> {
 public:
  DownloadResourceThrottle(DownloadRequestLimiter* limiter,
                           int render_process_id,
                           int render_view_id,
                           const GURL& url,
                           const std::string& request_method);

  // content::ResourceThrottle implementation:
  virtual void WillStartRequest(bool* defer) OVERRIDE;
  virtual void WillRedirectRequest(const GURL& new_url, bool* defer) OVERRIDE;
  virtual void WillProcessResponse(bool* defer) OVERRIDE;
  virtual const char* GetNameForLogging() const OVERRIDE;

 private:
  virtual ~DownloadResourceThrottle();

  void WillDownload(bool* defer);
  void ContinueDownload(bool allow);

  // Set to true when we are querying the DownloadRequestLimiter.
  bool querying_limiter_;

  // Set to true when we know that the request is allowed to start.
  bool request_allowed_;

  // Set to true when we have deferred the request.
  bool request_deferred_;

  DISALLOW_COPY_AND_ASSIGN(DownloadResourceThrottle);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_RESOURCE_THROTTLE_H_
