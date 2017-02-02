// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_NAVIGATION_RESOURCE_HANDLER_H_
#define CONTENT_BROWSER_LOADER_NAVIGATION_RESOURCE_HANDLER_H_

#include <memory>

#include "base/macros.h"
#include "content/browser/loader/resource_handler.h"
#include "content/browser/loader/stream_writer.h"

namespace net {
class SSLInfo;
}

namespace content {
class NavigationURLLoaderImplCore;
class ResourceController;
class ResourceDispatcherHostDelegate;
struct SSLStatus;

// PlzNavigate: The leaf ResourceHandler used with NavigationURLLoaderImplCore.
class NavigationResourceHandler : public ResourceHandler {
 public:
  static void GetSSLStatusForRequest(const GURL& url,
                                     const net::SSLInfo& ssl_info,
                                     int child_id,
                                     SSLStatus* ssl_status);

  NavigationResourceHandler(
      net::URLRequest* request,
      NavigationURLLoaderImplCore* core,
      ResourceDispatcherHostDelegate* resource_dispatcher_host_delegate);
  ~NavigationResourceHandler() override;

  // Called by the loader the cancel the request.
  void Cancel();

  // Called to the loader to resume a paused redirect.
  void FollowRedirect();

  // Called to proceed with the response.
  void ProceedWithResponse();

  // ResourceHandler implementation.
  void OnRequestRedirected(
      const net::RedirectInfo& redirect_info,
      ResourceResponse* response,
      std::unique_ptr<ResourceController> controller) override;
  void OnResponseStarted(
      ResourceResponse* response,
      std::unique_ptr<ResourceController> controller) override;
  void OnWillStart(const GURL& url,
                   std::unique_ptr<ResourceController> controller) override;
  bool OnWillRead(scoped_refptr<net::IOBuffer>* buf,
                  int* buf_size) override;
  void OnReadCompleted(int bytes_read,
                       std::unique_ptr<ResourceController> controller) override;
  void OnResponseCompleted(
      const net::URLRequestStatus& status,
      std::unique_ptr<ResourceController> controller) override;
  void OnDataDownloaded(int bytes_downloaded) override;

 private:
  // Clears |core_| and its reference to the resource handler. After calling
  // this, the lifetime of the request is no longer tied to |core_|.
  void DetachFromCore();

  NavigationURLLoaderImplCore* core_;
  StreamWriter writer_;
  ResourceDispatcherHostDelegate* resource_dispatcher_host_delegate_;

  DISALLOW_COPY_AND_ASSIGN(NavigationResourceHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_NAVIGATION_RESOURCE_HANDLER_H_
