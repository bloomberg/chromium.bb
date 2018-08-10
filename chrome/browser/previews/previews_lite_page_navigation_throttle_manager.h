// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREVIEWS_PREVIEWS_LITE_PAGE_NAVIGATION_THROTTLE_MANAGER_H_
#define CHROME_BROWSER_PREVIEWS_PREVIEWS_LITE_PAGE_NAVIGATION_THROTTLE_MANAGER_H_

#include "base/time/time.h"

// This interface specifies the interaction that a
// |PreviewsLitePageNavigationThrottle| has with it's state manager. This class
// tracks the state of the Navigation Throttle since a single instance of the
// navigation throttle can be very short lived, and therefore is not aware of
// any actions taken by its predecessor.
class PreviewsLitePageNavigationThrottleManager {
 public:
  // Used to notify that the Previews Server should not be sent anymore requests
  // until after the given time.
  virtual void SetServerUnavailableUntil(base::TimeTicks retry_at) = 0;

  // Returns true if a Preview should not be triggered because the server is
  // unavailable.
  virtual bool IsServerUnavailable(base::TimeTicks now) = 0;
};

#endif  // CHROME_BROWSER_PREVIEWS_PREVIEWS_LITE_PAGE_NAVIGATION_THROTTLE_MANAGER_H_
