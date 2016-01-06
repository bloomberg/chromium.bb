// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_CLOUD_PRINT_PRIVET_HTTP_ASYNCHRONOUS_FACTORY_H_
#define CHROME_BROWSER_PRINTING_CLOUD_PRINT_PRIVET_HTTP_ASYNCHRONOUS_FACTORY_H_

#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"

namespace net {
class HostPortPair;
class URLRequestContextGetter;
}

namespace cloud_print {

class PrivetHTTPClient;

class PrivetHTTPResolution {
 public:
  using ResultCallback = base::Callback<void(scoped_ptr<PrivetHTTPClient>)>;

  virtual ~PrivetHTTPResolution() {}

  virtual void Start(const ResultCallback& callback) = 0;

  virtual void Start(const net::HostPortPair& address,
                     const ResultCallback& callback) = 0;

  virtual const std::string& GetName() = 0;
};

class PrivetHTTPAsynchronousFactory {
 public:
  using ResultCallback = PrivetHTTPResolution::ResultCallback;

  virtual ~PrivetHTTPAsynchronousFactory() {}

  static scoped_ptr<PrivetHTTPAsynchronousFactory> CreateInstance(
      net::URLRequestContextGetter* request_context);

  virtual scoped_ptr<PrivetHTTPResolution> CreatePrivetHTTP(
      const std::string& service_name) = 0;
};

}  // namespace cloud_print

#endif  // CHROME_BROWSER_PRINTING_CLOUD_PRINT_PRIVET_HTTP_ASYNCHRONOUS_FACTORY_H_
