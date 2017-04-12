// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_NETWORK_NETWORK_CONTEXT_H_
#define CONTENT_NETWORK_NETWORK_CONTEXT_H_

#include <stdint.h>

#include <memory>
#include <set>

#include "base/macros.h"
#include "content/common/content_export.h"

namespace net {
class URLRequestContext;
}

namespace content {
class URLLoaderImpl;

class CONTENT_EXPORT NetworkContext {
 public:
  NetworkContext();
  ~NetworkContext();

  net::URLRequestContext* url_request_context() {
    return url_request_context_.get();
  }

  // These are called by individual url loaders as they are being created and
  // destroyed.
  void RegisterURLLoader(URLLoaderImpl* url_loader);
  void DeregisterURLLoader(URLLoaderImpl* url_loader);

 private:
  class MojoNetLog;
  std::unique_ptr<MojoNetLog> net_log_;

  std::unique_ptr<net::URLRequestContext> url_request_context_;

  // URLLoaderImpls register themselves with the NetworkContext so that they can
  // be cleaned up when the NetworkContext goes away. This is needed as
  // net::URLRequests held by URLLoaderImpls have to be gone when
  // net::URLRequestContext (held by NetworkContext) is destroyed.
  std::set<URLLoaderImpl*> url_loaders_;

  // Set when entering the destructor, in order to avoid manipulations of the
  // |url_loaders_| (as a url_loader might delete itself in Cleanup()).
  bool in_shutdown_;

  DISALLOW_COPY_AND_ASSIGN(NetworkContext);
};

}  // namespace content

#endif  // CONTENT_NETWORK_NETWORK_CONTEXT_H_
