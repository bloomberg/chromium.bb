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
#include "url/url_canon.h"
#include "url/url_parse.h"
#include "url/url_util.h"

using content::ResourceType;

namespace prerender {

namespace {

const char kModPagespeedHeader[] = "X-Mod-Pagespeed";
const char kPageSpeedHeader[] = "X-Page-Speed";
const char kPagespeedServerHistogram[] =
    "Prerender.PagespeedHeader.ServerCounts";
const char kPagespeedVersionHistogram[] =
    "Prerender.PagespeedHeader.VersionCounts";

enum PagespeedHeaderServerType {
  PAGESPEED_TOTAL_RESPONSES = 0,
  PAGESPEED_MOD_PAGESPEED_SERVER = 1,
  PAGESPEED_NGX_PAGESPEED_SERVER = 2,
  PAGESPEED_PAGESPEED_SERVICE_SERVER = 3,
  PAGESPEED_UNKNOWN_SERVER = 4,
  PAGESPEED_SERVER_MAXIMUM = 5
};

// Private function to parse the PageSpeed version number and encode it in
// buckets 2 through 99: if it is in the format a.b.c.d-e the bucket will be
// 2 + 2 * (max(c, 10) - 10) + (d > 1 ? 1 : 0); if it is not in this format
// we return zero.
int GetXModPagespeedBucketFromVersion(const std::string& version) {
  int a, b, c, d, e;
  int num_parsed = sscanf(version.c_str(), "%d.%d.%d.%d-%d",
                          &a, &b, &c, &d, &e);
  int output = 0;
  if (num_parsed == 5) {
    output = 2;
    if (c > 10)
      output += 2 * (c - 10);
    if (d > 1)
      output++;
    if (output < 2 || output > 99)
      output = 0;
  }
  return output;
}

// Private function to parse the X-Page-Speed header value and determine
// whether it is in the PageSpeed Service format, namely m_n_dc were m_n is
// a version number and dc is an encoded 2-character value.
bool IsPageSpeedServiceVersionNumber(const std::string& version) {
  int a, b;
  char c, d, e;  // e is to detect EOL as we check that it /isn't/ converted.
  int num_parsed = sscanf(version.c_str(), "%d_%d_%c%c%c", &a, &b, &c, &d, &e);
  return (num_parsed == 4);
}

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

const char kChromeNavigateExtraDataKey[] = "chrome_navigate";

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

uint8 GetQueryStringBasedExperiment(const GURL& url) {
  url::Parsed parsed;
  url::ParseStandardURL(url.spec().c_str(), url.spec().length(), &parsed);
  url::Component query = parsed.query;
  url::Component key, value;
  while (url::ExtractQueryKeyValue(url.spec().c_str(), &query, &key, &value)) {
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

bool IsNoSwapInExperiment(uint8 experiment_id) {
  // Currently, experiments 5 and 6 fall in this category.
  return experiment_id == 5 || experiment_id == 6;
}

bool IsControlGroupExperiment(uint8 experiment_id) {
  // Currently, experiments 7 and 8 fall in this category.
  return experiment_id == 7 || experiment_id == 8;
}

void GatherPagespeedData(const ResourceType::Type resource_type,
                         const GURL& request_url,
                         const net::HttpResponseHeaders* response_headers) {
  if (resource_type != ResourceType::MAIN_FRAME ||
      !request_url.SchemeIsHTTPOrHTTPS())
    return;

  // bucket 0 counts every response seen.
  UMA_HISTOGRAM_ENUMERATION(kPagespeedServerHistogram,
                            PAGESPEED_TOTAL_RESPONSES,
                            PAGESPEED_SERVER_MAXIMUM);
  if (!response_headers)
    return;

  void* iter = NULL;
  std::string name;
  std::string value;
  while (response_headers->EnumerateHeaderLines(&iter, &name, &value)) {
    if (name == kModPagespeedHeader) {
      // Bucket 1 counts occurences of the X-Mod-Pagespeed header.
      UMA_HISTOGRAM_ENUMERATION(kPagespeedServerHistogram,
                                PAGESPEED_MOD_PAGESPEED_SERVER,
                                PAGESPEED_SERVER_MAXIMUM);
      if (!value.empty()) {
        // If the header value is in the X-Mod-Pagespeed version number format
        // then increment the appropriate bucket, otherwise increment bucket 1,
        // which is the catch-all "unknown version number" bucket.
        int bucket = GetXModPagespeedBucketFromVersion(value);
        if (bucket > 0) {
          UMA_HISTOGRAM_SPARSE_SLOWLY(kPagespeedVersionHistogram, bucket);
        } else {
          UMA_HISTOGRAM_SPARSE_SLOWLY(kPagespeedVersionHistogram, 1);
        }
      }
      break;
    } else if (name == kPageSpeedHeader) {
      // X-Page-Speed header versions are either in the X-Mod-Pagespeed format,
      // indicating an nginx installation, or they're in the PageSpeed Service
      // format, indicating a PSS installation, or in some other format,
      // indicating an unknown installation [possibly IISpeed].
      if (!value.empty()) {
        int bucket = GetXModPagespeedBucketFromVersion(value);
        if (bucket > 0) {
          // Bucket 2 counts occurences of the X-Page-Speed header with a
          // value in the X-Mod-Pagespeed version number format. We also
          // count these responses in the version histogram.
          UMA_HISTOGRAM_ENUMERATION(kPagespeedServerHistogram,
                                    PAGESPEED_NGX_PAGESPEED_SERVER,
                                    PAGESPEED_SERVER_MAXIMUM);
          UMA_HISTOGRAM_SPARSE_SLOWLY(kPagespeedVersionHistogram, bucket);
        } else if (IsPageSpeedServiceVersionNumber(value)) {
          // Bucket 3 counts occurences of the X-Page-Speed header with a
          // value in the PageSpeed Service version number format.
          UMA_HISTOGRAM_ENUMERATION(kPagespeedServerHistogram,
                                    PAGESPEED_PAGESPEED_SERVICE_SERVER,
                                    PAGESPEED_SERVER_MAXIMUM);
        } else {
          // Bucket 4 counts occurences of all other values.
          UMA_HISTOGRAM_ENUMERATION(kPagespeedServerHistogram,
                                    PAGESPEED_UNKNOWN_SERVER,
                                    PAGESPEED_SERVER_MAXIMUM);
        }
      }
      break;
    }
  }
}

void URLRequestResponseStarted(net::URLRequest* request) {
  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request);
  GatherPagespeedData(info->GetResourceType(),
                      request->url(),
                      request->response_headers());
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
