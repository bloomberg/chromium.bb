// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/handoff/handoff_utility.h"

namespace handoff {

NSString* const kChromeHandoffActivityType = @"com.google.chrome.handoff";
NSString* const kOriginKey = @"kOriginKey";
NSString* const kOriginiOS = @"kOriginiOS";
NSString* const kOriginMac = @"kOriginMac";

Origin OriginFromString(NSString* string) {
  if ([string isEqualToString:kOriginiOS])
    return ORIGIN_IOS;

  if ([string isEqualToString:kOriginMac])
    return ORIGIN_MAC;

  return ORIGIN_UNKNOWN;
}

}  // namespace handoff
