// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_util.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/strings/string_util.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/resource_type.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_canon.h"
#include "url/url_util.h"

using content::ResourceType;

namespace prerender {

namespace {

enum PrerenderSchemeCancelReason {
  PRERENDER_SCHEME_CANCEL_REASON_EXTERNAL_PROTOCOL,
  PRERENDER_SCHEME_CANCEL_REASON_DATA,
  PRERENDER_SCHEME_CANCEL_REASON_BLOB,
  PRERENDER_SCHEME_CANCEL_REASON_FILE,
  PRERENDER_SCHEME_CANCEL_REASON_FILESYSTEM,
  PRERENDER_SCHEME_CANCEL_REASON_WEBSOCKET,
  PRERENDER_SCHEME_CANCEL_REASON_FTP,
  PRERENDER_SCHEME_CANCEL_REASON_CHROME,
  PRERENDER_SCHEME_CANCEL_REASON_CHROME_EXTENSION,
  PRERENDER_SCHEME_CANCEL_REASON_ABOUT,
  PRERENDER_SCHEME_CANCEL_REASON_UNKNOWN,
  PRERENDER_SCHEME_CANCEL_REASON_MAX,
};

void ReportPrerenderSchemeCancelReason(PrerenderSchemeCancelReason reason) {
  UMA_HISTOGRAM_ENUMERATION(
      "Prerender.SchemeCancelReason", reason,
      PRERENDER_SCHEME_CANCEL_REASON_MAX);
}

}  // namespace

bool MaybeGetQueryStringBasedAliasURL(
    const GURL& url, GURL* alias_url) {
  DCHECK(alias_url);
  url::Parsed parsed;
  url::ParseStandardURL(url.spec().c_str(), url.spec().length(), &parsed);
  url::Component query = parsed.query;
  url::Component key, value;
  while (url::ExtractQueryKeyValue(url.spec().c_str(), &query, &key, &value)) {
    if (key.len != 3 || strncmp(url.spec().c_str() + key.begin, "url", key.len))
      continue;
    // We found a url= query string component.
    if (value.len < 1)
      continue;
    url::RawCanonOutputW<1024> decoded_url;
    url::DecodeURLEscapeSequences(url.spec().c_str() + value.begin, value.len,
                                  &decoded_url);
    GURL new_url(base::string16(decoded_url.data(), decoded_url.length()));
    if (!new_url.is_empty() && new_url.is_valid()) {
      *alias_url = new_url;
      return true;
    }
    return false;
  }
  return false;
}

bool IsGoogleDomain(const GURL& url) {
  return base::StartsWith(url.host(), "www.google.",
                          base::CompareCase::SENSITIVE);
}

bool IsGoogleSearchResultURL(const GURL& url) {
  if (!IsGoogleDomain(url))
    return false;
  return (url.path().empty() ||
          base::StartsWith(url.path(), "/search",
                           base::CompareCase::SENSITIVE) ||
          (url.path() == "/") ||
          base::StartsWith(url.path(), "/webhp", base::CompareCase::SENSITIVE));
}

void ReportPrerenderExternalURL() {
  ReportPrerenderSchemeCancelReason(
      PRERENDER_SCHEME_CANCEL_REASON_EXTERNAL_PROTOCOL);
}

void ReportUnsupportedPrerenderScheme(const GURL& url) {
  if (url.SchemeIs("data")) {
    ReportPrerenderSchemeCancelReason(PRERENDER_SCHEME_CANCEL_REASON_DATA);
  } else if (url.SchemeIs("blob")) {
    ReportPrerenderSchemeCancelReason(PRERENDER_SCHEME_CANCEL_REASON_BLOB);
  } else if (url.SchemeIsFile()) {
    ReportPrerenderSchemeCancelReason(PRERENDER_SCHEME_CANCEL_REASON_FILE);
  } else if (url.SchemeIsFileSystem()) {
    ReportPrerenderSchemeCancelReason(
        PRERENDER_SCHEME_CANCEL_REASON_FILESYSTEM);
  } else if (url.SchemeIs("ws") || url.SchemeIs("wss")) {
    ReportPrerenderSchemeCancelReason(PRERENDER_SCHEME_CANCEL_REASON_WEBSOCKET);
  } else if (url.SchemeIs("ftp")) {
    ReportPrerenderSchemeCancelReason(PRERENDER_SCHEME_CANCEL_REASON_FTP);
  } else if (url.SchemeIs("chrome")) {
    ReportPrerenderSchemeCancelReason(PRERENDER_SCHEME_CANCEL_REASON_CHROME);
  } else if (url.SchemeIs("chrome-extension")) {
    ReportPrerenderSchemeCancelReason(
        PRERENDER_SCHEME_CANCEL_REASON_CHROME_EXTENSION);
  } else if (url.SchemeIs("about")) {
    ReportPrerenderSchemeCancelReason(PRERENDER_SCHEME_CANCEL_REASON_ABOUT);
  } else {
    ReportPrerenderSchemeCancelReason(PRERENDER_SCHEME_CANCEL_REASON_UNKNOWN);
  }
}

}  // namespace prerender
