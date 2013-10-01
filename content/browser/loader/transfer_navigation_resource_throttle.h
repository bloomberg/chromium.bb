// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_TRANSFER_NAVIGATION_RESOURCE_THROTTLE_H_
#define CONTENT_BROWSER_LOADER_TRANSFER_NAVIGATION_RESOURCE_THROTTLE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "content/public/browser/resource_throttle.h"

namespace net {
class URLRequest;
}

namespace content {

// This ResourceThrottle checks whether a navigation redirect will cause a
// renderer process swap. When that happens, we remember the request so
// that we can transfer it to be handled by the new renderer. This fixes
// http://crbug.com/79520
class TransferNavigationResourceThrottle : public ResourceThrottle {
 public:
  explicit TransferNavigationResourceThrottle(net::URLRequest* request);
  virtual ~TransferNavigationResourceThrottle();

  // ResourceThrottle implementation:
  virtual void WillRedirectRequest(const GURL& new_url, bool* defer) OVERRIDE;

 private:
  net::URLRequest* request_;

  DISALLOW_COPY_AND_ASSIGN(TransferNavigationResourceThrottle);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_TRANSFER_NAVIGATION_RESOURCE_THROTTLE_H_
