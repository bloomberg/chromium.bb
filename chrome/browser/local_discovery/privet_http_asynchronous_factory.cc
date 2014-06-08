// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/privet_http_asynchronous_factory.h"

#if defined(OS_MACOSX)
#include "chrome/browser/local_discovery/privet_http_asynchronous_factory_mac.h"
#else
#include "chrome/browser/local_discovery/privet_http_asynchronous_factory_impl.h"
#endif

namespace local_discovery {

// static
scoped_ptr<PrivetHTTPAsynchronousFactory>
PrivetHTTPAsynchronousFactory::CreateInstance(
    ServiceDiscoveryClient* service_discovery_client,
    net::URLRequestContextGetter* request_context) {
#if defined(OS_MACOSX)
  return make_scoped_ptr<PrivetHTTPAsynchronousFactory>(
      new PrivetHTTPAsynchronousFactoryMac(request_context));

#else
  return make_scoped_ptr<PrivetHTTPAsynchronousFactory>(
      new PrivetHTTPAsynchronousFactoryImpl(service_discovery_client,
                                            request_context));
#endif
}

}  // namespace local_discovery
