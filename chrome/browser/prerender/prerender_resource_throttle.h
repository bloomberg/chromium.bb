// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_RESOURCE_THROTTLE_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_RESOURCE_THROTTLE_H_

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/resource_throttle.h"
#include "content/public/common/resource_type.h"

class GURL;

namespace net {
class URLRequest;
}

namespace prerender {
class PrerenderContents;
class PrerenderThrottleInfo;

// This class implements policy on resource requests in prerenders.  It cancels
// prerenders on certain requests.  It also defers certain requests until after
// the prerender is swapped in.
//
// TODO(davidben): Experiment with deferring network requests that
// would otherwise cancel the prerender.
class PrerenderResourceThrottle
    : public content::ResourceThrottle,
      public base::SupportsWeakPtr<PrerenderResourceThrottle> {
 public:
  explicit PrerenderResourceThrottle(net::URLRequest* request);

  ~PrerenderResourceThrottle() override;

  // content::ResourceThrottle implementation:
  void WillStartRequest(bool* defer) override;
  void WillRedirectRequest(const net::RedirectInfo& redirect_info,
                           bool* defer) override;
  void WillProcessResponse(bool* defer) override;
  const char* GetNameForLogging() const override;

  // Called by the PrerenderContents when a prerender becomes visible.
  // May only be called if currently throttling the resource.
  void Resume();

  static void OverridePrerenderContentsForTesting(PrerenderContents* contents);

 private:
  // Helper method to cancel the request. May only be called if currently
  // throttling the resource.
  void Cancel();

  static void WillStartRequestOnUI(
      const base::WeakPtr<PrerenderResourceThrottle>& throttle,
      const std::string& method,
      content::ResourceType resource_type,
      int render_process_id,
      int render_frame_id,
      const GURL& url,
      scoped_refptr<PrerenderThrottleInfo> prerender_throttle_info);

  static void WillRedirectRequestOnUI(
      const base::WeakPtr<PrerenderResourceThrottle>& throttle,
      const std::string& follow_only_when_prerender_shown_header,
      content::ResourceType resource_type,
      bool async,
      bool is_no_store,
      int render_process_id,
      int render_frame_id,
      const GURL& new_url);

  static void WillProcessResponseOnUI(
      bool is_main_resource,
      bool is_no_store,
      int redirect_count,
      int render_process_id,
      int render_frame_id,
      scoped_refptr<PrerenderThrottleInfo> prerender_throttle_info);

  // Helper to return the PrerenderContents given a render frame id. May return
  // NULL if it's gone.
  static PrerenderContents* PrerenderContentsFromRenderFrame(
      int render_process_id, int render_frame_id);

  net::URLRequest* request_;

  scoped_refptr<PrerenderThrottleInfo> prerender_throttle_info_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderResourceThrottle);
};

}  // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_RESOURCE_THROTTLE_H_
