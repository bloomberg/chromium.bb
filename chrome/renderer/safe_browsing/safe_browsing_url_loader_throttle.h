// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SAFE_BROWSING_SAFE_BROWSING_URL_LOADER_THROTTLE_H_
#define CHROME_RENDERER_SAFE_BROWSING_SAFE_BROWSING_URL_LOADER_THROTTLE_H_

#include "base/memory/weak_ptr.h"
#include "components/safe_browsing/common/safe_browsing.mojom.h"
#include "content/public/common/url_loader_throttle.h"

namespace chrome {
namespace mojom {
class SafeBrowsing;
}
}

namespace safe_browsing {

// SafeBrowsingURLLoaderThrottle queries SafeBrowsing to determine whether the
// URL and also redirect URLs are safe to load. It defers response processing
// until all URL checks are completed; cancels the load if any URLs turn out to
// be bad.
class SafeBrowsingURLLoaderThrottle : public content::URLLoaderThrottle {
 public:
  // |safe_browsing| must stay alive until WillStartRequest() (if it is called)
  // or the end of this object.
  // |render_frame_id| is used for displaying SafeBrowsing UI when necessary.
  SafeBrowsingURLLoaderThrottle(mojom::SafeBrowsing* safe_browsing,
                                int render_frame_id);
  ~SafeBrowsingURLLoaderThrottle() override;

  // content::URLLoaderThrottle implementation.
  void WillStartRequest(const GURL& url,
                        int load_flags,
                        content::ResourceType resource_type,
                        bool* defer) override;
  void WillRedirectRequest(const net::RedirectInfo& redirect_info,
                           bool* defer) override;
  void WillProcessResponse(bool* defer) override;

 private:
  void OnCheckUrlResult(bool safe);

  void OnConnectionError();

  mojom::SafeBrowsing* safe_browsing_;
  const int render_frame_id_;

  mojom::SafeBrowsingUrlCheckerPtr url_checker_;

  size_t pending_checks_ = 0;
  bool blocked_ = false;

  base::WeakPtrFactory<SafeBrowsingURLLoaderThrottle> weak_factory_;
};

}  // namespace safe_browsing

#endif  // CHROME_RENDERER_SAFE_BROWSING_SAFE_BROWSING_URL_LOADER_THROTTLE_H_
