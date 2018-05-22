// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_MULTIPLEXED_CHANNEL_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_MULTIPLEXED_CHANNEL_H_

#include "base/macros.h"
#include "base/unguessable_token.h"
#include "chromeos/services/secure_channel/public/mojom/secure_channel.mojom.h"

namespace chromeos {

namespace secure_channel {

// Full-duplex communication channel which is shared between one or more
// clients. Messages received on the channel are passed to each client for
// processing, and messages sent by one client are delivered over the underlying
// connection. The channel stays active until either the remote device triggers
// its disconnection or all clients disconnect.
class MultiplexedChannel {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void OnDisconnected(const base::UnguessableToken& channel_id) = 0;
  };

  virtual ~MultiplexedChannel();

  virtual bool IsDisconnecting() = 0;
  virtual bool IsDisconnected() = 0;

  // Shares this channel with an additional client. Returns whether this action
  // was successful; all calls are expected to succeed unless the channel is
  // disconnected or disconnecting.
  bool AddClientToChannel(const std::string& feature,
                          mojom::ConnectionDelegatePtr connection_delegate_ptr);

  const base::UnguessableToken& channel_id() { return channel_id_; }

 protected:
  MultiplexedChannel(Delegate* delegate);

  virtual void PerformAddClientToChannel(
      const std::string& feature,
      mojom::ConnectionDelegatePtr connection_delegate_ptr) = 0;

  void NotifyDisconnected();

 private:
  Delegate* delegate_;
  const base::UnguessableToken channel_id_;

  DISALLOW_COPY_AND_ASSIGN(MultiplexedChannel);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_MULTIPLEXED_CHANNEL_H_
