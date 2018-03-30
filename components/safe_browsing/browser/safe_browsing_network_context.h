// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_BROWSER_SAFE_BROWSING_NETWORK_CONTEXT_H_
#define COMPONENTS_SAFE_BROWSING_BROWSER_SAFE_BROWSING_NETWORK_CONTEXT_H_

#include "base/memory/ref_counted.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace net {
class URLRequestContextGetter;
}

namespace network {
namespace mojom {
class NetworkContext;
}
}  // namespace network

namespace safe_browsing {

// This class owns the NetworkContext that is used for requests by Safe
// Browsing.
// All methods are called on the UI thread.
// Note: temporarily this is wrapping SafeBrowsingURLRequestContextGetter,
// however once all requests are converted to using the network service mojo
// APIs we can delete SafeBrowsingURLRequestContextGetter and this object will
// create the NetworkContext directly.  http://crbug.com/825242
class SafeBrowsingNetworkContext {
 public:
  explicit SafeBrowsingNetworkContext(
      scoped_refptr<net::URLRequestContextGetter> request_context_getter);
  ~SafeBrowsingNetworkContext();

  // Returns a SharedURLLoaderFactory.
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory();

  // Returns a NetworkContext.
  network::mojom::NetworkContext* GetNetworkContext();

  // Called at shutdown to ensure that the URLRequestContextGetter reference is
  // destroyed..
  void ServiceShuttingDown();

 private:
  class SharedURLLoaderFactory;

  scoped_refptr<SharedURLLoaderFactory> url_loader_factory_;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_BROWSER_SAFE_BROWSING_NETWORK_CONTEXT_H_
