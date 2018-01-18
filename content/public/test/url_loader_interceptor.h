// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_URL_LOADER_INTERCEPTOR_H_
#define CONTENT_PUBLIC_BROWSER_URL_LOADER_INTERCEPTOR_H_

#include <set>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/interfaces/url_loader_factory.mojom.h"

namespace content {
class URLLoaderFactoryGetter;

// Helper class to intercept URLLoaderFactory calls for tests.
// This intercepts:
//   -frame requests (which start from the browser, with PlzNavigate)
//   -subresource requests
//     -at ResourceMessageFilter for non network-service code path
//     -by sending renderer an intermediate URLLoaderFactory for network-service
//      codepath, as that normally routes directly to the network process
//     -http(s)://mock.failed.request/foo URLs internally, copying the behavior
//      of net::URLRequestFailedJob
//
// Notes:
//  -intercepting frame requests doesn't work yet for non network-service case
//   (will work once http://crbug.com/747130 is fixed)
//  -the callback is always called on the IO thread
//    -this is done to avoid changing message order
//  -intercepting resource requests for subresources when the network service is
//   enabled changes message order by definition (since they would normally go
//   directly from renderer->network process, but now they're routed through the
//   browser). This is why |intercept_subresources| is false by default.
//  -of course this only works when MojoLoading is enabled, which is default
//   for all shipping configs (TODO(jam): delete this comment when old path is
//   deleted)
//  -it doesn't yet intercept other request types, e.g. browser-initiated
//   requests that aren't for frames
class URLLoaderInterceptor {
 public:
  struct RequestParams {
    RequestParams();
    ~RequestParams();
    // This is the process_id of the process that is making the request (0 for
    // browser process).
    int process_id;
    // The following are the parameters to CreateLoaderAndStart.
    network::mojom::URLLoaderRequest request;
    int32_t routing_id;
    int32_t request_id;
    uint32_t options;
    network::ResourceRequest url_request;
    network::mojom::URLLoaderClientPtr client;
    net::MutableNetworkTrafficAnnotationTag traffic_annotation;
  };
  // Function signature for intercept method.
  // Return true if the request was intercepted. Otherwise this class will
  // forward the request to the original URLLoaderFactory.
  using InterceptCallback = base::Callback<bool(RequestParams* params)>;

  URLLoaderInterceptor(const InterceptCallback& callback,
                       bool intercept_frame_requests = true,
                       bool intercept_subresources = false);
  ~URLLoaderInterceptor();

  // Helper methods for use when intercepting.
  static void WriteResponse(const std::string& headers,
                            const std::string& body,
                            network::mojom::URLLoaderClient* client);

 private:
  class Interceptor;
  class SubresourceWrapper;
  class URLLoaderFactoryGetterWrapper;

  // Used to create a factory for subresources in the network service case.
  void CreateURLLoaderFactoryForSubresources(
      network::mojom::URLLoaderFactoryRequest request,
      int process_id,
      network::mojom::URLLoaderFactoryPtrInfo original_factory);

  // Callback on IO thread whenever a URLLoaderFactoryGetter::GetNetworkContext
  // is called on an object that doesn't have a test factory set up.
  void GetNetworkFactoryCallback(
      URLLoaderFactoryGetter* url_loader_factory_getter);

  // Called when a SubresourceWrapper's binding has an error.
  void SubresourceWrapperBindingError(SubresourceWrapper* wrapper);

  // Called on IO thread at initialization and shutdown.
  void InitializeOnIOThread(base::OnceClosure closure);
  void ShutdownOnIOThread(base::OnceClosure closure);

  InterceptCallback callback_;
  bool intercept_frame_requests_;
  bool intercept_subresources_;
  // For intercepting frame requests with network service. There is one per
  // StoragePartition. Only accessed on IO thread.
  std::set<std::unique_ptr<URLLoaderFactoryGetterWrapper>>
      url_loader_factory_getter_wrappers_;
  // For intercepting subresources without network service in
  // ResourceMessageFilter.
  std::unique_ptr<Interceptor> rmf_interceptor_;
  // For intercepting subresources with network service. There is one per active
  // render frame commit. Only accessed on IO thread.
  std::set<std::unique_ptr<SubresourceWrapper>> subresource_wrappers_;

  DISALLOW_COPY_AND_ASSIGN(URLLoaderInterceptor);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_URL_LOADER_INTERCEPTOR_H_
