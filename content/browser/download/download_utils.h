// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_UTILS_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_UTILS_H_

#include "base/optional.h"
#include "components/download/downloader/in_progress/download_source.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/download_source.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "net/http/http_response_headers.h"

namespace download {
struct DownloadEntry;
}  // namespace download

namespace net {
class URLRequest;
}  // namespace net

namespace network {
struct ResourceRequest;
}

namespace content {

class BrowserContext;
class DownloadUrlParameters;
struct DownloadCreateInfo;
struct DownloadSaveInfo;

// Handle the url request completion status and return the interrupt reasons.
// |cert_status| is ignored if error_code is not net::ERR_ABORTED.
DownloadInterruptReason CONTENT_EXPORT HandleRequestCompletionStatus(
    net::Error error_code, bool has_strong_validators,
    net::CertStatus cert_status, DownloadInterruptReason abort_reason);

// Create a ResourceRequest from |params|.
std::unique_ptr<network::ResourceRequest> CONTENT_EXPORT
CreateResourceRequest(DownloadUrlParameters* params);

// Create a URLRequest from |params|.
std::unique_ptr<net::URLRequest> CONTENT_EXPORT CreateURLRequestOnIOThread(
    DownloadUrlParameters* params);

// Parse the HTTP server response code.
// If |fetch_error_body| is true, most of HTTP response codes will be accepted
// as successful response.
DownloadInterruptReason CONTENT_EXPORT
HandleSuccessfulServerResponse(const net::HttpResponseHeaders& http_headers,
                               DownloadSaveInfo* save_info,
                               bool fetch_error_body);

// Parse response headers and update |create_info| accordingly.
CONTENT_EXPORT void HandleResponseHeaders(
    const net::HttpResponseHeaders* headers,
    DownloadCreateInfo* create_info);

// Converts content::DownloadSource to download::DownloadSource.
CONTENT_EXPORT download::DownloadSource ToDownloadSource(
    content::DownloadSource download_source);

// Converts download::DownloadSource to content::DownloadSource.
CONTENT_EXPORT content::DownloadSource ToDownloadSource(
    download::DownloadSource download_source);

// Get the entry based on |guid| from in progress cache.
CONTENT_EXPORT base::Optional<download::DownloadEntry> GetInProgressEntry(
    const std::string& guid,
    BrowserContext* browser_context);

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_UTILS_H_
