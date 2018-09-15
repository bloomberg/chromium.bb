// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREVIEWS_PREVIEWS_LITE_PAGE_NAVIGATION_THROTTLE_MANAGER_H_
#define CHROME_BROWSER_PREVIEWS_PREVIEWS_LITE_PAGE_NAVIGATION_THROTTLE_MANAGER_H_

#include <stdint.h>

#include "base/time/time.h"

// This interface specifies the interaction that a
// |PreviewsLitePageNavigationThrottle| has with it's state manager. This class
// tracks the state of the Navigation Throttle since a single instance of the
// navigation throttle can be very short lived, and therefore is not aware of
// any actions taken by its predecessor.
class PreviewsLitePageNavigationThrottleManager {
 public:
  // Used to notify that the Previews Server should not be sent anymore requests
  // until after the given duration.
  virtual void SetServerUnavailableFor(base::TimeDelta retry_after) = 0;

  // Returns true if a Preview should not be triggered because the server is
  // unavailable.
  virtual bool IsServerUnavailable() = 0;

  // Informs the manager that the given URL should be bypassed one time.
  virtual void AddSingleBypass(std::string url) = 0;

  // Queries the manager if the given URL should be bypassed one time, returning
  // true if yes.
  virtual bool CheckSingleBypass(std::string url) = 0;

  // Generates a new page id for a request to the previews server.
  virtual uint64_t GeneratePageID() = 0;
};

#endif  // CHROME_BROWSER_PREVIEWS_PREVIEWS_LITE_PAGE_NAVIGATION_THROTTLE_MANAGER_H_
