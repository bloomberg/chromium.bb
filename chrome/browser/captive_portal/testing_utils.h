// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CAPTIVE_PORTAL_TESTING_UTILS_H_
#define CHROME_BROWSER_CAPTIVE_PORTAL_TESTING_UTILS_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "chrome/browser/captive_portal/captive_portal_detector.h"
#include "net/http/http_response_headers.h"

namespace base {
class Time;
}

namespace net {
class URLFetcher;
}

namespace captive_portal {

scoped_refptr<net::HttpResponseHeaders> CreateResponseHeaders(
    const std::string& response_headers);

class CaptivePortalDetectorTestBase {
 public:
  CaptivePortalDetectorTestBase();
  virtual ~CaptivePortalDetectorTestBase();

  // Sets test time for captive portal detector.
  void SetTime(const base::Time& time);

  // Advances test time for captive portal detector.
  void AdvanceTime(const base::TimeDelta& time_delta);

  bool FetchingURL();

  // Calls the corresponding CaptivePortalDetector function.
  void OnURLFetchComplete(net::URLFetcher* fetcher);

  void set_detector(CaptivePortalDetector* detector) { detector_ = detector; }

  CaptivePortalDetector* detector() { return detector_; }

 protected:
  CaptivePortalDetector* detector_;
};

}  // namespace captive_portal

#endif  // CHROME_BROWSER_CAPTIVE_PORTAL_TESTING_UTILS_H_
