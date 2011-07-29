// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RESOURCE_DISPATCHER_HOST_DELEGATE_H_
#define CONTENT_BROWSER_RENDERER_HOST_RESOURCE_DISPATCHER_HOST_DELEGATE_H_
#pragma once

#include <string>

#include "base/basictypes.h"

class GURL;
class ResourceDispatcherHostLoginDelegate;
class ResourceHandler;
class ResourceMessageFilter;
struct ResourceHostMsg_Request;
struct ResourceResponse;

namespace content {
class ResourceContext;
}

namespace net {
class AuthChallengeInfo;
class SSLCertRequestInfo;
class URLRequest;
}

// Interface that the embedder provides to ResourceDispatcherHost to allow
// observing and modifying requests.
class ResourceDispatcherHostDelegate {
 public:
  // Called when a request begins. Return false to abort the request.
  virtual bool ShouldBeginRequest(
      int child_id, int route_id,
      const ResourceHostMsg_Request& request_data,
      const content::ResourceContext& resource_context,
      const GURL& referrer) = 0;

  // Called after ShouldBeginRequest when all the resource handlers from the
  // content layer have been added.  To add new handlers to the front, return
  // a new handler that is chained to the given one, otherwise just reutrn the
  // given handler.
  virtual ResourceHandler* RequestBeginning(
      ResourceHandler* handler,
      net::URLRequest* request,
      const content::ResourceContext& resource_context,
      bool is_subresource,
      int child_id,
      int route_id) = 0;

  // Called when a download is starting, after the resource handles from the
  // content layer have been added.
  virtual ResourceHandler* DownloadStarting(
      ResourceHandler* handler,
      const content::ResourceContext& resource_context,
      int child_id,
      int route_id) = 0;

  // Called to determine whether a request's start should be deferred. This
  // is only called if the ResourceHandler associated with the request does
  // not ask for a deferral. A return value of true will defer the start of
  // the request, false will continue the request.
  virtual bool ShouldDeferStart(
      net::URLRequest* request,
      const content::ResourceContext& resource_context) = 0;

  // Called when an SSL Client Certificate is requested. If false is returned,
  // the request is canceled. Otherwise, the certificate is chosen.
  virtual bool AcceptSSLClientCertificateRequest(
      net::URLRequest* request,
      net::SSLCertRequestInfo* cert_request_info) = 0;

  // Called when authentication is required and credentials are needed. If
  // false is returned, CancelAuth() is called on the URLRequest and the error
  // page is shown. If true is returned, the user will be prompted for
  // authentication credentials.
  virtual bool AcceptAuthRequest(net::URLRequest* request,
                                 net::AuthChallengeInfo* auth_info) = 0;

  // Creates a ResourceDispatcherHostLoginDelegate that asks the user for a
  // username and password.
  virtual ResourceDispatcherHostLoginDelegate* CreateLoginDelegate(
      net::AuthChallengeInfo* auth_info, net::URLRequest* request) = 0;

  // Launches the url for the given tab.
  virtual void HandleExternalProtocol(const GURL& url,
                                      int child_id,
                                      int route_id) = 0;

  // Returns true if we should force the given resource to be downloaded.
  // Otherwise, the content layer decides.
  virtual bool ShouldForceDownloadResource(
      const GURL& url, const std::string& mime_type) = 0;

  // Informs the delegate that a response has started.
  virtual void OnResponseStarted(
      net::URLRequest* request,
      ResourceResponse* response,
      ResourceMessageFilter* filter) = 0;

  // Informs the delegate that a request has been redirected.
  virtual void OnRequestRedirected(
      net::URLRequest* request,
      ResourceResponse* response,
      ResourceMessageFilter* filter) = 0;

 protected:
   ResourceDispatcherHostDelegate() {}
   virtual ~ResourceDispatcherHostDelegate() {}
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_RESOURCE_DISPATCHER_HOST_DELEGATE_H_
