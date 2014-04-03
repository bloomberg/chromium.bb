// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RESOURCE_DISPATCHER_HOST_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_RESOURCE_DISPATCHER_HOST_DELEGATE_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "webkit/common/resource_type.h"

class GURL;
template <class T> class ScopedVector;

namespace appcache {
class AppCacheService;
}

namespace content {
class ResourceContext;
class ResourceThrottle;
class StreamHandle;
struct Referrer;
struct ResourceResponse;
}

namespace IPC {
class Sender;
}

namespace net {
class AuthChallengeInfo;
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
      ResourceContext* resource_context);

  // Called after ShouldBeginRequest to allow the embedder to add resource
  // throttles.
  virtual void RequestBeginning(
      net::URLRequest* request,
      ResourceContext* resource_context,
      appcache::AppCacheService* appcache_service,
      ResourceType::Type resource_type,
      int child_id,
      int route_id,
      ScopedVector<ResourceThrottle>* throttles);

  // Allows an embedder to add additional resource handlers for a download.
  // |must_download| is set if the request must be handled as a download.
  virtual void DownloadStarting(
      net::URLRequest* request,
      ResourceContext* resource_context,
      int child_id,
      int route_id,
      int request_id,
      bool is_content_initiated,
      bool must_download,
      ScopedVector<ResourceThrottle>* throttles);

  // Creates a ResourceDispatcherHostLoginDelegate that asks the user for a
  // username and password.
  virtual ResourceDispatcherHostLoginDelegate* CreateLoginDelegate(
      net::AuthChallengeInfo* auth_info, net::URLRequest* request);

  // Launches the url for the given tab. Returns true if an attempt to handle
  // the url was made, e.g. by launching an app. Note that this does not
  // guarantee that the app successfully handled it.
  // Some external protocol handlers only run in the context of a user gesture,
  // so initiated_by_user_gesture should be passed accordingly.
  virtual bool HandleExternalProtocol(const GURL& url,
                                      int child_id,
                                      int route_id,
                                      bool initiated_by_user_gesture);

  // Returns true if we should force the given resource to be downloaded.
  // Otherwise, the content layer decides.
  virtual bool ShouldForceDownloadResource(
      const GURL& url, const std::string& mime_type);

  // Returns true and sets |origin| and |target_id| if a Stream should be
  // created for the resource.
  // If true is returned, a new Stream will be created and OnStreamCreated()
  // will be called with
  // - the |target_id| returned by this function
  // - a StreamHandle instance for the Stream. The handle contains the URL for
  //   reading the Stream etc.
  // The Stream's origin will be set to |origin|.
  virtual bool ShouldInterceptResourceAsStream(
      content::ResourceContext* resource_context,
      const GURL& url,
      const std::string& mime_type,
      GURL* origin,
      std::string* target_id);

  // Informs the delegate that a Stream was created. |target_id| will be filled
  // with the parameter returned by ShouldInterceptResourceAsStream(). The
  // Stream can be read from the blob URL of the Stream, but can only be read
  // once.
  virtual void OnStreamCreated(
      content::ResourceContext* resource_context,
      int render_process_id,
      int render_view_id,
      const std::string& target_id,
      scoped_ptr<StreamHandle> stream,
      int64 expected_content_size);

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

  // Notification that a request has completed.
  virtual void RequestComplete(net::URLRequest* url_request);

 protected:
  ResourceDispatcherHostDelegate();
  virtual ~ResourceDispatcherHostDelegate();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RESOURCE_DISPATCHER_HOST_DELEGATE_H_
