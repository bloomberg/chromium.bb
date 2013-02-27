// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NET_MANAGED_NETWORK_CONFIGURATION_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_NET_MANAGED_NETWORK_CONFIGURATION_HANDLER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/network/network_handler_callbacks.h"

namespace base {
class DictionaryValue;
}

namespace chromeos {

// The ManagedNetworkConfigurationHandler class is used to create and configure
// networks in ChromeOS using ONC.
//
// Its interface exposes only ONC and should decouple users from Shill.
// Internally it translates ONC to Shill dictionaries and calls through to the
// NetworkConfigurationHandler.
//
// For accessing lists of visible networks, and other state information, see the
// class NetworkStateHandler.
//
// This is a singleton and its lifetime is managed by the Chrome startup code.
//
// Network configurations are referred to by Shill's service path. These
// identifiers should at most be used to also access network state using the
// NetworkStateHandler, but dependencies to Shill should be avoided. In the
// future, we may switch to other identifiers.
//
// Note on callbacks: Because all the functions here are meant to be
// asynchronous, they all take a |callback| of some type, and an
// |error_callback|. When the operation succeeds, |callback| will be called, and
// when it doesn't, |error_callback| will be called with information about the
// error, including a symbolic name for the error and often some error message
// that is suitable for logging. None of the error message text is meant for
// user consumption.
//
// TODO(pneubeck): Enforce network policies.

class ManagedNetworkConfigurationHandler {
 public:
  // Initializes the singleton.
  static void Initialize();

  // Returns if the singleton is initialized.
  static bool IsInitialized();

  // Destroys the singleton.
  static void Shutdown();

  // Initialize() must be called before this.
  static ManagedNetworkConfigurationHandler* Get();

  // Provides the properties of the network with |service_path| to |callback|.
  void GetProperties(
      const std::string& service_path,
      const network_handler::DictionaryResultCallback& callback,
      const network_handler::ErrorCallback& error_callback) const;

  // Sets the user's settings of an already configured network with
  // |service_path|. A network can be initially configured by calling
  // CreateConfiguration or if it is managed by a policy. The given properties
  // will be merged with the existing settings, and it won't clear any existing
  // properties.
  void SetProperties(
      const std::string& service_path,
      const base::DictionaryValue& properties,
      const base::Closure& callback,
      const network_handler::ErrorCallback& error_callback) const;

  // Initiates a connection with network that has |service_path|. |callback| is
  // called if the connection request was successfully handled. That doesn't
  // mean that the connection was successfully established.
  void Connect(const std::string& service_path,
               const base::Closure& callback,
               const network_handler::ErrorCallback& error_callback) const;

  // Initiates a disconnect with the network at |service_path|. |callback| is
  // called if the diconnect request was successfully handled. That doesn't mean
  // that the network is already diconnected.
  void Disconnect(const std::string& service_path,
                  const base::Closure& callback,
                  const network_handler::ErrorCallback& error_callback) const;

  // Initially configures an unconfigured network with the given user settings
  // and returns the new identifier to |callback| if successful. Fails if the
  // network was already configured by a call to this function or because of a
  // policy.
  void CreateConfiguration(
      const base::DictionaryValue& properties,
      const network_handler::StringResultCallback& callback,
      const network_handler::ErrorCallback& error_callback) const;

  // Removes the user's configuration from the network with |service_path|. The
  // network may still show up in the visible networks after this, but no user
  // configuration will remain. If it was managed, it will still be configured.
  void RemoveConfiguration(
      const std::string& service_path,
      const base::Closure& callback,
      const network_handler::ErrorCallback& error_callback) const;

 private:
  ManagedNetworkConfigurationHandler();
  ~ManagedNetworkConfigurationHandler();

  DISALLOW_COPY_AND_ASSIGN(ManagedNetworkConfigurationHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NET_MANAGED_NETWORK_CONFIGURATION_HANDLER_H_
