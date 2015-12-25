// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_IMPL_CORE_H_
#define CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_IMPL_CORE_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/loader/navigation_url_loader_impl.h"

namespace net {
class URLRequest;
struct RedirectInfo;
}

namespace content {

class FrameTreeNode;
class NavigationResourceHandler;
class ResourceContext;
class ResourceHandler;
class ResourceRequestBody;
class ServiceWorkerNavigationHandleCore;
class StreamHandle;
struct ResourceResponse;

// The IO-thread counterpart to the NavigationURLLoaderImpl. It lives on the IO
// thread and is owned by the UI-thread NavigationURLLoaderImpl.
// NavigationURLLoaderImplCore interacts with the ResourceDispatcherHost stack
// and forwards signals back to the loader on the UI thread.
class NavigationURLLoaderImplCore {
 public:
  // Creates a new NavigationURLLoaderImplCore that forwards signals back to
  // |loader| on the UI thread.
  explicit NavigationURLLoaderImplCore(
      const base::WeakPtr<NavigationURLLoaderImpl>& loader);
  ~NavigationURLLoaderImplCore();

  // Starts the request.
  void Start(ResourceContext* resource_context,
             ServiceWorkerNavigationHandleCore* service_worker_handle_core,
             scoped_ptr<NavigationRequestInfo> request_info);

  // Follows the current pending redirect.
  void FollowRedirect();

  void set_resource_handler(NavigationResourceHandler* resource_handler) {
    resource_handler_ = resource_handler;
  }

  // Notifies |loader_| on the UI thread that the request was redirected.
  void NotifyRequestRedirected(const net::RedirectInfo& redirect_info,
                               ResourceResponse* response);

  // Notifies |loader_| on the UI thread that the response started.
  void NotifyResponseStarted(ResourceResponse* response,
                             scoped_ptr<StreamHandle> body);

  // Notifies |loader_| on the UI thread that the request failed.
  void NotifyRequestFailed(bool in_cache, int net_error);

 private:
  base::WeakPtr<NavigationURLLoaderImpl> loader_;
  NavigationResourceHandler* resource_handler_;

  DISALLOW_COPY_AND_ASSIGN(NavigationURLLoaderImplCore);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_IMPL_CORE_H_
