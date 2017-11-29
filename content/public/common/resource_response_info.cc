// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/resource_response_info.h"

#include "content/public/common/appcache_info.h"
#include "content/public/common/previews_state.h"
#include "net/http/http_response_headers.h"

namespace content {

ResourceResponseInfo::ResourceResponseInfo()
    : has_major_certificate_errors(false),
      is_legacy_symantec_cert(false),
      content_length(-1),
      encoded_data_length(-1),
      encoded_body_length(-1),
      appcache_id(kAppCacheNoCacheId),
      was_fetched_via_spdy(false),
      was_alpn_negotiated(false),
      was_alternate_protocol_available(false),
      connection_info(net::HttpResponseInfo::CONNECTION_INFO_UNKNOWN),
      was_fetched_via_service_worker(false),
      was_fallback_required_by_service_worker(false),
      response_type_via_service_worker(
          network::mojom::FetchResponseType::kDefault),
      previews_state(PREVIEWS_OFF),
      effective_connection_type(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN),
      cert_status(0),
      ssl_connection_status(0),
      ssl_key_exchange_group(0),
      did_service_worker_navigation_preload(false) {}

ResourceResponseInfo::ResourceResponseInfo(const ResourceResponseInfo& other) =
    default;

ResourceResponseInfo::~ResourceResponseInfo() {
}

}  // namespace content
