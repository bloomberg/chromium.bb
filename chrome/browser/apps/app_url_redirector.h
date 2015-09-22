// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_URL_REDIRECTOR_H_
#define CHROME_BROWSER_APPS_APP_URL_REDIRECTOR_H_

#include "base/basictypes.h"

namespace content {
class ResourceThrottle;
}

namespace net {
class URLRequest;
}

class ProfileIOData;

// This class creates resource throttles that redirect URLs to apps that
// have a matching URL handler in the 'url_handlers' manifest key.
class AppUrlRedirector {
 public:
  static content::ResourceThrottle* MaybeCreateThrottleFor(
      net::URLRequest* request,
      ProfileIOData* profile_io_data);

 private:
  DISALLOW_COPY_AND_ASSIGN(AppUrlRedirector);
};

#endif  // CHROME_BROWSER_APPS_APP_URL_REDIRECTOR_H_
