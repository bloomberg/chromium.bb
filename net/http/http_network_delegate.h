// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_NETWORK_DELEGATE_H_
#define NET_HTTP_HTTP_NETWORK_DELEGATE_H_
#pragma once

namespace net {

class HttpRequestHeaders;

class HttpNetworkDelegate {
 public:
  // Called right before the HTTP headers are sent.  Allows the delegate to
  // read/write |headers| before they get sent out.
  virtual void OnSendHttpRequest(HttpRequestHeaders* headers) = 0;

 protected:
  virtual ~HttpNetworkDelegate() {}
};

}  // namespace net

#endif  // NET_HTTP_HTTP_NETWORK_DELEGATE_H_
