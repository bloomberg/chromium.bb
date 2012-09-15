// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/resource_dispatcher_host_delegate.h"

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

void ResourceDispatcherHostDelegate::HandleExternalProtocol(const GURL& url,
                                                            int child_id,
                                                            int route_id) {
}

bool ResourceDispatcherHostDelegate::ShouldForceDownloadResource(
    const GURL& url,
    const std::string& mime_type) {
  return false;
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
