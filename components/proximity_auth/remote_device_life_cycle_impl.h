// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_REMOTE_DEVICE_LIFE_CYCLE_IMPL_H
#define COMPONENTS_PROXIMITY_AUTH_REMOTE_DEVICE_LIFE_CYCLE_IMPL_H

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "components/proximity_auth/authenticator.h"
#include "components/proximity_auth/messenger_observer.h"
#include "components/proximity_auth/remote_device.h"
#include "components/proximity_auth/remote_device_life_cycle.h"

namespace proximity_auth {

class BluetoothThrottler;
class BluetoothLowEnergyDeviceWhitelist;
class Messenger;
class Connection;
class ConnectionFinder;
class ProximityAuthClient;
class SecureContext;

// Implementation of RemoteDeviceLifeCycle.
class RemoteDeviceLifeCycleImpl : public RemoteDeviceLifeCycle,
                                  public MessengerObserver {
 public:
  // Creates the life cycle for controlling the given |remote_device|.
  // |proximity_auth_client| is not owned.
  RemoteDeviceLifeCycleImpl(const RemoteDevice& remote_device,
                            ProximityAuthClient* proximity_auth_client);
  ~RemoteDeviceLifeCycleImpl() override;

  // RemoteDeviceLifeCycle:
  void Start() override;
  RemoteDevice GetRemoteDevice() const override;
  RemoteDeviceLifeCycle::State GetState() const override;
  Messenger* GetMessenger() override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

 protected:
  // Creates and returns a ConnectionFinder instance for |remote_device_|.
  // Exposed for testing.
  virtual std::unique_ptr<ConnectionFinder> CreateConnectionFinder();

  // Creates and returns an Authenticator instance for |connection_|.
  // Exposed for testing.
  virtual std::unique_ptr<Authenticator> CreateAuthenticator();

 private:
  // Transitions to |new_state|, and notifies observers.
  void TransitionToState(RemoteDeviceLifeCycle::State new_state);

  // Transtitions to FINDING_CONNECTION state. Creates and starts
  // |connection_finder_|.
  void FindConnection();

  // Called when |connection_finder_| finds a connection.
  void OnConnectionFound(std::unique_ptr<Connection> connection);

  // Callback when |authenticator_| completes authentication.
  void OnAuthenticationResult(Authenticator::Result result,
                              std::unique_ptr<SecureContext> secure_context);

  // Creates the messenger which parses status updates.
  void CreateMessenger();

  // MessengerObserver:
  void OnDisconnected() override;

  // The remote device being controlled.
  const RemoteDevice remote_device_;

  // Used to grab dependencies in chrome. Not owned.
  ProximityAuthClient* proximity_auth_client_;

  // The current state in the life cycle.
  RemoteDeviceLifeCycle::State state_;

  // Observers added to the life cycle. Configured as NOTIFY_EXISTING_ONLY.
  base::ObserverList<Observer> observers_;

  // The connection that is established by |connection_finder_|.
  std::unique_ptr<Connection> connection_;

  // Context for encrypting and decrypting messages. Created after
  // authentication succeeds. Ownership is eventually passed to |messenger_|.
  std::unique_ptr<SecureContext> secure_context_;

  // The messenger for sending and receiving messages in the
  // SECURE_CHANNEL_ESTABLISHED state.
  std::unique_ptr<Messenger> messenger_;

  // Authenticates the remote device after it is connected. Used in the
  // AUTHENTICATING state.
  std::unique_ptr<Authenticator> authenticator_;

  // Used in the FINDING_CONNECTION state to establish a connection to the
  // remote device.
  std::unique_ptr<ConnectionFinder> connection_finder_;

  // Rate limits Bluetooth connections to the same device. Used to in the
  // created ConnectionFinder.
  std::unique_ptr<BluetoothThrottler> bluetooth_throttler_;

  // After authentication fails, this timer waits for a period of time before
  // retrying the connection.
  base::OneShotTimer authentication_recovery_timer_;

  base::WeakPtrFactory<RemoteDeviceLifeCycleImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RemoteDeviceLifeCycleImpl);
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_REMOTE_DEVICE_LIFE_CYCLE_IMPL_H
