// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_RESOURCE_THROTTLE_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_RESOURCE_THROTTLE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/resource_throttle.h"
#include "content/public/common/resource_type.h"

namespace net {
class URLRequest;
}

namespace prerender {
class PrerenderContents;

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

  // content::ResourceThrottle implementation:
  virtual void WillStartRequest(bool* defer) OVERRIDE;
  virtual void WillRedirectRequest(const GURL& new_url, bool* defer) OVERRIDE;
  virtual const char* GetNameForLogging() const OVERRIDE;

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
      const GURL& url);

  static void WillRedirectRequestOnUI(
      const base::WeakPtr<PrerenderResourceThrottle>& throttle,
      const std::string& follow_only_when_prerender_shown_header,
      content::ResourceType resource_type,
      bool async,
      int render_process_id,
      int render_frame_id,
      const GURL& new_url);

  // Helper to return the PrerenderContents given a render frame id. May return
  // NULL if it's gone.
  static PrerenderContents* PrerenderContentsFromRenderFrame(
      int render_process_id, int render_frame_id);

  net::URLRequest* request_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderResourceThrottle);
};

}  // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_RESOURCE_THROTTLE_H_
