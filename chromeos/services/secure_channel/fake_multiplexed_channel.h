// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_MULTIPLEXED_CHANNEL_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_MULTIPLEXED_CHANNEL_H_

#include <utility>
#include <vector>

#include "base/macros.h"
#include "chromeos/services/secure_channel/multiplexed_channel.h"
#include "chromeos/services/secure_channel/public/mojom/secure_channel.mojom.h"

namespace chromeos {

namespace secure_channel {

// Test MultiplexedChannel implementation.
class FakeMultiplexedChannel : public MultiplexedChannel {
 public:
  FakeMultiplexedChannel(Delegate* delegate);
  ~FakeMultiplexedChannel() override;

  std::vector<std::pair<std::string, mojom::ConnectionDelegatePtr>>&
  added_clients() {
    return added_clients_;
  }

  void SetDisconnecting();
  void SetDisconnected();

  // Make NotifyDisconnected() public for testing.
  using MultiplexedChannel::NotifyDisconnected;

 private:
  // MultiplexedChannel:
  bool IsDisconnecting() override;
  bool IsDisconnected() override;
  void PerformAddClientToChannel(
      const std::string& feature,
      mojom::ConnectionDelegatePtr connection_delegate_ptr) override;

  bool is_disconnecting_ = false;
  bool is_disconnected_ = false;

  std::vector<std::pair<std::string, mojom::ConnectionDelegatePtr>>
      added_clients_;

  DISALLOW_COPY_AND_ASSIGN(FakeMultiplexedChannel);
};

// Test MultiplexedChannel::Delegate implementation.
class FakeMultiplexedChannelDelegate : public MultiplexedChannel::Delegate {
 public:
  FakeMultiplexedChannelDelegate();
  ~FakeMultiplexedChannelDelegate() override;

  // Returns an empty token if no channel has yet been disconnected.
  const base::UnguessableToken& disconnected_channel_id() {
    return disconnected_channel_id_;
  }

 private:
  void OnDisconnected(const base::UnguessableToken& channel_id) override;

  base::UnguessableToken disconnected_channel_id_;

  DISALLOW_COPY_AND_ASSIGN(FakeMultiplexedChannelDelegate);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_MULTIPLEXED_CHANNEL_H_
