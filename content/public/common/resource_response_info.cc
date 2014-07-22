// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/resource_response_info.h"

#include "content/public/common/appcache_info.h"
#include "net/http/http_response_headers.h"

namespace content {

ResourceResponseInfo::ResourceResponseInfo()
    : content_length(-1),
      encoded_data_length(-1),
      appcache_id(kAppCacheNoCacheId),
      was_fetched_via_spdy(false),
      was_npn_negotiated(false),
      was_alternate_protocol_available(false),
      connection_info(net::HttpResponseInfo::CONNECTION_INFO_UNKNOWN),
      was_fetched_via_proxy(false),
      was_fetched_via_service_worker(false) {
}

ResourceResponseInfo::~ResourceResponseInfo() {
}

}  // namespace content
