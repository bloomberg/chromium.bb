// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/signin/feature_flags.h"

#include "base/ios/ios_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const base::Feature kSSOWithWKWebView{"SSOWithWKWebView",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

bool ShouldEnableWKWebViewWithSSO() {
  if (!base::ios::IsRunningOnIOS12OrLater())
    return false;
  return base::FeatureList::IsEnabled(kSSOWithWKWebView);
}
