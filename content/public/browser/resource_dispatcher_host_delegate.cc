// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/resource_dispatcher_host_delegate.h"

#include "content/public/browser/stream_handle.h"

namespace content {

bool ResourceDispatcherHostDelegate::ShouldBeginRequest(
    int child_id,
    int route_id,
    const std::string& method,
    const GURL& url,
    ResourceType::Type resource_type,
    ResourceContext* resource_context,
    const Referrer& referrer) {
  return true;
}

void ResourceDispatcherHostDelegate::RequestBeginning(
    net::URLRequest* request,
    ResourceContext* resource_context,
    appcache::AppCacheService* appcache_service,
    ResourceType::Type resource_type,
    int child_id,
    int route_id,
    bool is_continuation_of_transferred_request,
    ScopedVector<ResourceThrottle>* throttles) {
}

void ResourceDispatcherHostDelegate::DownloadStarting(
    net::URLRequest* request,
    ResourceContext* resource_context,
    int child_id,
    int route_id,
    int request_id,
    bool is_content_initiated,
    bool must_download,
    ScopedVector<ResourceThrottle>* throttles) {
}

bool ResourceDispatcherHostDelegate::AcceptSSLClientCertificateRequest(
    net::URLRequest* request,
    net::SSLCertRequestInfo* cert_request_info) {
  return false;
}

bool ResourceDispatcherHostDelegate::AcceptAuthRequest(
    net::URLRequest* request,
    net::AuthChallengeInfo* auth_info) {
  return false;
}

ResourceDispatcherHostLoginDelegate*
    ResourceDispatcherHostDelegate::CreateLoginDelegate(
        net::AuthChallengeInfo* auth_info,
        net::URLRequest* request) {
  return NULL;
}

bool ResourceDispatcherHostDelegate::HandleExternalProtocol(const GURL& url,
                                                            int child_id,
                                                            int route_id) {
  return true;
}

bool ResourceDispatcherHostDelegate::ShouldForceDownloadResource(
    const GURL& url,
    const std::string& mime_type) {
  return false;
}

bool ResourceDispatcherHostDelegate::ShouldInterceptResourceAsStream(
    content::ResourceContext* resource_context,
    const GURL& url,
    const std::string& mime_type,
    GURL* security_origin,
    std::string* target_id) {
  return false;
}

void ResourceDispatcherHostDelegate::OnStreamCreated(
    content::ResourceContext* resource_context,
    int render_process_id,
    int render_view_id,
    const std::string& target_id,
    scoped_ptr<StreamHandle> stream) {
}

void ResourceDispatcherHostDelegate::OnResponseStarted(
    net::URLRequest* request,
    ResourceContext* resource_context,
    ResourceResponse* response,
    IPC::Sender* sender) {
}

void ResourceDispatcherHostDelegate::OnRequestRedirected(
    const GURL& redirect_url,
    net::URLRequest* request,
    ResourceContext* resource_context,
    ResourceResponse* response) {
}

ResourceDispatcherHostDelegate::ResourceDispatcherHostDelegate() {
}

ResourceDispatcherHostDelegate::~ResourceDispatcherHostDelegate() {
}

}  // namespace content
