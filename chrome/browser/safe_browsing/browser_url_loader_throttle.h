// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_BROWSER_URL_LOADER_THROTTLE_H_
#define CHROME_BROWSER_SAFE_BROWSING_BROWSER_URL_LOADER_THROTTLE_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/public/common/url_loader_throttle.h"

namespace content {
class WebContents;
}

namespace safe_browsing {

class SafeBrowsingDatabaseManager;
class SafeBrowsingUIManager;
class SafeBrowsingUrlCheckerImpl;

// BrowserURLLoaderThrottle is used in the browser process to query
// SafeBrowsing to determine whether a URL and also its redirect URLs are safe
// to load. It defers response processing until all URL checks are completed;
// cancels the load if any URLs turn out to be bad.
// Used when --enable-network-service is in effect.
class BrowserURLLoaderThrottle : public content::URLLoaderThrottle {
 public:
  // |web_contents_getter| is used for displaying SafeBrowsing UI when
  // necessary.
  BrowserURLLoaderThrottle(
      scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
      scoped_refptr<SafeBrowsingUIManager> ui_manager,
      const base::Callback<content::WebContents*()>& web_contents_getter);
  ~BrowserURLLoaderThrottle() override;

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

  // The following two members stay valid until |url_checker_| is created.
  scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;
  scoped_refptr<SafeBrowsingUIManager> ui_manager_;

  base::Callback<content::WebContents*()> web_contents_getter_;

  std::unique_ptr<SafeBrowsingUrlCheckerImpl> url_checker_;

  size_t pending_checks_ = 0;
  bool blocked_ = false;

  DISALLOW_COPY_AND_ASSIGN(BrowserURLLoaderThrottle);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_BROWSER_URL_LOADER_THROTTLE_H_
