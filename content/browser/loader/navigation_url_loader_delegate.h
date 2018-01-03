// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_DELEGATE_H_
#define CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_DELEGATE_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "content/common/content_export.h"
#include "content/public/common/url_loader.mojom.h"

namespace base {
class Value;
}

namespace net {
struct RedirectInfo;
class SSLInfo;
}

namespace content {

class StreamHandle;
struct GlobalRequestID;
struct ResourceResponse;
struct SubresourceLoaderParams;

// PlzNavigate: The delegate interface to NavigationURLLoader.
class CONTENT_EXPORT NavigationURLLoaderDelegate {
 public:
  // Called when the request is redirected. Call FollowRedirect to continue
  // processing the request.
  virtual void OnRequestRedirected(
      const net::RedirectInfo& redirect_info,
      const scoped_refptr<ResourceResponse>& response) = 0;

  // Called when the request receives its response. No further calls will be
  // made to the delegate. The response body is returned as a stream in
  // |body_stream|. |navigation_data| is passed to the NavigationHandle.
  // If the Network Service or NavigationMojoResponse is enabled, then the
  // |url_loader_client_endpoints| will be used, otherwise |body_stream|. Only
  // one of these will ever be non-null.
  // |subresource_loader_params| is used in the network service only for passing
  // necessary info to create a custom subresource loader in the renderer
  // process if the navigated context is controlled by a request interceptor
  // like AppCache or ServiceWorker.
  virtual void OnResponseStarted(
      const scoped_refptr<ResourceResponse>& response,
      mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
      std::unique_ptr<StreamHandle> body_stream,
      const net::SSLInfo& ssl_info,
      base::Value navigation_data,
      const GlobalRequestID& request_id,
      bool is_download,
      bool is_stream,
      base::Optional<SubresourceLoaderParams> subresource_loader_params) = 0;

  // Called if the request fails before receving a response. |net_error| is a
  // network error code for the failure. |has_stale_copy_in_cache| is true if
  // there is a stale copy of the unreachable page in cache. |ssl_info| is the
  // SSL info for the request. If |net_error| is a certificate error and the
  // navigation request was for the main frame, the caller must pass a value
  // for |ssl_info|. If |net_error| is not a certificate error, |ssl_info| is
  // ignored.
  virtual void OnRequestFailed(
      bool has_stale_copy_in_cache,
      int net_error,
      const base::Optional<net::SSLInfo>& ssl_info) = 0;

  // Called after the network request has begun on the IO thread at time
  // |timestamp|. This is just a thread hop but is used to compare timing
  // against the pre-PlzNavigate codepath which didn't start the network request
  // until after the renderer was initialized.
  virtual void OnRequestStarted(base::TimeTicks timestamp) = 0;

 protected:
  NavigationURLLoaderDelegate() {}
  virtual ~NavigationURLLoaderDelegate() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NavigationURLLoaderDelegate);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_DELEGATE_H_
