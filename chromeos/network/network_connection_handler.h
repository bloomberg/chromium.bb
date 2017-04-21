// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_NETWORK_CONNECTION_HANDLER_H_
#define CHROMEOS_NETWORK_NETWORK_CONNECTION_HANDLER_H_

#include <map>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/cert_loader.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/login/login_state.h"
#include "chromeos/network/network_connection_observer.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_handler_callbacks.h"
#include "chromeos/network/network_state_handler_observer.h"

namespace chromeos {

class NetworkState;

// The NetworkConnectionHandler class is used to manage network connection
// requests. This is the only class that should make Shill Connect calls.
// It handles the following steps:
// 1. Determine whether or not sufficient information (e.g. passphrase) is
//    known to be available to connect to the network.
// 2. Request additional information (e.g. user data which contains certificate
//    information) and determine whether sufficient information is available.
// 3. Possibly configure the network certificate info (tpm slot and pkcs11 id).
// 4. Send the connect request.
// 5. Wait for the network state to change to a non connecting state.
// 6. Invoke the appropriate callback (always) on success or failure.
//
// NetworkConnectionHandler depends on NetworkStateHandler for immediately
// available State information, and NetworkConfigurationHandler for any
// configuration calls.

class CHROMEOS_EXPORT NetworkConnectionHandler
    : public LoginState::Observer,
      public CertLoader::Observer,
      public NetworkStateHandlerObserver,
      public base::SupportsWeakPtr<NetworkConnectionHandler> {
 public:
  // Constants for |error_name| from |error_callback| for Connect.

  //  No network matching |service_path| is found (hidden networks must be
  //  configured before connecting).
  static const char kErrorNotFound[];

  // Already connected to the network.
  static const char kErrorConnected[];

  // Already connecting to the network.
  static const char kErrorConnecting[];

  // The passphrase is missing or invalid.
  static const char kErrorPassphraseRequired[];

  // The passphrase is incorrect.
  static const char kErrorBadPassphrase[];

  // The network requires a cert and none exists.
  static const char kErrorCertificateRequired[];

  // The network had an authentication error, indicating that additional or
  // different authentication information is required.
  static const char kErrorAuthenticationRequired[];

  // Additional configuration is required.
  static const char kErrorConfigurationRequired[];

  // Configuration failed during the configure stage of the connect flow.
  static const char kErrorConfigureFailed[];

  // An unexpected DBus or Shill error occurred while connecting.
  static const char kErrorConnectFailed[];

  // An unexpected DBus or Shill error occurred while disconnecting.
  static const char kErrorDisconnectFailed[];

  // A new network connect request canceled this one.
  static const char kErrorConnectCanceled[];

  // Constants for |error_name| from |error_callback| for Disconnect.
  static const char kErrorNotConnected[];

  // Certificate load timed out.
  static const char kErrorCertLoadTimeout[];

  // Trying to configure an unmanged network but policy prohibits that
  static const char kErrorUnmanagedNetwork[];

  // Network activation failed.
  static const char kErrorActivateFailed[];

  ~NetworkConnectionHandler() override;

  void AddObserver(NetworkConnectionObserver* observer);
  void RemoveObserver(NetworkConnectionObserver* observer);

  // ConnectToNetwork() will start an asynchronous connection attempt.
  // On success, |success_callback| will be called.
  // On failure, |error_callback| will be called with |error_name| one of the
  //   constants defined above.
  // |error_message| will contain an additional error string for debugging.
  // If |check_error_state| is true, the current state of the network is
  //  checked for errors, otherwise current state is ignored (e.g. for recently
  //  configured networks or repeat attempts).
  void ConnectToNetwork(const std::string& service_path,
                        const base::Closure& success_callback,
                        const network_handler::ErrorCallback& error_callback,
                        bool check_error_state);

  // DisconnectNetwork() will send a Disconnect request to Shill.
  // On success, |success_callback| will be called.
  // On failure, |error_callback| will be called with |error_name| one of:
  //  kErrorNotFound if no network matching |service_path| is found.
  //  kErrorNotConnected if not connected to the network.
  //  kErrorDisconnectFailed if a DBus or Shill error occurred.
  // |error_message| will contain and additional error string for debugging.
  void DisconnectNetwork(const std::string& service_path,
                         const base::Closure& success_callback,
                         const network_handler::ErrorCallback& error_callback);

  // Returns true if ConnectToNetwork has been called with |service_path| and
  // has not completed (i.e. success or error callback has been called).
  bool HasConnectingNetwork(const std::string& service_path);

  // Returns true if there are any pending connect requests.
  bool HasPendingConnectRequest();

  // NetworkStateHandlerObserver
  void NetworkListChanged() override;
  void NetworkPropertiesUpdated(const NetworkState* network) override;

  // LoginState::Observer
  void LoggedInStateChanged() override;

  // CertLoader::Observer
  void OnCertificatesLoaded(const net::CertificateList& cert_list,
                            bool initial_load) override;

 private:
  friend class NetworkHandler;
  friend class NetworkConnectionHandlerTest;

  struct ConnectRequest;

  NetworkConnectionHandler();

  void Init(NetworkStateHandler* network_state_handler,
            NetworkConfigurationHandler* network_configuration_handler,
            ManagedNetworkConfigurationHandler*
                managed_network_configuration_handler);

  ConnectRequest* GetPendingRequest(const std::string& service_path);

  // Callback from Shill.Service.GetProperties. Parses |properties| to verify
  // whether or not the network appears to be configured. If configured,
  // attempts a connection, otherwise invokes error_callback from
  // pending_requests_[service_path]. |check_error_state| is passed from
  // ConnectToNetwork(), see comment for info.
  void VerifyConfiguredAndConnect(bool check_error_state,
                                  const std::string& service_path,
                                  const base::DictionaryValue& properties);

  bool IsNetworkProhibitedByPolicy(const std::string& type,
                                   const std::string& guid,
                                   const std::string& profile_path);

  // Queues a connect request until certificates have loaded.
  void QueueConnectRequest(const std::string& service_path);

  // Checks to see if certificates have loaded and if not, cancels any queued
  // connect request and notifies the user.
  void CheckCertificatesLoaded();

  // Handles connecting to a queued network after certificates are loaded or
  // handle cert load timeout.
  void ConnectToQueuedNetwork();

  // Calls Shill.Manager.Connect asynchronously.
  void CallShillConnect(const std::string& service_path);

  // Handles failure from ConfigurationHandler calls.
  void HandleConfigurationFailure(
      const std::string& service_path,
      const std::string& error_name,
      std::unique_ptr<base::DictionaryValue> error_data);

  // Handles success or failure from Shill.Service.Connect.
  void HandleShillConnectSuccess(const std::string& service_path);
  void HandleShillConnectFailure(const std::string& service_path,
                                 const std::string& error_name,
                                 const std::string& error_message);

  // Note: |service_path| is passed by value here, because in some cases
  // the value may be located in the map and then it can be deleted, producing
  // a reference to invalid memory.
  void CheckPendingRequest(const std::string service_path);

  void CheckAllPendingRequests();

  // Notify caller and observers that the connect request succeeded.
  void InvokeConnectSuccessCallback(const std::string& service_path,
                                    const base::Closure& success_callback);

  // Look up the ConnectRequest for |service_path| and call
  // InvokeConnectErrorCallback.
  void ErrorCallbackForPendingRequest(const std::string& service_path,
                                      const std::string& error_name);

  // Notify caller and observers that the connect request failed.
  // |error_name| will be one of the kError* messages defined above.
  void InvokeConnectErrorCallback(
      const std::string& service_path,
      const network_handler::ErrorCallback& error_callback,
      const std::string& error_name);

  // Calls Shill.Manager.Disconnect asynchronously.
  void CallShillDisconnect(
      const std::string& service_path,
      const base::Closure& success_callback,
      const network_handler::ErrorCallback& error_callback);

  // Handle success from Shill.Service.Disconnect.
  void HandleShillDisconnectSuccess(const std::string& service_path,
                                    const base::Closure& success_callback);

  base::ObserverList<NetworkConnectionObserver, true> observers_;

  // Local references to the associated handler instances.
  CertLoader* cert_loader_;
  NetworkStateHandler* network_state_handler_;
  NetworkConfigurationHandler* configuration_handler_;
  ManagedNetworkConfigurationHandler* managed_configuration_handler_;

  // Map of pending connect requests, used to prevent repeated attempts while
  // waiting for Shill and to trigger callbacks on eventual success or failure.
  std::map<std::string, ConnectRequest> pending_requests_;
  std::unique_ptr<ConnectRequest> queued_connect_;

  // Track certificate loading state.
  bool logged_in_;
  bool certificates_loaded_;
  base::TimeTicks logged_in_time_;

  DISALLOW_COPY_AND_ASSIGN(NetworkConnectionHandler);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_NETWORK_CONNECTION_HANDLER_H_
