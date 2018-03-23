// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_UTILS_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_UTILS_H_

#include "components/download/public/common/download_export.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "net/http/http_response_headers.h"

namespace net {
class HttpRequestHeaders;
}

namespace network {
struct ResourceRequest;
}

namespace download {
struct DownloadCreateInfo;
struct DownloadSaveInfo;
class DownloadUrlParameters;

// Handle the url request completion status and return the interrupt reasons.
// |cert_status| is ignored if error_code is not net::ERR_ABORTED.
COMPONENTS_DOWNLOAD_EXPORT DownloadInterruptReason
HandleRequestCompletionStatus(net::Error error_code,
                              bool has_strong_validators,
                              net::CertStatus cert_status,
                              DownloadInterruptReason abort_reason);

// Parse the HTTP server response code.
// If |fetch_error_body| is true, most of HTTP response codes will be accepted
// as successful response.
COMPONENTS_DOWNLOAD_EXPORT DownloadInterruptReason
HandleSuccessfulServerResponse(const net::HttpResponseHeaders& http_headers,
                               DownloadSaveInfo* save_info,
                               bool fetch_error_body);

// Parse response headers and update |create_info| accordingly.
COMPONENTS_DOWNLOAD_EXPORT void HandleResponseHeaders(
    const net::HttpResponseHeaders* headers,
    DownloadCreateInfo* create_info);

// Create a ResourceRequest from |params|.
COMPONENTS_DOWNLOAD_EXPORT std::unique_ptr<network::ResourceRequest>
CreateResourceRequest(DownloadUrlParameters* params);

// Gets LoadFlags from |params|.
COMPONENTS_DOWNLOAD_EXPORT int GetLoadFlags(DownloadUrlParameters* params,
                                            bool has_upload_data);

// Gets addtional request headers from |params|.
COMPONENTS_DOWNLOAD_EXPORT std::unique_ptr<net::HttpRequestHeaders>
GetAdditionalRequestHeaders(DownloadUrlParameters* params);

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_UTILS_H_
