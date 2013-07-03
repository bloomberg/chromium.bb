// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/mdns_client.h"

#include "net/dns/mdns_client_impl.h"

namespace net {

// static
scoped_ptr<MDnsClient> MDnsClient::CreateDefault() {
  return scoped_ptr<MDnsClient>(
      new MDnsClientImpl(MDnsConnection::SocketFactory::CreateDefault()));
}

}  // namespace net
