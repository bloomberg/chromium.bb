// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_util.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "content/public/browser/resource_request_info.h"
#include "googleurl/src/url_canon.h"
#include "googleurl/src/url_parse.h"
#include "googleurl/src/url_util.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"

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

bool IsWebURL(const GURL& url) {
  return url.SchemeIs("http") || url.SchemeIs("https");
}

bool IsNoSwapInExperiment(uint8 experiment_id) {
  // Currently, experiments 5 and 6 fall in this category.
  return experiment_id == 5 || experiment_id == 6;
}

bool IsControlGroupExperiment(uint8 experiment_id) {
  // Currently, experiments 7 and 8 fall in this category.
  return experiment_id == 7 || experiment_id == 8;
}

void URLRequestResponseStarted(net::URLRequest* request) {
  static const char* kModPagespeedHeader = "X-Mod-Pagespeed";
  static const char* kModPagespeedHistogram = "Prerender.ModPagespeedHeader";
  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request);
  // Gather histogram information about the X-Mod-Pagespeed header.
  if (info->GetResourceType() == ResourceType::MAIN_FRAME &&
      IsWebURL(request->url())) {
    UMA_HISTOGRAM_ENUMERATION(kModPagespeedHistogram, 0, 101);
    if (request->response_headers() &&
        request->response_headers()->HasHeader(kModPagespeedHeader)) {
      UMA_HISTOGRAM_ENUMERATION(kModPagespeedHistogram, 1, 101);

      // Attempt to parse the version number, and encode it in buckets
      // 2 through 99. 0 and 1 are used to store all pageviews and
      // # pageviews with the MPS header (see above).
      void* iter = NULL;
      std::string mps_version;
      if (request->response_headers()->EnumerateHeader(
              &iter, kModPagespeedHeader, &mps_version) &&
          !mps_version.empty()) {
        // Mod Pagespeed versions are of the form a.b.c.d-e
        int a, b, c, d, e;
        int num_parsed = sscanf(mps_version.c_str(), "%d.%d.%d.%d-%d",
                                &a, &b, &c, &d, &e);
        if (num_parsed == 5) {
          int output = 2;
          if (c > 10)
            output += 2 * (c - 10);
          if (d > 1)
            output++;
          if (output < 2 || output >= 99)
            output = 99;
          UMA_HISTOGRAM_ENUMERATION(kModPagespeedHistogram, output, 101);
        }
      }
    }
  }
}

}  // namespace prerender
