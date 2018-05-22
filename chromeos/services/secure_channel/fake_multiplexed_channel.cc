// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/fake_multiplexed_channel.h"

namespace chromeos {

namespace secure_channel {

FakeMultiplexedChannel::FakeMultiplexedChannel(Delegate* delegate)
    : MultiplexedChannel(delegate) {}

FakeMultiplexedChannel::~FakeMultiplexedChannel() = default;

void FakeMultiplexedChannel::SetDisconnecting() {
  DCHECK(!is_disconnected_);
  DCHECK(!is_disconnecting_);
  is_disconnecting_ = true;
}

void FakeMultiplexedChannel::SetDisconnected() {
  DCHECK(!is_disconnected_);
  is_disconnecting_ = false;
  is_disconnected_ = true;
}

bool FakeMultiplexedChannel::IsDisconnecting() {
  return is_disconnecting_;
}

bool FakeMultiplexedChannel::IsDisconnected() {
  return is_disconnected_;
}

void FakeMultiplexedChannel::PerformAddClientToChannel(
    const std::string& feature,
    mojom::ConnectionDelegatePtr connection_delegate_ptr) {
  added_clients_.push_back(
      std::make_pair(feature, std::move(connection_delegate_ptr)));
}

FakeMultiplexedChannelDelegate::FakeMultiplexedChannelDelegate() = default;

FakeMultiplexedChannelDelegate::~FakeMultiplexedChannelDelegate() = default;

void FakeMultiplexedChannelDelegate::OnDisconnected(
    const base::UnguessableToken& channel_id) {
  disconnected_channel_id_ = channel_id;
}

}  // namespace secure_channel

}  // namespace chromeos
