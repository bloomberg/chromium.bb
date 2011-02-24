// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_CHROME_NETWORK_DELEGATE_H_
#define CHROME_BROWSER_NET_CHROME_NETWORK_DELEGATE_H_
#pragma once

#include "base/basictypes.h"
#include "net/base/network_delegate.h"

class ExtensionIOEventRouter;

// ChromeNetworkDelegate is the central point from within the chrome code to
// add hooks into the network stack.
class ChromeNetworkDelegate : public net::NetworkDelegate {
 public:
  explicit ChromeNetworkDelegate(
      ExtensionIOEventRouter* extension_io_event_router);
  ~ChromeNetworkDelegate();

 private:
  // NetworkDelegate methods:
  virtual void OnBeforeURLRequest(net::URLRequest* request);
  virtual void OnSendHttpRequest(net::HttpRequestHeaders* headers);
  virtual void OnResponseStarted(net::URLRequest* request);
  virtual void OnReadCompleted(net::URLRequest* request, int bytes_read);

  ExtensionIOEventRouter* const extension_io_event_router_;
  DISALLOW_COPY_AND_ASSIGN(ChromeNetworkDelegate);
};

#endif  // CHROME_BROWSER_NET_CHROME_NETWORK_DELEGATE_H_
