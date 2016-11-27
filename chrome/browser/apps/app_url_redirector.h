// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_URL_REDIRECTOR_H_
#define CHROME_BROWSER_APPS_APP_URL_REDIRECTOR_H_

#include <memory>

#include "base/macros.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationHandle;
class NavigationThrottle;
}

// This class creates navigation throttles that redirect URLs to apps that have
// a matching URL handler in the 'url_handlers' manifest key. Note that this is
// a UI thread class.
class AppUrlRedirector {
 public:
  static std::unique_ptr<content::NavigationThrottle> MaybeCreateThrottleFor(
      content::NavigationHandle* handle);

 private:
  DISALLOW_COPY_AND_ASSIGN(AppUrlRedirector);
};

#endif  // CHROME_BROWSER_APPS_APP_URL_REDIRECTOR_H_
