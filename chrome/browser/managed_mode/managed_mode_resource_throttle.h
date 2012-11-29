// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MANAGED_MODE_MANAGED_MODE_RESOURCE_THROTTLE_H_
#define CHROME_BROWSER_MANAGED_MODE_MANAGED_MODE_RESOURCE_THROTTLE_H_

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/resource_throttle.h"

class ManagedModeURLFilter;

namespace net {
class URLRequest;
}

// This class temporarily blocks network requests that aren't whitelisted,
// and allows resuming them later.
class ManagedModeResourceThrottle : public content::ResourceThrottle {
 public:
  ManagedModeResourceThrottle(const net::URLRequest* request,
                              int render_process_host_id,
                              int render_view_id,
                              bool is_main_frame);
  virtual ~ManagedModeResourceThrottle();

  // content::ResourceThrottle implementation:
  virtual void WillStartRequest(bool* defer) OVERRIDE;

 private:
  void OnInterstitialResult(bool continue_request);

  base::WeakPtrFactory<ManagedModeResourceThrottle> weak_ptr_factory_;
  const net::URLRequest* request_;
  int render_process_host_id_;
  int render_view_id_;
  bool is_main_frame_;
  const ManagedModeURLFilter* url_filter_;

  DISALLOW_COPY_AND_ASSIGN(ManagedModeResourceThrottle);
};

#endif  // CHROME_BROWSER_MANAGED_MODE_MANAGED_MODE_RESOURCE_THROTTLE_H_
