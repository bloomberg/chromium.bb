// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/navigation/placeholder_navigation_util.h"

#include <string>

#include "base/strings/string_util.h"
#include "net/base/escape.h"
#include "net/base/url_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {
namespace placeholder_navigation_util {

bool IsPlaceholderUrl(const GURL& url) {
  return url.IsAboutBlank() &&
         base::StartsWith(url.query(), "for=", base::CompareCase::SENSITIVE);
}

GURL CreatePlaceholderUrlForUrl(const GURL& original_url) {
  if (!original_url.is_valid())
    return GURL::EmptyGURL();

  GURL::Replacements query_replacements;
  std::string encoded = "for=" + net::EscapeQueryParamValue(
                                     original_url.spec(), false /* use_plus */);
  query_replacements.SetQueryStr(encoded);
  GURL placeholder_url =
      GURL(url::kAboutBlankURL).ReplaceComponents(query_replacements);
  DCHECK(placeholder_url.is_valid());
  return placeholder_url;
}

GURL ExtractUrlFromPlaceholderUrl(const GURL& url) {
  std::string value;
  if (IsPlaceholderUrl(url) && net::GetValueForKeyInQuery(url, "for", &value)) {
    GURL decoded_url(value);
    if (decoded_url.is_valid())
      return decoded_url;
  }
  return GURL::EmptyGURL();
}

}  // namespace placeholder_navigation_util
}  // namespace
