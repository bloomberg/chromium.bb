// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/core/request_metadata.h"

#include "net/http/http_response_headers.h"

namespace image_fetcher {

namespace {

bool HttpHeadersEqual(const net::HttpResponseHeaders* lhs,
                      const net::HttpResponseHeaders* rhs) {
  if (!lhs && !rhs) {
    return true;  // Equal if both nullptr.
  }
  if (!lhs || !rhs) {
    return false;  // We cannot compare the content if one of them is nullptr.
  }
  return lhs->raw_headers() == rhs->raw_headers();
}

}  // namespace

RequestMetadata::RequestMetadata()
    : http_response_code(RESPONSE_CODE_INVALID),
      from_http_cache(false),
      http_response_headers(nullptr) {}

bool operator==(const RequestMetadata& lhs, const RequestMetadata& rhs) {
  return lhs.mime_type == rhs.mime_type &&
         lhs.http_response_code == rhs.http_response_code &&
         lhs.from_http_cache == rhs.from_http_cache &&
         HttpHeadersEqual(lhs.http_response_headers, rhs.http_response_headers);
}

bool operator!=(const RequestMetadata& lhs, const RequestMetadata& rhs) {
  return !(lhs == rhs);
}

}  // namespace image_fetcher
