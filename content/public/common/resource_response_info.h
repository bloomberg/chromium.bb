// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_RESOURCE_RESPONSE_INFO_H_
#define CONTENT_PUBLIC_COMMON_RESOURCE_RESPONSE_INFO_H_

#include <stdint.h>

#include <string>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/public/common/previews_state.h"
#include "content/public/common/resource_devtools_info.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_timing_info.h"
#include "net/cert/signed_certificate_timestamp_and_status.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/nqe/effective_connection_type.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerResponseType.h"
#include "url/gurl.h"

namespace content {

// NOTE: when modifying this structure, also update ResourceResponse::DeepCopy
// in resource_response.cc.
struct CONTENT_EXPORT ResourceResponseInfo {
  ResourceResponseInfo();
  ResourceResponseInfo(const ResourceResponseInfo& other);
  ~ResourceResponseInfo();

  // The time at which the request was made that resulted in this response.
  // For cached responses, this time could be "far" in the past.
  base::Time request_time;

  // The time at which the response headers were received.  For cached
  // responses, this time could be "far" in the past.
  base::Time response_time;

  // The response headers or NULL if the URL type does not support headers.
  scoped_refptr<net::HttpResponseHeaders> headers;

  // The mime type of the response.  This may be a derived value.
  std::string mime_type;

  // The character encoding of the response or none if not applicable to the
  // response's mime type.  This may be a derived value.
  std::string charset;

  // True if the resource was loaded in spite of certificate errors.
  bool has_major_certificate_errors;

  // Content length if available. -1 if not available
  int64_t content_length;

  // Length of the encoded data transferred over the network. In case there is
  // no data, contains -1.
  int64_t encoded_data_length;

  // Length of the response body data before decompression. -1 unless the body
  // has been read to the end.
  int64_t encoded_body_length;

  // The appcache this response was loaded from, or kAppCacheNoCacheId.
  int64_t appcache_id;

  // The manifest url of the appcache this response was loaded from.
  // Note: this value is only populated for main resource requests.
  GURL appcache_manifest_url;

  // Detailed timing information used by the WebTiming, HAR and Developer
  // Tools.  Includes socket ID and socket reuse information.
  net::LoadTimingInfo load_timing;

  // Actual request and response headers, as obtained from the network stack.
  // Only present if the renderer set report_raw_headers to true and had the
  // CanReadRawCookies permission.
  scoped_refptr<ResourceDevToolsInfo> devtools_info;

  // The path to a file that will contain the response body.  It may only
  // contain a portion of the response body at the time that the ResponseInfo
  // becomes available.
  base::FilePath download_file_path;

  // True if the response was delivered using SPDY.
  bool was_fetched_via_spdy;

  // True if the response was delivered after NPN is negotiated.
  bool was_alpn_negotiated;

  // True if response could use alternate protocol. However, browser will
  // ignore the alternate protocol when spdy is not enabled on browser side.
  bool was_alternate_protocol_available;

  // Information about the type of connection used to fetch this response.
  net::HttpResponseInfo::ConnectionInfo connection_info;

  // ALPN protocol negotiated with the server.
  std::string alpn_negotiated_protocol;

  // Remote address of the socket which fetched this resource.
  net::HostPortPair socket_address;

  // True if the response was fetched by a ServiceWorker.
  bool was_fetched_via_service_worker;

  // True if the response was fetched by a foreign fetch ServiceWorker;
  bool was_fetched_via_foreign_fetch;

  // True when the request whoes mode is |CORS| or |CORS-with-forced-preflight|
  // is sent to a ServiceWorker but FetchEvent.respondWith is not called. So the
  // renderer have to resend the request with skip service worker flag
  // considering the CORS preflight logic.
  bool was_fallback_required_by_service_worker;

  // The URL list of the response which was served by the ServiceWorker. See
  // ServiceWorkerResponseInfo::url_list_via_service_worker().
  std::vector<GURL> url_list_via_service_worker;

  // The type of the response which was fetched by the ServiceWorker.
  blink::WebServiceWorkerResponseType response_type_via_service_worker;

  // The time immediately before starting ServiceWorker. If the response is not
  // provided by the ServiceWorker, kept empty.
  // TODO(ksakamoto): Move this to net::LoadTimingInfo.
  base::TimeTicks service_worker_start_time;

  // The time immediately before dispatching fetch event in ServiceWorker.
  // If the response is not provided by the ServiceWorker, kept empty.
  // TODO(ksakamoto): Move this to net::LoadTimingInfo.
  base::TimeTicks service_worker_ready_time;

  // True when the response is served from the CacheStorage via the
  // ServiceWorker.
  bool is_in_cache_storage = false;

  // The cache name of the CacheStorage from where the response is served via
  // the ServiceWorker. Empty if the response isn't from the CacheStorage.
  std::string cache_storage_cache_name;

  // A bitmask of potentially several Previews optimizations that the resource
  // could have requested.
  PreviewsState previews_state;

  // Effective connection type when the resource was fetched. This is populated
  // only for responses that correspond to main frame requests.
  net::EffectiveConnectionType effective_connection_type;

  // DER-encoded X509Certificate certificate chain. Only present if the renderer
  // process set report_raw_headers to true.
  std::vector<std::string> certificate;

  // Bitmask of status info of the SSL certificate. See cert_status_flags.h for
  // values. Only present if the renderer process set report_raw_headers to
  // true.
  net::CertStatus cert_status;

  // Information about the SSL connection itself. See
  // ssl_connection_status_flags.h for values. The protocol version,
  // ciphersuite, and compression in use are encoded within. Only present if
  // the renderer process set report_raw_headers to true.
  int ssl_connection_status;

  // The key exchange group used by the SSL connection or zero if unknown or not
  // applicable. Only present if the renderer process set report_raw_headers to
  // true.
  uint16_t ssl_key_exchange_group;

  // List of Signed Certificate Timestamps (SCTs) and their corresponding
  // validation status. Only present if the renderer process set
  // report_raw_headers to true.
  net::SignedCertificateTimestampAndStatusList signed_certificate_timestamps;

  // In case this is a CORS response fetched by a ServiceWorker, this is the
  // set of headers that should be exposed.
  std::vector<std::string> cors_exposed_header_names;

  // True if service worker navigation preload was performed due to the request
  // for this response.
  bool did_service_worker_navigation_preload;

  // NOTE: When adding or changing fields here, also update
  // ResourceResponse::DeepCopy in resource_response.cc.
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_RESOURCE_RESPONSE_INFO_H_
