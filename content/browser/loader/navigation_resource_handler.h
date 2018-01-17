// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_NAVIGATION_RESOURCE_HANDLER_H_
#define CONTENT_BROWSER_LOADER_NAVIGATION_RESOURCE_HANDLER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/browser/loader/layered_resource_handler.h"
#include "content/public/browser/stream_handle.h"

namespace content {
class NavigationURLLoaderImplCore;
class ResourceController;
class ResourceDispatcherHostDelegate;

// PlzNavigate: The ResourceHandler used with NavigationURLLoaderImplCore to
// control the flow of navigation requests.
class NavigationResourceHandler : public LayeredResourceHandler {
 public:
  NavigationResourceHandler(
      net::URLRequest* request,
      std::unique_ptr<ResourceHandler> next_handler,
      NavigationURLLoaderImplCore* core,
      ResourceDispatcherHostDelegate* resource_dispatcher_host_delegate,
      std::unique_ptr<StreamHandle> stream_handle);
  ~NavigationResourceHandler() override;

  // Called by the loader the cancel the request.
  void Cancel();

  // Called to the loader to resume a paused redirect.
  void FollowRedirect();

  // Called to proceed with the response.
  void ProceedWithResponse();

  // LayeredResourceHandler implementation.
  void OnRequestRedirected(
      const net::RedirectInfo& redirect_info,
      network::ResourceResponse* response,
      std::unique_ptr<ResourceController> controller) override;
  void OnResponseStarted(
      network::ResourceResponse* response,
      std::unique_ptr<ResourceController> controller) override;
  void OnResponseCompleted(
      const net::URLRequestStatus& status,
      std::unique_ptr<ResourceController> controller) override;

 private:
  // Clears |core_| and its reference to the resource handler. After calling
  // this, the lifetime of the request is no longer managed by the
  // NavigationURLLoader.
  void DetachFromCore();

  // Used to buffer the response and redirect info while waiting for UI thread
  // checks to execute.
  scoped_refptr<network::ResourceResponse> response_;
  std::unique_ptr<net::RedirectInfo> redirect_info_;

  // NavigationResourceHandler has joint ownership of the
  // NavigationURLLoaderImplCore with the NavigationURLLoaderImpl.
  scoped_refptr<NavigationURLLoaderImplCore> core_;
  std::unique_ptr<StreamHandle> stream_handle_;
  ResourceDispatcherHostDelegate* resource_dispatcher_host_delegate_;

  DISALLOW_COPY_AND_ASSIGN(NavigationResourceHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_NAVIGATION_RESOURCE_HANDLER_H_
