// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/network/network_context.h"

#include <algorithm>
#include <vector>

#include "base/base_paths.h"
#include "base/path_service.h"
#include "mojo/services/network/url_loader_impl.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"

namespace mojo {

NetworkContext::NetworkContext(
    scoped_ptr<net::URLRequestContext> url_request_context)
    : url_request_context_(url_request_context.Pass()),
      in_shutdown_(false) {
}

NetworkContext::NetworkContext(const base::FilePath& base_path)
    : NetworkContext(MakeURLRequestContext(base_path)) {
}

NetworkContext::~NetworkContext() {
  in_shutdown_ = true;
  // TODO(darin): Be careful about destruction order of member variables?

  // Call each URLLoaderImpl and ask it to release its net::URLRequest, as the
  // corresponding net::URLRequestContext is going away with this
  // NetworkContext. The loaders can be deregistering themselves in Cleanup(),
  // so iterate over a copy.
  for (auto& url_loader : url_loaders_) {
    url_loader->Cleanup();
  }
}

void NetworkContext::RegisterURLLoader(URLLoaderImpl* url_loader) {
  DCHECK(url_loaders_.count(url_loader) == 0);
  url_loaders_.insert(url_loader);
}

void NetworkContext::DeregisterURLLoader(URLLoaderImpl* url_loader) {
  if (!in_shutdown_) {
    size_t removed_count = url_loaders_.erase(url_loader);
    DCHECK(removed_count);
  }
}

size_t NetworkContext::GetURLLoaderCountForTesting() {
  return url_loaders_.size();
}

// static
scoped_ptr<net::URLRequestContext> NetworkContext::MakeURLRequestContext(
    const base::FilePath& base_path) {
  net::URLRequestContextBuilder builder;
  builder.set_accept_language("en-us,en");
  // TODO(darin): This is surely the wrong UA string.
  builder.set_user_agent("Mojo/0.1");
  builder.set_proxy_service(net::ProxyService::CreateDirect());
  builder.set_transport_security_persister_path(base_path);

  net::URLRequestContextBuilder::HttpCacheParams cache_params;
#if defined(OS_ANDROID)
  // On Android, we store the cache on disk becase we can run only a single
  // instance of the shell at a time.
  cache_params.type = net::URLRequestContextBuilder::HttpCacheParams::DISK;
  cache_params.path = base_path.Append(FILE_PATH_LITERAL("Cache"));
#else
  // On desktop, we store the cache in memory so we can run many shells
  // in parallel when running tests, otherwise the network services in each
  // shell will corrupt the disk cache.
  cache_params.type = net::URLRequestContextBuilder::HttpCacheParams::IN_MEMORY;
#endif

  builder.EnableHttpCache(cache_params);
  builder.set_file_enabled(true);
  return make_scoped_ptr(builder.Build());
}

}  // namespace mojo
