// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/security_filter_peer.h"

#include <string>
#include <utility>

#include "base/memory/scoped_ptr.h"
#include "base/strings/stringprintf.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/child/fixed_received_data.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "ui/base/l10n/l10n_util.h"

SecurityFilterPeer::SecurityFilterPeer(scoped_ptr<content::RequestPeer> peer)
    : original_peer_(std::move(peer)) {}

SecurityFilterPeer::~SecurityFilterPeer() {
}

// static
scoped_ptr<content::RequestPeer>
SecurityFilterPeer::CreateSecurityFilterPeerForDeniedRequest(
    content::ResourceType resource_type,
    scoped_ptr<content::RequestPeer> peer,
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
      if (content::IsResourceTypeFrame(resource_type))
        return CreateSecurityFilterPeerForFrame(std::move(peer), os_error);
      // Any other content is entirely filtered-out.
      return make_scoped_ptr(new ReplaceContentPeer(
          std::move(peer), std::string(), std::string()));
    default:
      // For other errors, we use our normal error handling.
      return peer;
  }
}

// static
scoped_ptr<content::RequestPeer>
SecurityFilterPeer::CreateSecurityFilterPeerForFrame(
    scoped_ptr<content::RequestPeer> peer,
    int os_error) {
  // TODO(jcampan): use a different message when getting a phishing/malware
  // error.
  std::string html = base::StringPrintf(
      "<html><meta charset='UTF-8'>"
      "<body style='background-color:#990000;color:white;'>"
      "%s</body></html>",
      l10n_util::GetStringUTF8(IDS_UNSAFE_FRAME_MESSAGE).c_str());
  return make_scoped_ptr(
      new ReplaceContentPeer(std::move(peer), "text/html", html));
}

void SecurityFilterPeer::OnUploadProgress(uint64_t position, uint64_t size) {
  original_peer_->OnUploadProgress(position, size);
}

bool SecurityFilterPeer::OnReceivedRedirect(
    const net::RedirectInfo& redirect_info,
    const content::ResourceResponseInfo& info) {
  NOTREACHED();
  return false;
}

// static
void ProcessResponseInfo(const content::ResourceResponseInfo& info_in,
                         content::ResourceResponseInfo* info_out,
                         const std::string& mime_type) {
  DCHECK(info_out);
  *info_out = info_in;
  info_out->mime_type = mime_type;
  // Let's create our own HTTP headers.
  std::string raw_headers;
  raw_headers.append("HTTP/1.1 200 OK");
  raw_headers.push_back('\0');
  // Don't cache the data we are serving, it is not the real data for that URL
  // (if the filtered resource were to make it into the WebCore cache, then the
  // same URL loaded in a safe scenario would still return the filtered
  // resource).
  raw_headers.append("cache-control: no-cache");
  raw_headers.push_back('\0');
  if (!mime_type.empty()) {
    raw_headers.append("content-type: ");
    raw_headers.append(mime_type);
    raw_headers.push_back('\0');
  }
  raw_headers.push_back('\0');
  net::HttpResponseHeaders* new_headers =
      new net::HttpResponseHeaders(raw_headers);
  info_out->headers = new_headers;
}

////////////////////////////////////////////////////////////////////////////////
// BufferedPeer

BufferedPeer::BufferedPeer(scoped_ptr<content::RequestPeer> peer,
                           const std::string& mime_type)
    : SecurityFilterPeer(std::move(peer)), mime_type_(mime_type) {}

BufferedPeer::~BufferedPeer() {
}

void BufferedPeer::OnReceivedResponse(
    const content::ResourceResponseInfo& info) {
  ProcessResponseInfo(info, &response_info_, mime_type_);
}

void BufferedPeer::OnReceivedData(scoped_ptr<ReceivedData> data) {
  data_.append(data->payload(), data->length());
}

void BufferedPeer::OnCompletedRequest(int error_code,
                                      bool was_ignored_by_handler,
                                      bool stale_copy_in_cache,
                                      const std::string& security_info,
                                      const base::TimeTicks& completion_time,
                                      int64_t total_transfer_size) {
  // Give sub-classes a chance at altering the data.
  if (error_code != net::OK || !DataReady()) {
    // Pretend we failed to load the resource.
    original_peer_->OnReceivedResponse(response_info_);
    original_peer_->OnCompletedRequest(net::ERR_ABORTED, false,
                                       stale_copy_in_cache, security_info,
                                       completion_time, total_transfer_size);
    return;
  }

  original_peer_->OnReceivedResponse(response_info_);
  if (!data_.empty()) {
    original_peer_->OnReceivedData(make_scoped_ptr(
        new content::FixedReceivedData(data_.data(), data_.size(), -1)));
  }
  original_peer_->OnCompletedRequest(error_code, was_ignored_by_handler,
                                     stale_copy_in_cache, security_info,
                                     completion_time, total_transfer_size);
}

////////////////////////////////////////////////////////////////////////////////
// ReplaceContentPeer

ReplaceContentPeer::ReplaceContentPeer(scoped_ptr<content::RequestPeer> peer,
                                       const std::string& mime_type,
                                       const std::string& data)
    : SecurityFilterPeer(std::move(peer)), mime_type_(mime_type), data_(data) {}

ReplaceContentPeer::~ReplaceContentPeer() {
}

void ReplaceContentPeer::OnReceivedResponse(
    const content::ResourceResponseInfo& info) {
  // Ignore this, we'll serve some alternate content in OnCompletedRequest.
}

void ReplaceContentPeer::OnReceivedData(scoped_ptr<ReceivedData> data) {
  // Ignore this, we'll serve some alternate content in OnCompletedRequest.
}

void ReplaceContentPeer::OnCompletedRequest(
    int error_code,
    bool was_ignored_by_handler,
    bool stale_copy_in_cache,
    const std::string& security_info,
    const base::TimeTicks& completion_time,
    int64_t total_transfer_size) {
  content::ResourceResponseInfo info;
  ProcessResponseInfo(info, &info, mime_type_);
  info.security_info = security_info;
  info.content_length = static_cast<int>(data_.size());
  original_peer_->OnReceivedResponse(info);
  if (!data_.empty()) {
    original_peer_->OnReceivedData(make_scoped_ptr(
        new content::FixedReceivedData(data_.data(), data_.size(), -1)));
  }
  original_peer_->OnCompletedRequest(net::OK, false, stale_copy_in_cache,
                                     security_info, completion_time,
                                     total_transfer_size);
}
