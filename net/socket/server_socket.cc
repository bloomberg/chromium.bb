// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/server_socket.h"

#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"

namespace net {

ServerSocket::ServerSocket() {
}

ServerSocket::~ServerSocket() {
}

int ServerSocket::ListenWithAddressAndPort(const std::string& address_string,
                                           int port,
                                           int backlog) {
  IPAddressNumber address_number;
  if (!ParseIPLiteralToNumber(address_string, &address_number)) {
    return ERR_ADDRESS_INVALID;
  }

  return Listen(IPEndPoint(address_number, port), backlog);
}

}  // namespace net
