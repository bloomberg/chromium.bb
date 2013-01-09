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

class ManagedModeResourceThrottle : public content::ResourceThrottle {
 public:
  ManagedModeResourceThrottle(const net::URLRequest* request,
                              int render_process_host_id,
                              int render_view_id,
                              bool is_main_frame);
  virtual ~ManagedModeResourceThrottle();

  // Adds/removes a temporary exception to filtering for a
  // render_process_host_id and render_view_id pair (which identify a tab)
  // to the preview map. Adding saves the last approved hostname in the map,
  // which is then used to allow the user to browse on that hostname without
  // getting an interstitial. See managed_mode_resource_throttle.cc for more
  // details on the preview map.
  static void AddTemporaryException(int render_process_host_id,
                                    int render_view_id,
                                    const GURL& url);
  static void RemoveTemporaryException(int render_process_host_id,
                                       int render_view_id);

  // content::ResourceThrottle implementation:
  virtual void WillStartRequest(bool* defer) OVERRIDE;

  virtual void WillRedirectRequest(const GURL& new_url, bool* defer) OVERRIDE;

 private:
  void ShowInterstitialIfNeeded(bool is_redirect,
                                const GURL& url,
                                bool* defer);
  void OnInterstitialResult(bool continue_request);

  base::WeakPtrFactory<ManagedModeResourceThrottle> weak_ptr_factory_;
  const net::URLRequest* request_;
  int render_process_host_id_;
  int render_view_id_;
  bool is_main_frame_;
  bool temporarily_allowed_;
  const ManagedModeURLFilter* url_filter_;

  DISALLOW_COPY_AND_ASSIGN(ManagedModeResourceThrottle);
};

#endif  // CHROME_BROWSER_MANAGED_MODE_MANAGED_MODE_RESOURCE_THROTTLE_H_
