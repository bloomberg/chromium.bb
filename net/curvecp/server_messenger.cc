// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/curvecp/server_messenger.h"

namespace net {

ServerMessenger::ServerMessenger(Packetizer* packetizer,
                                 Acceptor* acceptor)
    : Messenger(packetizer),
      acceptor_(acceptor) {
  DCHECK(acceptor_);
}

ServerMessenger::ServerMessenger(ConnectionKey key,
                                 Packetizer* packetizer,
                                 Acceptor* acceptor)
    : Messenger(packetizer),
      acceptor_(acceptor) {
  DCHECK(acceptor_);
  key_ = key;
}

ServerMessenger::~ServerMessenger() {
}

void ServerMessenger::OnConnection(ConnectionKey key) {
  acceptor_->OnAccept(key);
}

}  // namespace net
