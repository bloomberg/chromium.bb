// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RESOURCE_DISPATCHER_HOST_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_RESOURCE_DISPATCHER_HOST_DELEGATE_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "content/common/content_export.h"
#include "webkit/glue/resource_type.h"

class GURL;
class ResourceHandler;
class ResourceMessageFilter;

namespace content {
struct Referrer;
class ResourceContext;
struct ResourceResponse;
}

namespace net {
class AuthChallengeInfo;
class SSLCertRequestInfo;
class URLRequest;
}

namespace content {

class ResourceDispatcherHostLoginDelegate;

// Interface that the embedder provides to ResourceDispatcherHost to allow
// observing and modifying requests.
class CONTENT_EXPORT ResourceDispatcherHostDelegate {
 public:
  // Called when a request begins. Return false to abort the request.
  virtual bool ShouldBeginRequest(
      int child_id,
      int route_id,
      const std::string& method,
      const GURL& url,
      ResourceType::Type resource_type,
      const ResourceContext& resource_context,
      const Referrer& referrer);

  // Called after ShouldBeginRequest when all the resource handlers from the
  // content layer have been added.  To add new handlers to the front, return
  // a new handler that is chained to the given one, otherwise just reutrn the
  // given handler.
  virtual ResourceHandler* RequestBeginning(
      ResourceHandler* handler,
      net::URLRequest* request,
      const ResourceContext& resource_context,
      bool is_subresource,
      int child_id,
      int route_id,
      bool is_continuation_of_transferred_request);

  // Allows an embedder to add additional resource handlers for a download.
  // |is_new_request| is true if this is a request that is just starting, i.e.
  // the content layer has just added its own resource handlers; it's false if
  // this was originally a non-download request that had some resource handlers
  // applied already and now we found out it's a download.
  // |in_complete| is true if this is invoked from |OnResponseCompleted|.
  virtual ResourceHandler* DownloadStarting(
      ResourceHandler* handler,
      const ResourceContext& resource_context,
      net::URLRequest* request,
      int child_id,
      int route_id,
      int request_id,
      bool is_new_request);

  // Called to determine whether a request's start should be deferred. This
  // is only called if the ResourceHandler associated with the request does
  // not ask for a deferral. A return value of true will defer the start of
  // the request, false will continue the request.
  virtual bool ShouldDeferStart(
      net::URLRequest* request,
      const ResourceContext& resource_context);

  // Called when an SSL Client Certificate is requested. If false is returned,
  // the request is canceled. Otherwise, the certificate is chosen.
  virtual bool AcceptSSLClientCertificateRequest(
      net::URLRequest* request,
      net::SSLCertRequestInfo* cert_request_info);

  // Called when authentication is required and credentials are needed. If
  // false is returned, CancelAuth() is called on the URLRequest and the error
  // page is shown. If true is returned, the user will be prompted for
  // authentication credentials.
  virtual bool AcceptAuthRequest(net::URLRequest* request,
                                 net::AuthChallengeInfo* auth_info);

  // Creates a ResourceDispatcherHostLoginDelegate that asks the user for a
  // username and password.
  virtual ResourceDispatcherHostLoginDelegate* CreateLoginDelegate(
      net::AuthChallengeInfo* auth_info, net::URLRequest* request);

  // Launches the url for the given tab.
  virtual void HandleExternalProtocol(const GURL& url,
                                      int child_id,
                                      int route_id);

  // Returns true if we should force the given resource to be downloaded.
  // Otherwise, the content layer decides.
  virtual bool ShouldForceDownloadResource(
      const GURL& url, const std::string& mime_type);

  // Informs the delegate that a response has started.
  virtual void OnResponseStarted(
      net::URLRequest* request,
      ResourceResponse* response,
      ResourceMessageFilter* filter);

  // Informs the delegate that a request has been redirected.
  virtual void OnRequestRedirected(
      net::URLRequest* request,
      ResourceResponse* response,
      ResourceMessageFilter* filter);

 protected:
  ResourceDispatcherHostDelegate();
  virtual ~ResourceDispatcherHostDelegate();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RESOURCE_DISPATCHER_HOST_DELEGATE_H_
