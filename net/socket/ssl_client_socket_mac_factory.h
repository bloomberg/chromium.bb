// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SSL_CLIENT_SOCKET_MAC_FACTORY_H_
#define NET_SOCKET_SSL_CLIENT_SOCKET_MAC_FACTORY_H_
#pragma once

#include "net/socket/client_socket_factory.h"

namespace net {

// Creates SSLClientSocketMac objects.
SSLClientSocket* SSLClientSocketMacFactory(
    ClientSocketHandle* transport_socket,
    const std::string& hostname,
    const SSLConfig& ssl_config);

}  // namespace net

#endif  // NET_SOCKET_SSL_CLIENT_SOCKET_MAC_FACTORY_H_
