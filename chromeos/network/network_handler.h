// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_NETWORK_HANDLER_H_
#define CHROMEOS_NETWORK_NETWORK_HANDLER_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "chromeos/chromeos_export.h"

namespace chromeos {

class ClientCertResolver;
class GeolocationHandler;
class ManagedNetworkConfigurationHandler;
class ManagedNetworkConfigurationHandlerImpl;
class NetworkActivationHandler;
class NetworkCertMigrator;
class NetworkConfigurationHandler;
class NetworkConnectionHandler;
class NetworkDeviceHandler;
class NetworkDeviceHandlerImpl;
class NetworkProfileHandler;
class NetworkStateHandler;
class NetworkSmsHandler;

// Class for handling initialization and access to chromeos network handlers.
// This class should NOT be used in unit tests. Instead, construct individual
// classes independently.
class CHROMEOS_EXPORT NetworkHandler {
 public:
  // Sets the global instance. Must be called before any calls to Get().
  static void Initialize();

  // Destroys the global instance.
  static void Shutdown();

  // Gets the global instance. Initialize() must be called first.
  static NetworkHandler* Get();

  // Returns true if the global instance has been initialized.
  static bool IsInitialized();

  // Returns the MessageLoopProxy for posting NetworkHandler calls from
  // other threads.
  base::MessageLoopProxy* message_loop() { return message_loop_.get(); }

  // Do not use these accessors within this module; all dependencies should be
  // explicit so that classes can be constructed explicitly in tests without
  // NetworkHandler.
  NetworkStateHandler* network_state_handler();
  NetworkDeviceHandler* network_device_handler();
  NetworkProfileHandler* network_profile_handler();
  NetworkConfigurationHandler* network_configuration_handler();
  ManagedNetworkConfigurationHandler* managed_network_configuration_handler();
  NetworkActivationHandler* network_activation_handler();
  NetworkConnectionHandler* network_connection_handler();
  NetworkSmsHandler* network_sms_handler();
  GeolocationHandler* geolocation_handler();

 private:
  NetworkHandler();
  virtual ~NetworkHandler();

  void Init();

  // The order of these determines the (inverse) destruction order.
  scoped_refptr<base::MessageLoopProxy> message_loop_;
  scoped_ptr<NetworkStateHandler> network_state_handler_;
  scoped_ptr<NetworkDeviceHandlerImpl> network_device_handler_;
  scoped_ptr<NetworkProfileHandler> network_profile_handler_;
  scoped_ptr<NetworkConfigurationHandler> network_configuration_handler_;
  scoped_ptr<ManagedNetworkConfigurationHandlerImpl>
      managed_network_configuration_handler_;
  scoped_ptr<NetworkCertMigrator> network_cert_migrator_;
  scoped_ptr<ClientCertResolver> client_cert_resolver_;
  scoped_ptr<NetworkActivationHandler> network_activation_handler_;
  scoped_ptr<NetworkConnectionHandler> network_connection_handler_;
  scoped_ptr<NetworkSmsHandler> network_sms_handler_;
  scoped_ptr<GeolocationHandler> geolocation_handler_;

  DISALLOW_COPY_AND_ASSIGN(NetworkHandler);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_NETWORK_HANDLER_H_
