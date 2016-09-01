// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGINS_FLASH_DOWNLOAD_INTERCEPTION_H_
#define CHROME_BROWSER_PLUGINS_FLASH_DOWNLOAD_INTERCEPTION_H_

#include <memory>

#include "base/macros.h"

namespace content {
class NavigationHandle;
class NavigationThrottle;
}

// This class creates navigation throttles that intercept navigations to Flash's
// download page. The user is queried about activating Flash instead, since
// Chrome already ships with it. Note that this is an UI thread class.
class FlashDownloadInterception {
 public:
  static std::unique_ptr<content::NavigationThrottle> MaybeCreateThrottleFor(
      content::NavigationHandle* handle);

 private:
  DISALLOW_COPY_AND_ASSIGN(FlashDownloadInterception);
};

#endif  // CHROME_BROWSER_PLUGINS_FLASH_DOWNLOAD_INTERCEPTION_H_
