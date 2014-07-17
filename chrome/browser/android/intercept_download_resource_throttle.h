// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_INTERCEPT_DOWNLOAD_RESOURCE_THROTTLE_H_
#define CHROME_BROWSER_ANDROID_INTERCEPT_DOWNLOAD_RESOURCE_THROTTLE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "content/public/browser/resource_throttle.h"

namespace net {
class URLRequest;
}

namespace chrome {

// InterceptDownloadResourceThrottle checks if a download request should be
// handled by Chrome or passsed to the Android Download Manager.
class InterceptDownloadResourceThrottle : public content::ResourceThrottle {
 public:
  InterceptDownloadResourceThrottle(net::URLRequest* request,
                                    int render_process_id,
                                    int render_view_id,
                                    int request_id);

  // content::ResourceThrottle implementation:
  virtual void WillProcessResponse(bool* defer) OVERRIDE;
  virtual const char* GetNameForLogging() const OVERRIDE;

 private:
  virtual ~InterceptDownloadResourceThrottle();

  void ProcessDownloadRequest();
  // Set to true when if we want chrome to handle the download.
  const net::URLRequest* request_;
  int render_process_id_;
  int render_view_id_;
  int request_id_;

  DISALLOW_COPY_AND_ASSIGN(InterceptDownloadResourceThrottle);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_INTERCEPT_DOWNLOAD_RESOURCE_THROTTLE_H_
