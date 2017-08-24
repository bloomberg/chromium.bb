// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_NTP_HOME_METRICS_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_NTP_HOME_METRICS_H_

#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"

namespace ntp_home {

// Records an NTP impression of type |impression_type|.
void RecordNTPImpression(ntp_home::IOSNTPImpression impression_type);

}  // namespace ntp_home

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_NTP_HOME_METRICS_H_
