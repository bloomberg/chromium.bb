// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_VARIATIONS_SERVICE_CLIENT_H_
#define COMPONENTS_VARIATIONS_VARIATIONS_SERVICE_CLIENT_H_

#include <string>

namespace net {
class URLRequestContextGetter;
}

namespace network_time {
class NetworkTimeTracker;
}

namespace chrome_variations {

// An abstraction of operations that depend on the embedder's (e.g. Chrome)
// environment.
class VariationsServiceClient {
 public:
  virtual ~VariationsServiceClient() {}

  // Returns the current application locale (e.g. "en-US").
  virtual std::string GetApplicationLocale() = 0;

  virtual net::URLRequestContextGetter* GetURLRequestContext() = 0;
  virtual network_time::NetworkTimeTracker* GetNetworkTimeTracker() = 0;
};

}  // namespace chrome_variations

#endif  // COMPONENTS_VARIATIONS_VARIATIONS_SERVICE_CLIENT_H_
