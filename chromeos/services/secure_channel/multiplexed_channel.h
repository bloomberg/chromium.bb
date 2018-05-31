// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_MULTIPLEXED_CHANNEL_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_MULTIPLEXED_CHANNEL_H_

#include <memory>

#include "base/macros.h"
#include "chromeos/services/secure_channel/client_connection_parameters.h"
#include "chromeos/services/secure_channel/connection_details.h"
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
    virtual void OnDisconnected(
        const ConnectionDetails& connection_details) = 0;
  };

  virtual ~MultiplexedChannel();

  virtual bool IsDisconnecting() const = 0;
  virtual bool IsDisconnected() const = 0;

  // Shares this channel with an additional client. Returns whether this action
  // was successful; all calls are expected to succeed unless the channel is
  // disconnected or disconnecting.
  bool AddClientToChannel(
      std::unique_ptr<ClientConnectionParameters> client_connection_parameters);

  const ConnectionDetails& connection_details() { return connection_details_; }

 protected:
  MultiplexedChannel(Delegate* delegate, ConnectionDetails connection_details);

  virtual void PerformAddClientToChannel(
      std::unique_ptr<ClientConnectionParameters>
          client_connection_parameters) = 0;

  void NotifyDisconnected();

 private:
  Delegate* delegate_;
  const ConnectionDetails connection_details_;

  DISALLOW_COPY_AND_ASSIGN(MultiplexedChannel);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_MULTIPLEXED_CHANNEL_H_
