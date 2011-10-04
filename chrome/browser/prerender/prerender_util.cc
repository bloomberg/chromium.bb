// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_util.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "googleurl/src/url_canon.h"
#include "googleurl/src/url_parse.h"
#include "googleurl/src/url_util.h"

namespace prerender {

bool MaybeGetQueryStringBasedAliasURL(
    const GURL& url, GURL* alias_url) {
  DCHECK(alias_url);
  url_parse::Parsed parsed;
  url_parse::ParseStandardURL(url.spec().c_str(), url.spec().length(),
                              &parsed);
  url_parse::Component query = parsed.query;
  url_parse::Component key, value;
  while (url_parse::ExtractQueryKeyValue(url.spec().c_str(), &query, &key,
                                         &value)) {
    if (key.len != 3 || strncmp(url.spec().c_str() + key.begin, "url", key.len))
      continue;
    // We found a url= query string component.
    if (value.len < 1)
      continue;
    url_canon::RawCanonOutputW<1024> decoded_url;
    url_util::DecodeURLEscapeSequences(url.spec().c_str() + value.begin,
                                       value.len, &decoded_url);
    GURL new_url(string16(decoded_url.data(), decoded_url.length()));
    if (!new_url.is_empty() && new_url.is_valid()) {
      *alias_url = new_url;
      return true;
    }
    return false;
  }
  return false;
}

uint8 GetQueryStringBasedExperiment(const GURL& url) {
  url_parse::Parsed parsed;
  url_parse::ParseStandardURL(url.spec().c_str(), url.spec().length(),
                              &parsed);
  url_parse::Component query = parsed.query;
  url_parse::Component key, value;
  while (url_parse::ExtractQueryKeyValue(url.spec().c_str(), &query, &key,
                                         &value)) {
    if (key.len != 3 || strncmp(url.spec().c_str() + key.begin, "lpe", key.len))
      continue;

    // We found a lpe= query string component.
    if (value.len != 1)
      continue;
    uint8 exp = *(url.spec().c_str() + value.begin) - '0';
    if (exp < 1 || exp > 9)
      continue;
    return exp;
  }
  return kNoExperiment;
}

bool IsGoogleDomain(const GURL& url) {
  return StartsWithASCII(url.host(), std::string("www.google."), true);
}

bool IsGoogleSearchResultURL(const GURL& url) {
  if (!IsGoogleDomain(url))
    return false;
  return (url.path().empty() ||
          StartsWithASCII(url.path(), std::string("/search"), true) ||
          (url.path() == "/") ||
          StartsWithASCII(url.path(), std::string("/webhp"), true));
}

}  // namespace prerender
