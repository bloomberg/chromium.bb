// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/url_schemes.h"

#include <algorithm>
#include <vector>

#import "ios/web/public/web_client.h"
#include "url/url_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

namespace {
void AddStandardSchemeHelper(const url::SchemeWithType& scheme) {
  url::AddStandardScheme(scheme.scheme, scheme.type);
}
}  // namespace

void RegisterWebSchemes(bool lock_schemes) {
  std::vector<url::SchemeWithType> additional_standard_schemes;
  GetWebClient()->AddAdditionalSchemes(&additional_standard_schemes);
  std::for_each(additional_standard_schemes.begin(),
                additional_standard_schemes.end(), AddStandardSchemeHelper);

  // Prevent future modification of the schemes lists. This is to prevent
  // accidental creation of data races in the program. Add*Scheme aren't
  // threadsafe so must be called when GURL isn't used on any other thread. This
  // is really easy to mess up, so we say that all calls to Add*Scheme in Chrome
  // must be inside this function.
  if (lock_schemes)
    url::LockSchemeRegistries();
}

}  // namespace web
