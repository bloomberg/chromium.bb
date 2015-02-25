// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/privet_http_asynchronous_factory.h"

#include "chrome/browser/local_discovery/privet_http_asynchronous_factory_impl.h"

namespace local_discovery {

// static
scoped_ptr<PrivetHTTPAsynchronousFactory>
PrivetHTTPAsynchronousFactory::CreateInstance(
    net::URLRequestContextGetter* request_context) {
  return make_scoped_ptr<PrivetHTTPAsynchronousFactory>(
      new PrivetHTTPAsynchronousFactoryImpl(request_context));
}

}  // namespace local_discovery
