// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_loader_helpers.h"

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/browser/service_worker/service_worker_consts.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"

namespace content {

namespace service_worker_loader_helpers {

std::unique_ptr<net::HttpResponseInfo> CreateHttpResponseInfoAndCheckHeaders(
    const network::ResourceResponseHead& response_head,
    network::URLLoaderCompletionStatus* out_completion_status,
    std::string* out_error_message) {
  net::Error error_code = net::OK;
  std::string error_message;
  auto response_info = std::make_unique<net::HttpResponseInfo>();
  response_info->headers = response_head.headers;
  if (response_head.ssl_info.has_value())
    response_info->ssl_info = *response_head.ssl_info;
  response_info->was_fetched_via_spdy = response_head.was_fetched_via_spdy;
  response_info->was_alpn_negotiated = response_head.was_alpn_negotiated;
  response_info->alpn_negotiated_protocol =
      response_head.alpn_negotiated_protocol;
  response_info->connection_info = response_head.connection_info;
  response_info->remote_endpoint = response_head.remote_endpoint;
  response_info->response_time = response_head.response_time;

  if (response_head.headers->response_code() / 100 != 2) {
    // Non-2XX HTTP status code is handled as an error.
    error_code = net::ERR_INVALID_RESPONSE;
    error_message = base::StringPrintf(
        ServiceWorkerConsts::kServiceWorkerBadHTTPResponseError,
        response_head.headers->response_code());
  } else if (net::IsCertStatusError(response_head.cert_status) &&
             !base::CommandLine::ForCurrentProcess()->HasSwitch(
                 switches::kIgnoreCertificateErrors)) {
    error_code = static_cast<net::Error>(
        net::MapCertStatusToNetError(response_head.cert_status));
    error_message = ServiceWorkerConsts::kServiceWorkerSSLError;
  } else if (!blink::IsSupportedJavascriptMimeType(response_head.mime_type)) {
    error_code = net::ERR_INSECURE_RESPONSE;
    error_message = response_head.mime_type.empty()
                        ? ServiceWorkerConsts::kServiceWorkerNoMIMEError
                        : base::StringPrintf(
                              ServiceWorkerConsts::kServiceWorkerBadMIMEError,
                              response_head.mime_type.c_str());
  }
  if (out_completion_status)
    *out_completion_status = network::URLLoaderCompletionStatus(error_code);

  if (out_error_message)
    *out_error_message = error_message;

  return response_info;
}

blink::ServiceWorkerStatusCode MapNetErrorToServiceWorkerStatus(
    net::Error error_code) {
  switch (error_code) {
    case net::OK:
      return blink::ServiceWorkerStatusCode::kOk;
    case net::ERR_INSECURE_RESPONSE:
      return blink::ServiceWorkerStatusCode::kErrorSecurity;
    default:
      return blink::ServiceWorkerStatusCode::kErrorNetwork;
  }
}

bool ShouldBypassCacheDueToUpdateViaCache(
    bool is_main_script,
    blink::mojom::ServiceWorkerUpdateViaCache cache_mode) {
  switch (cache_mode) {
    case blink::mojom::ServiceWorkerUpdateViaCache::kImports:
      return is_main_script;
    case blink::mojom::ServiceWorkerUpdateViaCache::kNone:
      return true;
    case blink::mojom::ServiceWorkerUpdateViaCache::kAll:
      return false;
  }
  NOTREACHED() << static_cast<int>(cache_mode);
  return false;
}

bool ShouldValidateBrowserCacheForScript(
    bool is_main_script,
    bool force_bypass_cache,
    blink::mojom::ServiceWorkerUpdateViaCache cache_mode,
    base::TimeDelta time_since_last_check) {
  return (ShouldBypassCacheDueToUpdateViaCache(is_main_script, cache_mode) ||
          time_since_last_check >
              ServiceWorkerConsts::kServiceWorkerScriptMaxCacheAge ||
          force_bypass_cache);
}

}  // namespace service_worker_loader_helpers

}  // namespace content
