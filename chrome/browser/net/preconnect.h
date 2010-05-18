// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A Preconnect instance maintains state while a TCP/IP connection is made, and
// and then released into the pool of available connections for future use.

#ifndef CHROME_BROWSER_NET_PRECONNECT_H_
#define CHROME_BROWSER_NET_PRECONNECT_H_

#include "base/ref_counted.h"
#include "net/base/completion_callback.h"
#include "net/base/host_port_pair.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/tcp_client_socket_pool.h"
#include "net/url_request/url_request_context.h"

namespace chrome_browser_net {

class Preconnect : public net::CompletionCallback {
 public:
  static bool PreconnectOnUIThread(const net::HostPortPair& hostport);

  static void PreconnectOnIOThread(const net::HostPortPair& hostport);

 private:
  Preconnect() {}

  // Supply an instance that could have been used in an IO callback, but will
  // never actually be used (because we reset the connection so quickly).
  static Preconnect* callback_instance_;

  // IO Callback which whould be performed when the connection is established.
  virtual void RunWithParams(const Tuple1<int>& params);

  DISALLOW_COPY_AND_ASSIGN(Preconnect);
};
}  // chrome_browser_net

#endif  // CHROME_BROWSER_NET_PRECONNECT_H_
