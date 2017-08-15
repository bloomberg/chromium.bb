// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_BROWSER_SAFE_BROWSING_URL_CHECKER_IMPL_H_
#define COMPONENTS_SAFE_BROWSING_BROWSER_SAFE_BROWSING_URL_CHECKER_IMPL_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "components/safe_browsing/common/safe_browsing.mojom.h"
#include "components/safe_browsing_db/database_manager.h"
#include "content/public/common/resource_type.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace safe_browsing {

class UrlCheckerDelegate;

// A SafeBrowsingUrlCheckerImpl instance is used to perform SafeBrowsing check
// for a URL and its redirect URLs. It implements Mojo interface so that it can
// be used to handle queries from renderers. But it is also used to handle
// queries from the browser. In that case, the public methods are called
// directly instead of through Mojo.
// Used when --enable-network-service is in effect.
class SafeBrowsingUrlCheckerImpl : public mojom::SafeBrowsingUrlChecker,
                                   public SafeBrowsingDatabaseManager::Client {
 public:
  SafeBrowsingUrlCheckerImpl(
      const std::string& headers,
      int load_flags,
      content::ResourceType resource_type,
      bool has_user_gesture,
      scoped_refptr<UrlCheckerDelegate> url_checker_delegate,
      const base::Callback<content::WebContents*()>& web_contents_getter);

  ~SafeBrowsingUrlCheckerImpl() override;

  // mojom::SafeBrowsingUrlChecker implementation.
  void CheckUrl(const GURL& url,
                const std::string& method,
                CheckUrlCallback callback) override;

 private:
  // SafeBrowsingDatabaseManager::Client implementation:
  void OnCheckBrowseUrlResult(const GURL& url,
                              SBThreatType threat_type,
                              const ThreatMetadata& metadata) override;

  void OnCheckUrlTimeout();

  void ProcessUrls();

  void BlockAndProcessUrls(bool showed_interstitial);

  void OnBlockingPageComplete(bool proceed);

  SBThreatType CheckWebUIUrls(const GURL& url);

  void RunNextCallback(bool proceed, bool showed_interstitial);

  enum State {
    // Haven't started checking or checking is complete.
    STATE_NONE,
    // We have one outstanding URL-check.
    STATE_CHECKING_URL,
    // We're displaying a blocking page.
    STATE_DISPLAYING_BLOCKING_PAGE,
    // The blocking page has returned *not* to proceed.
    STATE_BLOCKED
  };

  struct UrlInfo {
    UrlInfo(const GURL& url,
            const std::string& method,
            CheckUrlCallback callback);
    UrlInfo(UrlInfo&& other);

    ~UrlInfo();

    GURL url;
    std::string method;
    CheckUrlCallback callback;
  };

  const std::string headers_;
  const int load_flags_;
  const content::ResourceType resource_type_;
  const bool has_user_gesture_;
  base::Callback<content::WebContents*()> web_contents_getter_;
  scoped_refptr<UrlCheckerDelegate> url_checker_delegate_;
  scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;

  // The redirect chain for this resource, including the original URL and
  // subsequent redirect URLs.
  std::vector<UrlInfo> urls_;
  // |urls_| before |next_index_| have been checked. If |next_index_| is smaller
  // than the size of |urls_|, the URL at |next_index_| is being processed.
  size_t next_index_ = 0;

  State state_ = STATE_NONE;

  // Timer to abort the SafeBrowsing check if it takes too long.
  base::OneShotTimer timer_;

  base::WeakPtrFactory<SafeBrowsingUrlCheckerImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingUrlCheckerImpl);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_BROWSER_SAFE_BROWSING_URL_CHECKER_IMPL_H_
