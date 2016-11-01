// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mod_pagespeed/mod_pagespeed_metrics.h"

#include <stdio.h>

#include <string>

#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "net/http/http_response_headers.h"
#include "url/gurl.h"

namespace mod_pagespeed {

namespace {

const char kModPagespeedHeader[] = "X-Mod-Pagespeed";
const char kPageSpeedHeader[] = "X-Page-Speed";

// For historical reasons, mod_pagespeed usage metrics are named with a
// prerender prefix.
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

// Parse the PageSpeed version number and encodes it in buckets 2 through 99: if
// it is in the format MAJOR.MINOR.BRANCH.POINT-COMMIT the bucket will be 2 + 2
// * (max(BRANCH, 10) - 10) + (POINT > 1 ? 1 : 0); otherwise the bucket is
// zero.
int GetXModPagespeedBucketFromVersion(const std::string& version) {
  int unused_major, unused_minor, branch, point, unused_commit;
  int num_parsed = sscanf(version.c_str(), "%d.%d.%d.%d-%d", &unused_major,
                          &unused_minor, &branch, &point, &unused_commit);
  int output = 0;
  if (num_parsed >= 4) {
    output = 2;
    if (branch > 10)
      output += 2 * (branch - 10);
    if (point > 1)
      output++;
    if (output < 2 || output > 99)
      output = 0;
  }
  return output;
}

// Parse the X-Page-Speed header value and determine whether it is in the
// PageSpeed Service format, namely MAJOR_MINOR_AB were MAJOR_MINOR is a version
// number and AB is an encoded 2-character value.
bool IsPageSpeedServiceVersionNumber(const std::string& version) {
  int major, minor;
  // is_eol is to detect EOL as we check that it /isn't/ converted.
  char a, b, is_eol;
  int num_parsed =
      sscanf(version.c_str(), "%d_%d_%c%c%c", &major, &minor, &a, &b, &is_eol);
  return (num_parsed == 4);
}

}  // namespace

void RecordMetrics(const content::ResourceType resource_type,
                   const GURL& request_url,
                   const net::HttpResponseHeaders* response_headers) {
  if (resource_type != content::RESOURCE_TYPE_MAIN_FRAME ||
      !request_url.SchemeIsHTTPOrHTTPS()) {
    return;
  }

  // Bucket 0 counts every response seen.
  UMA_HISTOGRAM_ENUMERATION(kPagespeedServerHistogram,
                            PAGESPEED_TOTAL_RESPONSES,
                            PAGESPEED_SERVER_MAXIMUM);
  if (!response_headers)
    return;

  size_t iter = 0;
  std::string name;
  std::string value;
  while (response_headers->EnumerateHeaderLines(&iter, &name, &value)) {
    if (name == kModPagespeedHeader) {
      // Bucket 1 counts occurrences of the X-Mod-Pagespeed header.
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

}  // namespace mod_pagespeed
