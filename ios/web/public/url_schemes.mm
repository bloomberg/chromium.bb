// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/url_schemes.h"

#include <algorithm>
#include <vector>

#include "ios/web/public/web_client.h"
#include "url/url_util.h"

namespace web {

namespace {
void AddStandardSchemeHelper(const url::SchemeWithType& scheme) {
  url::AddStandardScheme(scheme.scheme, scheme.type);
}
}  // namespace

void RegisterWebSchemes(bool lock_standard_schemes) {
  std::vector<url::SchemeWithType> additional_standard_schemes;
  GetWebClient()->AddAdditionalSchemes(&additional_standard_schemes);
  std::for_each(additional_standard_schemes.begin(),
                additional_standard_schemes.end(), AddStandardSchemeHelper);

  // Prevent future modification of the standard schemes list. This is to
  // prevent accidental creation of data races in the program. AddStandardScheme
  // isn't threadsafe so must be called when GURL isn't used on any other
  // thread. This is really easy to mess up, so we say that all calls to
  // AddStandardScheme in Chrome must be inside this function.
  if (lock_standard_schemes)
    url::LockStandardSchemes();
}

}  // namespace web
