// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/search_urls.h"

#include "content/public/common/url_constants.h"
#include "url/gurl.h"

namespace search {

namespace {
bool MatchesOrigin(const GURL& my_url, const GURL& other_url) {
  return my_url.host() == other_url.host() &&
         my_url.port() == other_url.port() &&
         (my_url.scheme() == other_url.scheme() ||
          (my_url.SchemeIs(url::kHttpsScheme) &&
           other_url.SchemeIs(url::kHttpScheme)));
}
}  // namespace

bool MatchesOriginAndPath(const GURL& my_url, const GURL& other_url) {
  return MatchesOrigin(my_url, other_url) && my_url.path() == other_url.path();
}

} // namespace search
