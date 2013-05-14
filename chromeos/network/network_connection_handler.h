// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_NETWORK_CONNECTION_HANDLER_H_
#define CHROMEOS_NETWORK_NETWORK_CONNECTION_HANDLER_H_

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/network/network_handler_callbacks.h"

namespace chromeos {

class NetworkState;

// The NetworkConnectionHandler class is used to manage network connection
// requests. This is the only class that should make Shill Connect calls.
// It handles the following steps:
// 1. Determine whether or not sufficient information (e.g. passphrase) is
//    known to be available to connect to the network.
// 2. Request additional information (e.g. user data which contains certificate
//    information) and determine whether sufficient information is available.
// 3. Send the connect request.
// 4. Invoke the appropriate callback (always) on success or failure.
//
// NetworkConnectionHandler depends on NetworkStateHandler for immediately
// available State information, and NetworkConfigurationHandler for any
// configuration calls.

class CHROMEOS_EXPORT NetworkConnectionHandler
    : public base::SupportsWeakPtr<NetworkConnectionHandler> {
 public:
  // Constants for |error_name| from |error_callback| for Connect/Disconnect.
  static const char kErrorNotFound[];
  static const char kErrorConnected[];
  static const char kErrorConnecting[];
  static const char kErrorNotConnected[];
  static const char kErrorPassphraseRequired[];
  static const char kErrorActivationRequired[];
  static const char kErrorCertificateRequired[];
  static const char kErrorConfigurationRequired[];
  static const char kErrorShillError[];

  // Sets the global instance. Must be called before any calls to Get().
  static void Initialize();

  // Destroys the global instance.
  static void Shutdown();

  // Gets the global instance. Initialize() must be called first.
  static NetworkConnectionHandler* Get();

  // ConnectToNetwork() will start an asynchronous connection attempt.
  // On success, |success_callback| will be called.
  // On failure, |error_callback| will be called with |error_name| one of:
  //  kErrorNotFound if no network matching |service_path| is found
  //    (hidden networks must be configured before connecting).
  //  kErrorConnected if already connected to the network.
  //  kErrorConnecting if already connecting to the network.
  //  kErrorCertificateRequired if the network requires a cert and none exists.
  //  kErrorPassphraseRequired if passphrase only is required.
  //  kErrorConfigurationRequired if additional configuration is required.
  //  kErrorShillError if a DBus or Shill error occurred.
  // |error_message| will contain an additional error string for debugging.
  void ConnectToNetwork(const std::string& service_path,
                        const base::Closure& success_callback,
                        const network_handler::ErrorCallback& error_callback);

  // DisconnectToNetwork() will send a Disconnect request to Shill.
  // On success, |success_callback| will be called.
  // On failure, |error_callback| will be called with |error_name| one of:
  //  kErrorNotFound if no network matching |service_path| is found.
  //  kErrorNotConnected if not connected to the network.
  //  kErrorShillError if a DBus or Shill error occurred.
  // |error_message| will contain and additional error string for debugging.
  void DisconnectNetwork(const std::string& service_path,
                         const base::Closure& success_callback,
                         const network_handler::ErrorCallback& error_callback);

 private:
  NetworkConnectionHandler();
  ~NetworkConnectionHandler();

  // Calls Shill.Manager.Connect asynchronously.
  void CallShillConnect(
      const std::string& service_path,
      const base::Closure& success_callback,
      const network_handler::ErrorCallback& error_callback);

  // Calls Shill.Manager.Disconnect asynchronously.
  void CallShillDisconnect(
      const std::string& service_path,
      const base::Closure& success_callback,
      const network_handler::ErrorCallback& error_callback);

  // Callback from Shill.Service.GetProperties. Parses |properties| to verify
  // whether or not the network appears to be configured. If configured,
  // attempts a connection, otherwise invokes |error_callback|.
  void VerifyConfiguredAndConnect(
      const base::Closure& success_callback,
      const network_handler::ErrorCallback& error_callback,
      const std::string& service_path,
      const base::DictionaryValue& properties);

  // Sets the property for the service with an empty callback (logs errors).
  void SetServiceProperty(const std::string& service_path,
                          const std::string& property,
                          const std::string& value) const;

  // Handle failure from ConfigurationHandler calls.
  void HandleConfigurationFailure(
      const std::string& service_path,
      const network_handler::ErrorCallback& error_callback,
      const std::string& error_name,
      scoped_ptr<base::DictionaryValue> error_data);

  // Handle success or failure from Shill.Service.Connect.
  void HandleShillSuccess(const std::string& service_path,
                          const base::Closure& success_callback);
  void HandleShillFailure(const std::string& service_path,
                          const network_handler::ErrorCallback& error_callback,
                          const std::string& error_name,
                          const std::string& error_message);

  // Set of pending connect requests, used to prevent repeat attempts while
  // waiting for Shill.
  std::set<std::string> pending_requests_;

  DISALLOW_COPY_AND_ASSIGN(NetworkConnectionHandler);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_NETWORK_CONNECTION_HANDLER_H_
