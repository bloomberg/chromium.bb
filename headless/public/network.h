// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_PUBLIC_NETWORK_H_
#define HEADLESS_PUBLIC_NETWORK_H_

#include <base/macros.h>
#include <base/memory/ref_counted.h>
#include <base/memory/scoped_ptr.h>

namespace net {
class URLRequestContextGetter;
class ClientSocketFactory;
class HttpTransactionFactory;
}

namespace headless {

class Network {
 public:
  static scoped_refptr<net::URLRequestContextGetter>
  CreateURLRequestContextGetterUsingSocketFactory(
      scoped_ptr<net::ClientSocketFactory> socket_factory);

  static scoped_refptr<net::URLRequestContextGetter>
  CreateURLRequestContextGetterUsingHttpTransactionFactory(
      scoped_ptr<net::HttpTransactionFactory> http_transaction_factory);

 private:
  Network() = delete;
};

}  // namespace headless

#endif  // HEADLESS_PUBLIC_NETWORK_H_
