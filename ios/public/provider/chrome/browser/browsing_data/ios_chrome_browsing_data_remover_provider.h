// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_BROWSING_DATA_IOS_CHROME_BROWSING_DATA_REMOVER_PROVIDER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_BROWSING_DATA_IOS_CHROME_BROWSING_DATA_REMOVER_PROVIDER_H_

#include "base/basictypes.h"
#include "base/callback_forward.h"

class GURL;

namespace base {
class Time;
}

namespace net {
class URLRequestContextGetter;
}

namespace ios {

class ChromeBrowserState;

// A class that provides additional functionality to
// IOSChromeBrowsingDataRemover.
class IOSChromeBrowsingDataRemoverProvider {
 public:
  virtual ~IOSChromeBrowsingDataRemoverProvider() {}

  // Clears the hostname resolution cache and runs |callback| on completion.
  // If the hostname resolution cache doesn't exist, runs |callback|
  // immediately.
  virtual void ClearHostnameResolutionCache(const base::Closure& callback) = 0;

  // Gets the URLRequestContextGetter used by the SafeBrowsing service. Returns
  // null if there is no SafeBrowsing service.
  virtual net::URLRequestContextGetter* GetSafeBrowsingURLRequestContext() = 0;

  // Clears the relevant storage partition and runs |callback| on
  // completion.
  virtual void ClearStoragePartition(const GURL& remove_url,
                                     base::Time delete_begin,
                                     base::Time delete_end,
                                     const base::Closure& callback) = 0;
};

}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_BROWSING_DATA_IOS_CHROME_BROWSING_DATA_REMOVER_PROVIDER_H_
