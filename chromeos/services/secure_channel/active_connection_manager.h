// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_ACTIVE_CONNECTION_MANAGER_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_ACTIVE_CONNECTION_MANAGER_H_

#include <ostream>
#include <string>
#include <vector>

#include "base/macros.h"
#include "chromeos/services/secure_channel/client_connection_parameters.h"
#include "chromeos/services/secure_channel/connection_details.h"
#include "chromeos/services/secure_channel/public/mojom/secure_channel.mojom.h"

namespace chromeos {

namespace secure_channel {

class AuthenticatedChannel;

// Manages zero or more active connections to remote devices. Each connection
// can be shared among one or more clients so that the underlying resources for
// the connection to not need to be duplicated.
class ActiveConnectionManager {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void OnDisconnected(
        const ConnectionDetails& connection_details) = 0;
  };

  enum class ConnectionState {
    kActiveConnectionExists,
    kNoConnectionExists,
    kDisconnectingConnectionExists
  };

  virtual ~ActiveConnectionManager();

  virtual ConnectionState GetConnectionState(
      const ConnectionDetails& connection_details) const = 0;

  // Adds an active connection to be managed. A connection can only be added if
  // GetConnectionState() returns kNoConnectionExists.
  void AddActiveConnection(
      std::unique_ptr<AuthenticatedChannel> authenticated_channel,
      std::vector<std::unique_ptr<ClientConnectionParameters>> initial_clients,
      const ConnectionDetails& connection_details);

  // Adds a client to an active connection. A client can only be added if
  // GetConnectionState() returns kActiveConnectionExists.
  void AddClientToChannel(
      std::unique_ptr<ClientConnectionParameters> client_connection_parameters,
      const ConnectionDetails& connection_details);

 protected:
  ActiveConnectionManager(Delegate* delegate);

  // Actually adds the provided connection. By the time this function is called,
  // it has already been verified that there is no existing connection.
  virtual void PerformAddActiveConnection(
      std::unique_ptr<AuthenticatedChannel> authenticated_channel,
      std::vector<std::unique_ptr<ClientConnectionParameters>> initial_clients,
      const ConnectionDetails& connection_details) = 0;

  // Actually adds the provided client/feature pair. By the time this function
  // is called, it has already been verified that there an active connection
  // exists.
  virtual void PerformAddClientToChannel(
      std::unique_ptr<ClientConnectionParameters> client_connection_parameters,
      const ConnectionDetails& connection_details) = 0;

  void OnChannelDisconnected(const ConnectionDetails& connection_details);

 private:
  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(ActiveConnectionManager);
};

std::ostream& operator<<(
    std::ostream& stream,
    const ActiveConnectionManager::ConnectionState& connection_state);

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_ACTIVE_CONNECTION_MANAGER_H_
