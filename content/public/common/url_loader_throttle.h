// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_URL_LOADER_THROTTLE_H_
#define CONTENT_PUBLIC_COMMON_URL_LOADER_THROTTLE_H_

#include "content/common/content_export.h"
#include "content/public/common/resource_type.h"

class GURL;

namespace net {
struct RedirectInfo;
}

namespace content {

// A URLLoaderThrottle gets notified at various points during the process of
// loading a resource. At each stage, it has the opportunity to defer the
// resource load.
class CONTENT_EXPORT URLLoaderThrottle {
 public:
  // An interface for the throttle implementation to resume (when deferred) or
  // cancel the resource load.
  class CONTENT_EXPORT Delegate {
   public:
    // Cancels the resource load with the specified error code.
    virtual void CancelWithError(int error_code) = 0;

    // Resumes the deferred resource load. It is a no-op if the resource load is
    // not deferred or has already been canceled.
    virtual void Resume() = 0;

   protected:
    virtual ~Delegate() {}
  };

  virtual ~URLLoaderThrottle() {}

  // Called before the resource request is started.
  virtual void WillStartRequest(const GURL& url,
                                int load_flags,
                                ResourceType resource_type,
                                bool* defer) {}

  // Called when the request was redirected.  |redirect_info| contains the
  // redirect responses's HTTP status code and some information about the new
  // request that will be sent if the redirect is followed, including the new
  // URL and new method.
  virtual void WillRedirectRequest(const net::RedirectInfo& redirect_info,
                                   bool* defer) {}

  // Called when the response headers and meta data are available.
  virtual void WillProcessResponse(bool* defer) {}

  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

 protected:
  URLLoaderThrottle() = default;

  Delegate* delegate_ = nullptr;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_URL_LOADER_THROTTLE_H_
