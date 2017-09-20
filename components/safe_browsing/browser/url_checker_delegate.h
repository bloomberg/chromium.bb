// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_BROWSER_URL_CHECKER_DELEGATE_H_
#define COMPONENTS_SAFE_BROWSING_BROWSER_URL_CHECKER_DELEGATE_H_

#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "components/safe_browsing/db/v4_protocol_manager_util.h"

namespace content {
class WebContents;
}

namespace net {
class HttpRequestHeaders;
}

namespace security_interstitials {
struct UnsafeResource;
}

namespace safe_browsing {

class BaseUIManager;
class SafeBrowsingDatabaseManager;

// Delegate interface for SafeBrowsingUrlCheckerImpl and SafeBrowsing's
// content::ResourceThrottle subclasses. They delegate to this interface those
// operations that different embedders (Chrome and Android WebView) handle
// differently.
//
// All methods should only be called from the IO thread.
class UrlCheckerDelegate
    : public base::RefCountedThreadSafe<UrlCheckerDelegate> {
 public:
  // Destroys prerender contents if necessary.
  virtual void MaybeDestroyPrerenderContents(
      const base::Callback<content::WebContents*()>& web_contents_getter) = 0;

  // Starts displaying the SafeBrowsing interstitial page.
  virtual void StartDisplayingBlockingPageHelper(
      const security_interstitials::UnsafeResource& resource,
      const std::string& method,
      const net::HttpRequestHeaders& headers,
      bool is_main_frame,
      bool has_user_gesture) = 0;

  // A whitelisted URL is considered safe and therefore won't be checked with
  // the SafeBrowsing database.
  virtual bool IsUrlWhitelisted(const GURL& url) = 0;

  virtual const SBThreatTypeSet& GetThreatTypes() = 0;
  virtual SafeBrowsingDatabaseManager* GetDatabaseManager() = 0;
  virtual BaseUIManager* GetUIManager() = 0;

 protected:
  friend class base::RefCountedThreadSafe<UrlCheckerDelegate>;
  virtual ~UrlCheckerDelegate() {}
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_BROWSER_URL_CHECKER_DELEGATE_H_
