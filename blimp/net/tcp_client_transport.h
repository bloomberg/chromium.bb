// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_TCP_CLIENT_TRANSPORT_H_
#define BLIMP_NET_TCP_CLIENT_TRANSPORT_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "blimp/net/blimp_transport.h"
#include "net/base/address_list.h"
#include "net/base/net_errors.h"

namespace net {
class NetLog;
}  // namespace net

namespace blimp {

class BlimpConnection;

// BlimpTransport which creates a TCP connection to one of the specified
// |addresses| on each call to Connect().
class TCPClientTransport : public BlimpTransport {
 public:
  TCPClientTransport(const net::AddressList& addresses, net::NetLog* net_log);
  ~TCPClientTransport() override;

  // BlimpTransport implementation.
  int Connect(const net::CompletionCallback& callback) override;
  scoped_ptr<BlimpConnection> TakeConnection() override;

 private:
  void OnTCPConnectComplete(int result);

  net::AddressList addresses_;
  net::NetLog* net_log_;
  scoped_ptr<BlimpConnection> connection_;
  net::CompletionCallback connect_callback_;

  DISALLOW_COPY_AND_ASSIGN(TCPClientTransport);
};

}  // namespace blimp

#endif  // BLIMP_NET_TCP_CLIENT_TRANSPORT_H_
