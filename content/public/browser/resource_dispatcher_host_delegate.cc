// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/resource_dispatcher_host_delegate.h"

#include "content/public/browser/navigation_data.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/stream_info.h"
#include "net/ssl/client_cert_store.h"

namespace content {

bool ResourceDispatcherHostDelegate::ShouldBeginRequest(
    const std::string& method,
    const GURL& url,
    ResourceType resource_type,
    ResourceContext* resource_context) {
  return true;
}

void ResourceDispatcherHostDelegate::RequestBeginning(
    net::URLRequest* request,
    ResourceContext* resource_context,
    AppCacheService* appcache_service,
    ResourceType resource_type,
    std::vector<std::unique_ptr<ResourceThrottle>>* throttles) {}

void ResourceDispatcherHostDelegate::DownloadStarting(
    net::URLRequest* request,
    ResourceContext* resource_context,
    bool is_content_initiated,
    bool must_download,
    bool is_new_request,
    std::vector<std::unique_ptr<ResourceThrottle>>* throttles) {}

ResourceDispatcherHostLoginDelegate*
    ResourceDispatcherHostDelegate::CreateLoginDelegate(
        net::AuthChallengeInfo* auth_info,
        net::URLRequest* request) {
  return nullptr;
}

bool ResourceDispatcherHostDelegate::HandleExternalProtocol(
    const GURL& url,
    ResourceRequestInfo* info) {
  return true;
}

bool ResourceDispatcherHostDelegate::ShouldForceDownloadResource(
    const GURL& url,
    const std::string& mime_type) {
  return false;
}

bool ResourceDispatcherHostDelegate::ShouldInterceptResourceAsStream(
    net::URLRequest* request,
    const base::FilePath& plugin_path,
    const std::string& mime_type,
    GURL* origin,
    std::string* payload) {
  return false;
}

void ResourceDispatcherHostDelegate::OnStreamCreated(
    net::URLRequest* request,
    std::unique_ptr<content::StreamInfo> stream) {}

void ResourceDispatcherHostDelegate::OnResponseStarted(
    net::URLRequest* request,
    ResourceContext* resource_context,
    ResourceResponse* response) {}

void ResourceDispatcherHostDelegate::OnRequestRedirected(
    const GURL& redirect_url,
    net::URLRequest* request,
    ResourceContext* resource_context,
    ResourceResponse* response) {
}

void ResourceDispatcherHostDelegate::RequestComplete(
    net::URLRequest* url_request,
    int net_error) {}

// Deprecated.
void ResourceDispatcherHostDelegate::RequestComplete(
    net::URLRequest* url_request) {}

PreviewsState ResourceDispatcherHostDelegate::GetPreviewsState(
    const net::URLRequest& url_request,
    content::ResourceContext* resource_context,
    PreviewsState previews_to_allow) {
  return PREVIEWS_UNSPECIFIED;
}

NavigationData* ResourceDispatcherHostDelegate::GetNavigationData(
    net::URLRequest* request) const {
  return nullptr;
}

std::unique_ptr<net::ClientCertStore>
ResourceDispatcherHostDelegate::CreateClientCertStore(
    ResourceContext* resource_context) {
  return std::unique_ptr<net::ClientCertStore>();
}

bool ResourceDispatcherHostDelegate::ShouldUseResourceScheduler() const {
  return true;
}

ResourceDispatcherHostDelegate::~ResourceDispatcherHostDelegate() {
}

}  // namespace content
