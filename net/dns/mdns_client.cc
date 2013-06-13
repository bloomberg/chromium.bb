// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/mdns_client.h"

#include "net/dns/mdns_client_impl.h"

namespace net {

static MDnsClient* g_instance = NULL;

MDnsClient* MDnsClient::GetInstance() {
  if (!g_instance) {
    g_instance =
        new MDnsClientImpl(MDnsConnection::SocketFactory::CreateDefault());
  }

  return g_instance;
}

void MDnsClient::SetInstance(MDnsClient* instance) {
  g_instance = instance;
}

}  // namespace net
