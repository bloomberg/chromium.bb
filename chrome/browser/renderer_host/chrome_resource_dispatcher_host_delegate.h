// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_CHROME_RESOURCE_DISPATCHER_HOST_DELEGATE_H_
#define CHROME_BROWSER_RENDERER_HOST_CHROME_RESOURCE_DISPATCHER_HOST_DELEGATE_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"

class DownloadRequestLimiter;
class ResourceDispatcherHost;
class SafeBrowsingService;

namespace prerender {
class PrerenderTracker;
}

// Implements ResourceDispatcherHostDelegate. Currently used by the Prerender
// system to abort requests and add to the load flags when a request begins.
class ChromeResourceDispatcherHostDelegate
    : public content::ResourceDispatcherHostDelegate {
 public:
  // This class does not take ownership of the tracker but merely holds a
  // reference to it to avoid accessing g_browser_process.
  // Both |resource_dispatcher_host| and |prerender_tracker| must outlive
  // |this|.
  ChromeResourceDispatcherHostDelegate(
      ResourceDispatcherHost* resource_dispatcher_host,
      prerender::PrerenderTracker* prerender_tracker);
  virtual ~ChromeResourceDispatcherHostDelegate();

  // ResourceDispatcherHostDelegate implementation.
  virtual bool ShouldBeginRequest(
      int child_id,
      int route_id,
      const std::string& method,
      const GURL& url,
      ResourceType::Type resource_type,
      content::ResourceContext* resource_context,
      const content::Referrer& referrer) OVERRIDE;
  virtual void RequestBeginning(
      net::URLRequest* request,
      content::ResourceContext* resource_context,
      ResourceType::Type resource_type,
      int child_id,
      int route_id,
      bool is_continuation_of_transferred_request,
      ScopedVector<content::ResourceThrottle>* throttles) OVERRIDE;
  virtual void DownloadStarting(
      net::URLRequest* request,
      content::ResourceContext* resource_context,
      int child_id,
      int route_id,
      int request_id,
      bool is_new_request,
      ScopedVector<content::ResourceThrottle>* throttles) OVERRIDE;
  virtual bool ShouldDeferStart(
      net::URLRequest* request,
      content::ResourceContext* resource_context) OVERRIDE;
  virtual bool AcceptSSLClientCertificateRequest(
        net::URLRequest* request,
        net::SSLCertRequestInfo* cert_request_info) OVERRIDE;
  virtual bool AcceptAuthRequest(net::URLRequest* request,
                                 net::AuthChallengeInfo* auth_info) OVERRIDE;
  virtual content::ResourceDispatcherHostLoginDelegate* CreateLoginDelegate(
      net::AuthChallengeInfo* auth_info, net::URLRequest* request) OVERRIDE;
  virtual void HandleExternalProtocol(const GURL& url,
                                      int child_id,
                                      int route_id) OVERRIDE;
  virtual bool ShouldForceDownloadResource(
      const GURL& url, const std::string& mime_type) OVERRIDE;
  virtual void OnResponseStarted(
      net::URLRequest* request,
      content::ResourceResponse* response,
      IPC::Message::Sender* sender) OVERRIDE;
  virtual void OnRequestRedirected(
      net::URLRequest* request,
      content::ResourceResponse* response) OVERRIDE;

 private:
  content::ResourceThrottle* CreateSafeBrowsingResourceThrottle(
      const net::URLRequest* request, int child_id, int route_id,
      bool subresource);

  ResourceDispatcherHost* resource_dispatcher_host_;
  scoped_refptr<DownloadRequestLimiter> download_request_limiter_;
  scoped_refptr<SafeBrowsingService> safe_browsing_;
  prerender::PrerenderTracker* prerender_tracker_;

  DISALLOW_COPY_AND_ASSIGN(ChromeResourceDispatcherHostDelegate);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_CHROME_RESOURCE_DISPATCHER_HOST_DELEGATE_H_
