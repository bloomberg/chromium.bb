// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SSL_CAPTIVE_PORTAL_DETECTOR_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_SSL_CAPTIVE_PORTAL_DETECTOR_TAB_HELPER_H_

#import "ios/web/public/web_state/web_state_user_data.h"

namespace captive_portal {
class CaptivePortalDetector;
}

namespace web {
class WebState;
}

// Associates a Tab to a CaptivePortalDetector and manages its lifetime.
class CaptivePortalDetectorTabHelper
    : public web::WebStateUserData<CaptivePortalDetectorTabHelper> {
 public:
  explicit CaptivePortalDetectorTabHelper(web::WebState* web_state);

  captive_portal::CaptivePortalDetector* detector();

 private:
  ~CaptivePortalDetectorTabHelper() override;

  // The underlying CaptivePortalDetector.
  std::unique_ptr<captive_portal::CaptivePortalDetector> detector_;

  DISALLOW_COPY_AND_ASSIGN(CaptivePortalDetectorTabHelper);
};

#endif  // IOS_CHROME_BROWSER_SSL_CAPTIVE_PORTAL_DETECTOR_TAB_HELPER_H_
