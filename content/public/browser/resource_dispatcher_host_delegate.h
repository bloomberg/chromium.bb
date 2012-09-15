// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RESOURCE_DISPATCHER_HOST_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_RESOURCE_DISPATCHER_HOST_DELEGATE_H_

#include <string>

#include "base/basictypes.h"
#include "content/common/content_export.h"
#include "ipc/ipc_message.h"
#include "webkit/glue/resource_type.h"

class GURL;
template <class T> class ScopedVector;

namespace appcache {
class AppCacheService;
}

namespace content {
class ResourceContext;
class ResourceThrottle;
struct Referrer;
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
      ResourceContext* resource_context,
      const Referrer& referrer);

  // Called after ShouldBeginRequest to allow the embedder to add resource
  // throttles.
  virtual void RequestBeginning(
      net::URLRequest* request,
      ResourceContext* resource_context,
      appcache::AppCacheService* appcache_service,
      ResourceType::Type resource_type,
      int child_id,
      int route_id,
      bool is_continuation_of_transferred_request,
      ScopedVector<ResourceThrottle>* throttles);

  // Allows an embedder to add additional resource handlers for a download.
  // |is_new_request| is true if this is a request that is just starting, i.e.
  // the content layer has just added its own resource handlers; it's false if
  // this was originally a non-download request that had some resource handlers
  // applied already and now we found out it's a download.
  // |in_complete| is true if this is invoked from |OnResponseCompleted|.
  virtual void DownloadStarting(
      net::URLRequest* request,
      ResourceContext* resource_context,
      int child_id,
      int route_id,
      int request_id,
      bool is_content_initiated,
      ScopedVector<ResourceThrottle>* throttles);

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
      ResourceContext* resource_context,
      ResourceResponse* response,
      IPC::Sender* sender);

  // Informs the delegate that a request has been redirected.
  virtual void OnRequestRedirected(
      const GURL& redirect_url,
      net::URLRequest* request,
      ResourceContext* resource_context,
      ResourceResponse* response);

 protected:
  ResourceDispatcherHostDelegate();
  virtual ~ResourceDispatcherHostDelegate();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RESOURCE_DISPATCHER_HOST_DELEGATE_H_
