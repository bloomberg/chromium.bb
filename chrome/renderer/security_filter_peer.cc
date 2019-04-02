// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/security_filter_peer.h"

#include <memory>
#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/url_loader_completion_status.h"

SecurityFilterPeer::SecurityFilterPeer(
    std::unique_ptr<content::RequestPeer> peer)
    : original_peer_(std::move(peer)) {}
SecurityFilterPeer::~SecurityFilterPeer() {}

// static
std::unique_ptr<content::RequestPeer>
SecurityFilterPeer::CreateSecurityFilterPeerForDeniedRequest(
    std::unique_ptr<content::RequestPeer> peer,
    int os_error) {
  // Create a filter for SSL and CERT errors.
  switch (os_error) {
    case net::ERR_SSL_PROTOCOL_ERROR:
    case net::ERR_CERT_COMMON_NAME_INVALID:
    case net::ERR_CERT_DATE_INVALID:
    case net::ERR_CERT_AUTHORITY_INVALID:
    case net::ERR_CERT_CONTAINS_ERRORS:
    case net::ERR_CERT_NO_REVOCATION_MECHANISM:
    case net::ERR_CERT_UNABLE_TO_CHECK_REVOCATION:
    case net::ERR_CERT_REVOKED:
    case net::ERR_CERT_INVALID:
    case net::ERR_CERT_WEAK_SIGNATURE_ALGORITHM:
    case net::ERR_CERT_WEAK_KEY:
    case net::ERR_CERT_NAME_CONSTRAINT_VIOLATION:
    case net::ERR_INSECURE_RESPONSE:
    case net::ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN:
      return base::WrapUnique(new SecurityFilterPeer(std::move(peer)));
    default:
      // For other errors, we use our normal error handling.
      return peer;
  }
}

void SecurityFilterPeer::OnUploadProgress(uint64_t position, uint64_t size) {
  NOTREACHED();
}

bool SecurityFilterPeer::OnReceivedRedirect(
    const net::RedirectInfo& redirect_info,
    const network::ResourceResponseInfo& info) {
  NOTREACHED();
  return false;
}

void SecurityFilterPeer::OnReceivedResponse(
    const network::ResourceResponseInfo& info) {
  NOTREACHED();
}

void SecurityFilterPeer::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  NOTREACHED();
}

void SecurityFilterPeer::OnReceivedData(std::unique_ptr<ReceivedData> data) {
  NOTREACHED();
}

void SecurityFilterPeer::OnTransferSizeUpdated(int transfer_size_diff) {
  NOTREACHED();
}

void SecurityFilterPeer::OnCompletedRequest(
    const network::URLLoaderCompletionStatus& status) {
  network::ResourceResponseInfo info;
  info.headers = CreateHeaders();
  info.content_length = 0;
  original_peer_->OnReceivedResponse(info);
  network::URLLoaderCompletionStatus ok_status(status);
  ok_status.error_code = net::OK;
  original_peer_->OnCompletedRequest(ok_status);
}

scoped_refptr<base::TaskRunner> SecurityFilterPeer::GetTaskRunner() {
  return original_peer_->GetTaskRunner();
}

scoped_refptr<net::HttpResponseHeaders> SecurityFilterPeer::CreateHeaders() {
  std::string raw_headers;
  raw_headers.append("HTTP/1.1 200 OK");
  raw_headers.push_back('\0');
  // Don't cache the data we are serving, it is not the real data for that URL
  // (if the filtered resource were to make it into the WebCore cache, then the
  // same URL loaded in a safe scenario would still return the filtered
  // resource).
  raw_headers.append("cache-control: no-cache");
  raw_headers.push_back('\0');
  raw_headers.push_back('\0');
  return base::MakeRefCounted<net::HttpResponseHeaders>(raw_headers);
}
