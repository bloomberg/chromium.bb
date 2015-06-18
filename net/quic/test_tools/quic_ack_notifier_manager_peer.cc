// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/test_tools/quic_ack_notifier_manager_peer.h"

#include "net/quic/quic_ack_notifier_manager.h"

namespace net {
namespace test {

size_t AckNotifierManagerPeer::GetNumberOfRegisteredPackets(
    const AckNotifierManager* manager) {
  return manager->ack_notifier_map_.size();
}

}  // namespace test
}  // namespace net
