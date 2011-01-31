// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_CHROME_NETWORK_DELEGATE_H_
#define CHROME_BROWSER_NET_CHROME_NETWORK_DELEGATE_H_
#pragma once

#include "base/basictypes.h"
#include "net/http/http_network_delegate.h"

// ChromeNetworkDelegate is the central point from within the chrome code to
// add hooks into the network stack.
class ChromeNetworkDelegate : public net::HttpNetworkDelegate {
 public:
  ChromeNetworkDelegate();
  ~ChromeNetworkDelegate();

  // net::HttpNetworkDelegate methods:
  virtual void OnBeforeURLRequest(net::URLRequest* request);
  virtual void OnSendHttpRequest(net::HttpRequestHeaders* headers);

  // TODO(willchan): Add functions for consumers to register ways to
  // access/modify the request.

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeNetworkDelegate);
};

#endif  // CHROME_BROWSER_NET_CHROME_NETWORK_DELEGATE_H_
