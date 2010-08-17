// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_REQUEST_THROTTLER_REQUEST_THROTTLER_HEADER_ADAPTER_H_
#define NET_REQUEST_THROTTLER_REQUEST_THROTTLER_HEADER_ADAPTER_H_

#include <string>

#include "base/scoped_ptr.h"
#include "net/http/http_response_headers.h"
#include "net/request_throttler/request_throttler_header_interface.h"

namespace net {
class HttpResponseHeaders;
}

// Adapter for the http header interface of the request throttler component.
class RequestThrottlerHeaderAdapter : public RequestThrottlerHeaderInterface {
 public:
  RequestThrottlerHeaderAdapter(
      const scoped_refptr<net::HttpResponseHeaders>& headers);
  virtual ~RequestThrottlerHeaderAdapter() {}

  // Implementation of the Http header interface
  virtual std::string GetNormalizedValue(const std::string& key) const;
  virtual int GetResponseCode() const;
 private:
  const scoped_refptr<net::HttpResponseHeaders> response_header_;
};

#endif  // NET_REQUEST_THROTTLER_REQUEST_THROTTLER_HEADER_ADAPTER_H_
