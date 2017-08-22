// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOADER_SAFE_BROWSING_RESOURCE_THROTTLE_H_
#define CHROME_BROWSER_LOADER_SAFE_BROWSING_RESOURCE_THROTTLE_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/safe_browsing/url_checker_delegate_impl.h"
#include "components/safe_browsing/base_resource_throttle.h"
#include "components/safe_browsing/browser/base_parallel_resource_throttle.h"
#include "content/public/browser/resource_throttle.h"
#include "content/public/common/resource_type.h"

namespace net {
class URLRequest;
}

namespace safe_browsing {
class SafeBrowsingService;
}

// Contructs a resource throttle for SafeBrowsing. It returns a
// SafeBrowsingParallelResourceThrottle instance if
// --enable-features=S13nSafeBrowsingParallelUrlCheck is specified; returns
// a SafeBrowsingResourceThrottle otherwise.
//
// It could return nullptr if URL checking is not supported on this
// build+device.
content::ResourceThrottle* MaybeCreateSafeBrowsingResourceThrottle(
    net::URLRequest* request,
    content::ResourceType resource_type,
    safe_browsing::SafeBrowsingService* sb_service);

// SafeBrowsingResourceThrottle functions as its base class
// safe_browsing::BaseResourceThrottle, but dispatches to either the local or
// remote SafeBrowsingDatabaseManager, depending on if running on desktop or
// mobile. It also has its own chrome-specific prerender check of redirect URLs
// inside StartDisplayingBlockingPage().
//
// See also safe_browsing::BaseResourceThrottle for details on how the safe
// browsing check occurs.
//
// On desktop (ifdef SAFE_BROWSING_DB_LOCAL)
// -----------------------------------------
// This check is done before requesting the original URL, and additionally
// before following any subsequent redirect.  In the common case the check
// completes synchronously (no match in the in-memory DB), so the request's
// flow is un-interrupted.  However if the URL fails this quick check, it
// has the possibility of being on the blacklist. Now the request is
// deferred (prevented from starting), and a more expensive safe browsing
// check is begun (fetches the full hashes).
//
// On mobile (ifdef SAFE_BROWSING_DB_REMOTE):
// -----------------------------------------
// The check is started and runs in parallel with the resource load.  If the
// check is not complete by the time the headers are loaded, the request is
// suspended until the URL is classified.  We let the headers load on mobile
// since the RemoteSafeBrowsingDatabase checks always have some non-zero
// latency -- there no synchronous pass.  This parallelism helps
// performance.  Redirects are handled the same way as desktop so they
// always defer.
class SafeBrowsingResourceThrottle
    : public safe_browsing::BaseResourceThrottle {
 private:
  friend content::ResourceThrottle* MaybeCreateSafeBrowsingResourceThrottle(
      net::URLRequest* request,
      content::ResourceType resource_type,
      safe_browsing::SafeBrowsingService* sb_service);

  SafeBrowsingResourceThrottle(const net::URLRequest* request,
                               content::ResourceType resource_type,
                               safe_browsing::SafeBrowsingService* sb_service);

  ~SafeBrowsingResourceThrottle() override;

  const char* GetNameForLogging() const override;

  // This posts a task to destroy prerender contents
  void MaybeDestroyPrerenderContents(
      const content::ResourceRequestInfo* info) override;

  void StartDisplayingBlockingPageHelper(
      security_interstitials::UnsafeResource resource) override;

  scoped_refptr<safe_browsing::UrlCheckerDelegate> url_checker_delegate_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingResourceThrottle);
};

// Unlike SafeBrowsingResourceThrottle, this class never defers starting the URL
// request or following redirects, no matter on mobile or desktop. If any of the
// checks for the original URL and redirect chain are not complete by the time
// the response headers are available, the request is deferred until all the
// checks are done.
class SafeBrowsingParallelResourceThrottle
    : public safe_browsing::BaseParallelResourceThrottle {
 private:
  friend content::ResourceThrottle* MaybeCreateSafeBrowsingResourceThrottle(
      net::URLRequest* request,
      content::ResourceType resource_type,
      safe_browsing::SafeBrowsingService* sb_service);

  SafeBrowsingParallelResourceThrottle(
      const net::URLRequest* request,
      content::ResourceType resource_type,
      safe_browsing::SafeBrowsingService* sb_service);

  ~SafeBrowsingParallelResourceThrottle() override;

  const char* GetNameForLogging() const override;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingParallelResourceThrottle);
};

#endif  // CHROME_BROWSER_LOADER_SAFE_BROWSING_RESOURCE_THROTTLE_H_
