// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_HTTP_ASYNCHRONOUS_FACTORY_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_HTTP_ASYNCHRONOUS_FACTORY_H_

#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"

namespace net {
class HostPortPair;
class URLRequestContextGetter;
}

namespace local_discovery {

class PrivetHTTPClient;
class ServiceDiscoveryClient;

class PrivetHTTPResolution {
 public:
  virtual ~PrivetHTTPResolution() {}
  virtual void Start() = 0;
  virtual const std::string& GetName() = 0;
};

class PrivetHTTPAsynchronousFactory {
 public:
  typedef base::Callback<void(scoped_ptr<PrivetHTTPClient>)> ResultCallback;

  virtual ~PrivetHTTPAsynchronousFactory() {}

  static scoped_ptr<PrivetHTTPAsynchronousFactory> CreateInstance(
      ServiceDiscoveryClient* service_discovery_client,
      net::URLRequestContextGetter* request_context);

  virtual scoped_ptr<PrivetHTTPResolution> CreatePrivetHTTP(
      const std::string& name,
      const net::HostPortPair& address,
      const ResultCallback& callback) = 0;
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_HTTP_ASYNCHRONOUS_FACTORY_H_
