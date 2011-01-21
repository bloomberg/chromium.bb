// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/security_filter_peer.h"

#include "grit/generated_resources.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "ui/base/l10n/l10n_util.h"

SecurityFilterPeer::SecurityFilterPeer(
    webkit_glue::ResourceLoaderBridge* resource_loader_bridge,
    webkit_glue::ResourceLoaderBridge::Peer* peer)
    : original_peer_(peer),
      resource_loader_bridge_(resource_loader_bridge) {
}

SecurityFilterPeer::~SecurityFilterPeer() {
}

// static
SecurityFilterPeer*
    SecurityFilterPeer::CreateSecurityFilterPeerForDeniedRequest(
    ResourceType::Type resource_type,
    webkit_glue::ResourceLoaderBridge::Peer* peer,
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
    case net::ERR_INSECURE_RESPONSE:
      if (ResourceType::IsFrame(resource_type))
        return CreateSecurityFilterPeerForFrame(peer, os_error);
      // Any other content is entirely filtered-out.
      return new ReplaceContentPeer(NULL, peer, std::string(), std::string());
    default:
      // For other errors, we use our normal error handling.
      return NULL;
  }
}

// static
SecurityFilterPeer* SecurityFilterPeer::CreateSecurityFilterPeerForFrame(
    webkit_glue::ResourceLoaderBridge::Peer* peer, int os_error) {
  // TODO(jcampan): use a different message when getting a phishing/malware
  // error.
  std::string html = base::StringPrintf(
      "<html><meta charset='UTF-8'>"
      "<body style='background-color:#990000;color:white;'>"
      "%s</body></html>",
      l10n_util::GetStringUTF8(IDS_UNSAFE_FRAME_MESSAGE).c_str());
  return new ReplaceContentPeer(NULL, peer, "text/html", html);
}

void SecurityFilterPeer::OnUploadProgress(uint64 position, uint64 size) {
  original_peer_->OnUploadProgress(position, size);
}

bool SecurityFilterPeer::OnReceivedRedirect(
    const GURL& new_url,
    const webkit_glue::ResourceResponseInfo& info,
    bool* has_new_first_party_for_cookies,
    GURL* new_first_party_for_cookies) {
  NOTREACHED();
  return false;
}

void SecurityFilterPeer::OnReceivedResponse(
    const webkit_glue::ResourceResponseInfo& info,
    bool content_filtered) {
  NOTREACHED();
}

void SecurityFilterPeer::OnReceivedData(const char* data, int len) {
  NOTREACHED();
}

void SecurityFilterPeer::OnCompletedRequest(const net::URLRequestStatus& status,
                                            const std::string& security_info,
                                            const base::Time& completion_time) {
  NOTREACHED();
}

// static
void ProcessResponseInfo(
    const webkit_glue::ResourceResponseInfo& info_in,
    webkit_glue::ResourceResponseInfo* info_out,
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

BufferedPeer::BufferedPeer(
    webkit_glue::ResourceLoaderBridge* resource_loader_bridge,
    webkit_glue::ResourceLoaderBridge::Peer* peer,
    const std::string& mime_type)
    : SecurityFilterPeer(resource_loader_bridge, peer),
      mime_type_(mime_type) {
}

BufferedPeer::~BufferedPeer() {
}

void BufferedPeer::OnReceivedResponse(
    const webkit_glue::ResourceResponseInfo& info,
    bool response_filtered) {
  ProcessResponseInfo(info, &response_info_, mime_type_);
}

void BufferedPeer::OnReceivedData(const char* data, int len) {
  data_.append(data, len);
}

void BufferedPeer::OnCompletedRequest(const net::URLRequestStatus& status,
                                      const std::string& security_info,
                                      const base::Time& completion_time) {
  // Make sure we delete ourselves at the end of this call.
  scoped_ptr<BufferedPeer> this_deleter(this);

  // Give sub-classes a chance at altering the data.
  if (status.status() != net::URLRequestStatus::SUCCESS || !DataReady()) {
    // Pretend we failed to load the resource.
    original_peer_->OnReceivedResponse(response_info_, true);
    net::URLRequestStatus status(net::URLRequestStatus::CANCELED,
                                 net::ERR_ABORTED);
    original_peer_->OnCompletedRequest(status, security_info, completion_time);
    return;
  }

  original_peer_->OnReceivedResponse(response_info_, true);
  if (!data_.empty())
    original_peer_->OnReceivedData(data_.data(),
                                   static_cast<int>(data_.size()));
  original_peer_->OnCompletedRequest(status, security_info, completion_time);
}

////////////////////////////////////////////////////////////////////////////////
// ReplaceContentPeer

ReplaceContentPeer::ReplaceContentPeer(
    webkit_glue::ResourceLoaderBridge* resource_loader_bridge,
    webkit_glue::ResourceLoaderBridge::Peer* peer,
    const std::string& mime_type,
    const std::string& data)
    : SecurityFilterPeer(resource_loader_bridge, peer),
      mime_type_(mime_type),
      data_(data) {
}

ReplaceContentPeer::~ReplaceContentPeer() {
}

void ReplaceContentPeer::OnReceivedResponse(
    const webkit_glue::ResourceResponseInfo& info,
    bool content_filtered) {
  // Ignore this, we'll serve some alternate content in OnCompletedRequest.
}

void ReplaceContentPeer::OnReceivedData(const char* data, int len) {
  // Ignore this, we'll serve some alternate content in OnCompletedRequest.
}

void ReplaceContentPeer::OnCompletedRequest(
    const net::URLRequestStatus& status,
    const std::string& security_info,
    const base::Time& completion_time) {
  webkit_glue::ResourceResponseInfo info;
  ProcessResponseInfo(info, &info, mime_type_);
  info.security_info = security_info;
  info.content_length = static_cast<int>(data_.size());
  original_peer_->OnReceivedResponse(info, true);
  if (!data_.empty())
    original_peer_->OnReceivedData(data_.data(),
                                   static_cast<int>(data_.size()));
  original_peer_->OnCompletedRequest(net::URLRequestStatus(),
                                     security_info,
                                     completion_time);

  // The request processing is complete, we must delete ourselves.
  delete this;
}
