// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/resource_dispatcher_host_delegate.h"

ResourceDispatcherHostDelegate::ResourceDispatcherHostDelegate() {
}

ResourceDispatcherHostDelegate::~ResourceDispatcherHostDelegate() {
}

bool ResourceDispatcherHostDelegate::ShouldBeginRequest(
    int child_id, int route_id,
    const ResourceHostMsg_Request& request_data,
    const content::ResourceContext& resource_context,
    const GURL& referrer) {
  return true;
}

ResourceHandler* ResourceDispatcherHostDelegate::RequestBeginning(
    ResourceHandler* handler,
    net::URLRequest* request,
    bool is_subresource,
    int child_id,
    int route_id) {
  return handler;
}

ResourceHandler* ResourceDispatcherHostDelegate::DownloadStarting(
    ResourceHandler* handler,
    int child_id,
    int route_id) {
  return handler;
}

bool ResourceDispatcherHostDelegate::ShouldDeferStart(
    net::URLRequest* request,
    const content::ResourceContext& resource_context) {
  return false;
}

bool ResourceDispatcherHostDelegate::AcceptSSLClientCertificateRequest(
    net::URLRequest* request,
    net::SSLCertRequestInfo* cert_request_info) {
  return false;
}

bool ResourceDispatcherHostDelegate::AcceptAuthRequest(
    net::URLRequest* request, net::AuthChallengeInfo* auth_info) {
  return false;
}

ResourceDispatcherHostLoginDelegate*
    ResourceDispatcherHostDelegate::CreateLoginDelegate(
  net::AuthChallengeInfo* auth_info, net::URLRequest* request) {
  return NULL;
}

void ResourceDispatcherHostDelegate::HandleExternalProtocol(const GURL& url,
                                                            int child_id,
                                                            int route_id) {
}

bool ResourceDispatcherHostDelegate::ShouldForceDownloadResource(
    const GURL& url, const std::string& mime_type) {
  return false;
}
