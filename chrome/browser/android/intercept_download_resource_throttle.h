// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_INTERCEPT_DOWNLOAD_RESOURCE_THROTTLE_H_
#define CHROME_BROWSER_ANDROID_INTERCEPT_DOWNLOAD_RESOURCE_THROTTLE_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/android/download/download_controller_base.h"
#include "content/public/browser/resource_throttle.h"
#include "net/cookies/cookie_store.h"

namespace net {
class URLRequest;
}

namespace chrome {

// InterceptDownloadResourceThrottle checks if a download request should be
// handled by Chrome or passsed to the Android Download Manager.
class InterceptDownloadResourceThrottle : public content::ResourceThrottle {
 public:
  static bool IsDownloadInterceptionEnabled();

  InterceptDownloadResourceThrottle(net::URLRequest* request,
                                    int render_process_id,
                                    int render_view_id,
                                    bool must_download);

  // content::ResourceThrottle implementation:
  void WillProcessResponse(bool* defer) override;
  const char* GetNameForLogging() const override;

 private:
  ~InterceptDownloadResourceThrottle() override;

  void ProcessDownloadRequest(bool* defer);
  void CheckCookiePolicy(const net::CookieList& cookie_list);
  void StartDownload(const DownloadInfo& info);

  // Set to true when if we want chrome to handle the download.
  const net::URLRequest* request_;
  int render_process_id_;
  int render_view_id_;
  bool must_download_;

  base::WeakPtrFactory<InterceptDownloadResourceThrottle> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(InterceptDownloadResourceThrottle);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_INTERCEPT_DOWNLOAD_RESOURCE_THROTTLE_H_
