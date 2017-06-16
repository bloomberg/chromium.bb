// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_URL_CHECKER_IMPL_H_
#define CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_URL_CHECKER_IMPL_H_

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

namespace security_interstitials {
struct UnsafeResource;
}

namespace safe_browsing {

class SafeBrowsingUIManager;
class BaseUIManager;

// A SafeBrowsingUrlCheckerImpl instance is used to perform SafeBrowsing check
// for a URL and its redirect URLs. It implements Mojo interface so that it can
// be used to handle queries from renderers. But it is also used to handle
// queries from the browser. In that case, the public methods are called
// directly instead of through Mojo.
// Used when --enable-network-service is in effect.
//
// TODO(yzshen): Handle the case where SafeBrowsing is not enabled, or
// !database_manager()->IsSupported().
// TODO(yzshen): Make sure it also works on Andorid.
// TODO(yzshen): Do all the logging like what BaseResourceThrottle does.
class SafeBrowsingUrlCheckerImpl : public mojom::SafeBrowsingUrlChecker,
                                   public SafeBrowsingDatabaseManager::Client {
 public:
  SafeBrowsingUrlCheckerImpl(
      int load_flags,
      content::ResourceType resource_type,
      scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
      scoped_refptr<SafeBrowsingUIManager> ui_manager,
      const base::Callback<content::WebContents*()>& web_contents_getter);

  ~SafeBrowsingUrlCheckerImpl() override;

  // mojom::SafeBrowsingUrlChecker implementation.
  void CheckUrl(const GURL& url, CheckUrlCallback callback) override;

 private:
  // SafeBrowsingDatabaseManager::Client implementation:
  void OnCheckBrowseUrlResult(const GURL& url,
                              SBThreatType threat_type,
                              const ThreatMetadata& metadata) override;

  static void StartDisplayingBlockingPage(
      const base::WeakPtr<SafeBrowsingUrlCheckerImpl>& checker,
      scoped_refptr<BaseUIManager> ui_manager,
      const security_interstitials::UnsafeResource& resource);

  void OnCheckUrlTimeout();

  void ProcessUrls();

  void BlockAndProcessUrls();

  void OnBlockingPageComplete(bool proceed);

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

  const int load_flags_;
  const content::ResourceType resource_type_;
  base::Callback<content::WebContents*()> web_contents_getter_;
  scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;
  scoped_refptr<BaseUIManager> ui_manager_;

  // The redirect chain for this resource, including the original URL and
  // subsequent redirect URLs.
  std::vector<GURL> urls_;
  // Callbacks corresponding to |urls_| to report check results. |urls_| and
  // |callbacks_| are always of the same size.
  std::vector<CheckUrlCallback> callbacks_;
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

#endif  // CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_URL_CHECKER_IMPL_H_
