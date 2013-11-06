// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_EPHEMERAL_APP_THROTTLE_H_
#define CHROME_BROWSER_APPS_EPHEMERAL_APP_THROTTLE_H_

#include "base/basictypes.h"

namespace content {
class ResourceThrottle;
}

namespace net {
class URLRequest;
}

class ProfileIOData;

// This class creates resource throttles that will launch ephemeral apps for
// links to Chrome Web Store detail pages instead of opening the page in the
// browser. This will only occur if the enable-ephemeral-apps switch is enabled.
class EphemeralAppThrottle {
 public:
  static content::ResourceThrottle* MaybeCreateThrottleForLaunch(
      net::URLRequest* request,
      ProfileIOData* profile_io_data);

 private:
  DISALLOW_COPY_AND_ASSIGN(EphemeralAppThrottle);
};

#endif  // CHROME_BROWSER_APPS_EPHEMERAL_APP_THROTTLE_H_
