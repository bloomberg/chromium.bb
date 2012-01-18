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
    const ResourceContext& resource_context,
    const Referrer& referrer) {
  return true;
}

ResourceHandler* ResourceDispatcherHostDelegate::RequestBeginning(
    ResourceHandler* handler,
    net::URLRequest* request,
    const ResourceContext& resource_context,
    bool is_subresource,
    int child_id,
    int route_id,
    bool is_continuation_of_transferred_request) {
  return handler;
}

ResourceHandler* ResourceDispatcherHostDelegate::DownloadStarting(
    ResourceHandler* handler,
    const ResourceContext& resource_context,
    net::URLRequest* request,
    int child_id,
    int route_id,
    int request_id,
    bool is_new_request) {
  return handler;
}

bool ResourceDispatcherHostDelegate::ShouldDeferStart(
    net::URLRequest* request,
    const ResourceContext& resource_context) {
  return false;
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
    ResourceResponse* response,
    ResourceMessageFilter* filter) {
}

void ResourceDispatcherHostDelegate::OnRequestRedirected(
    net::URLRequest* request,
    ResourceResponse* response,
    ResourceMessageFilter* filter) {
}

ResourceDispatcherHostDelegate::ResourceDispatcherHostDelegate() {
}

ResourceDispatcherHostDelegate::~ResourceDispatcherHostDelegate() {
}

}  // namespace content
