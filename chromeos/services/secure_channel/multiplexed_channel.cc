// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/multiplexed_channel.h"

#include "base/guid.h"
#include "base/logging.h"

namespace chromeos {

namespace secure_channel {

MultiplexedChannel::MultiplexedChannel(Delegate* delegate)
    : delegate_(delegate), channel_id_(base::UnguessableToken::Create()) {
  DCHECK(delegate);
}

MultiplexedChannel::~MultiplexedChannel() = default;

bool MultiplexedChannel::AddClientToChannel(
    const std::string& feature,
    mojom::ConnectionDelegatePtr connection_delegate_ptr) {
  if (IsDisconnecting() || IsDisconnected())
    return false;

  PerformAddClientToChannel(feature, std::move(connection_delegate_ptr));
  return true;
}

void MultiplexedChannel::NotifyDisconnected() {
  delegate_->OnDisconnected(channel_id_);
}

}  // namespace secure_channel

}  // namespace chromeos
