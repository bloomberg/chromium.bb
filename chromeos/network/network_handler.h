// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_NETWORK_HANDLER_H_
#define CHROMEOS_NETWORK_NETWORK_HANDLER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chromeos/chromeos_export.h"

namespace chromeos {

class CertLoader;
class GeolocationHandler;
class ManagedNetworkConfigurationHandler;
class NetworkConfigurationHandler;
class NetworkConnectionHandler;
class NetworkProfileHandler;
class NetworkStateHandler;

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

  // Do not use these accessors within this module; all dependencies should be
  // explicit so that classes can be constructed explicitly in tests without
  // NetworkHandler.
  CertLoader* cert_loader();
  NetworkStateHandler* network_state_handler();
  NetworkProfileHandler* network_profile_handler();
  NetworkConfigurationHandler* network_configuration_handler();
  ManagedNetworkConfigurationHandler* managed_network_configuration_handler();
  NetworkConnectionHandler* network_connection_handler();
  GeolocationHandler* geolocation_handler();

 private:
  NetworkHandler();
  virtual ~NetworkHandler();

  void Init();

  // The order of these determines the (inverse) destruction order.
  scoped_ptr<CertLoader> cert_loader_;
  scoped_ptr<NetworkStateHandler> network_state_handler_;
  scoped_ptr<NetworkProfileHandler> network_profile_handler_;
  scoped_ptr<NetworkConfigurationHandler> network_configuration_handler_;
  scoped_ptr<ManagedNetworkConfigurationHandler>
      managed_network_configuration_handler_;
  scoped_ptr<NetworkConnectionHandler> network_connection_handler_;
  scoped_ptr<GeolocationHandler> geolocation_handler_;

  DISALLOW_COPY_AND_ASSIGN(NetworkHandler);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_NETWORK_HANDLER_H_
