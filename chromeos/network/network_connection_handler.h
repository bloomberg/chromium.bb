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
#include "chromeos/cert_loader.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/login/login_state.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_handler_callbacks.h"
#include "chromeos/network/network_state_handler_observer.h"

namespace chromeos {

class NetworkState;
class NetworkUIData;

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
  static const char kErrorNotFound[];
  static const char kErrorConnected[];
  static const char kErrorConnecting[];
  static const char kErrorPassphraseRequired[];
  static const char kErrorActivationRequired[];
  static const char kErrorCertificateRequired[];
  static const char kErrorAuthenticationRequired[];
  static const char kErrorConfigurationRequired[];
  static const char kErrorShillError[];
  static const char kErrorConnectFailed[];
  static const char kErrorDisconnectFailed[];
  static const char kErrorMissingProviderType[];
  static const char kErrorUnknown[];

  // Constants for |error_name| from |error_callback| for Disconnect.
  static const char kErrorNotConnected[];

  virtual ~NetworkConnectionHandler();

  // ConnectToNetwork() will start an asynchronous connection attempt.
  // On success, |success_callback| will be called.
  // On failure, |error_callback| will be called with |error_name| one of:
  //  kErrorNotFound if no network matching |service_path| is found
  //    (hidden networks must be configured before connecting).
  //  kErrorConnected if already connected to the network.
  //  kErrorConnecting if already connecting to the network.
  //  kErrorCertificateRequired if the network requires a cert and none exists.
  //  kErrorPassphraseRequired if passphrase only is missing or incorrect.
  //  kErrorAuthenticationRequired if other authentication is required.
  //  kErrorConfigurationRequired if additional configuration is required.
  //  kErrorShillError if a DBus or Shill error occurred.
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
  //  kErrorShillError if a DBus or Shill error occurred.
  // |error_message| will contain and additional error string for debugging.
  void DisconnectNetwork(const std::string& service_path,
                         const base::Closure& success_callback,
                         const network_handler::ErrorCallback& error_callback);

  // Returns true if ConnectToNetwork has been called with |service_path| and
  // has not completed (i.e. success or error callback has been called).
  bool HasConnectingNetwork(const std::string& service_path);

  // NetworkStateHandlerObserver
  virtual void NetworkListChanged() OVERRIDE;
  virtual void NetworkPropertiesUpdated(const NetworkState* network) OVERRIDE;

  // LoginState::Observer
  virtual void LoggedInStateChanged(LoginState::LoggedInState state) OVERRIDE;

  // CertLoader::Observer
  virtual void OnCertificatesLoaded(const net::CertificateList& cert_list,
                                    bool initial_load) OVERRIDE;

 private:
  friend class NetworkHandler;
  friend class NetworkConnectionHandlerTest;

  struct ConnectRequest;

  NetworkConnectionHandler();

  void Init(NetworkStateHandler* network_state_handler,
            NetworkConfigurationHandler* network_configuration_handler);

  ConnectRequest* pending_request(const std::string& service_path);

  // Callback from Shill.Service.GetProperties. Parses |properties| to verify
  // whether or not the network appears to be configured. If configured,
  // attempts a connection, otherwise invokes error_callback from
  // pending_requests_[service_path]. |check_error_state| is passed from
  // ConnectToNetwork(), see comment for info.
  void VerifyConfiguredAndConnect(bool check_error_state,
                                  const std::string& service_path,
                                  const base::DictionaryValue& properties);

  // Calls Shill.Manager.Connect asynchronously.
  void CallShillConnect(const std::string& service_path);

  // Handle failure from ConfigurationHandler calls.
  void HandleConfigurationFailure(
      const std::string& service_path,
      const std::string& error_name,
      scoped_ptr<base::DictionaryValue> error_data);

  // Handle success or failure from Shill.Service.Connect.
  void HandleShillConnectSuccess(const std::string& service_path);
  void HandleShillConnectFailure(const std::string& service_path,
                                 const std::string& error_name,
                                 const std::string& error_message);

  void CheckPendingRequest(const std::string service_path);
  void CheckAllPendingRequests();
  bool CertificateIsConfigured(NetworkUIData* ui_data, std::string* pkcs11_id);
  void ErrorCallbackForPendingRequest(const std::string& service_path,
                                      const std::string& error_name);

  // Calls Shill.Manager.Disconnect asynchronously.
  void CallShillDisconnect(
      const std::string& service_path,
      const base::Closure& success_callback,
      const network_handler::ErrorCallback& error_callback);

  // Handle success or failure from Shill.Service.Disconnect.
  void HandleShillDisconnectSuccess(const std::string& service_path,
                                    const base::Closure& success_callback);

  // Local references to the associated handler instances.
  CertLoader* cert_loader_;
  NetworkStateHandler* network_state_handler_;
  NetworkConfigurationHandler* network_configuration_handler_;

  // Map of pending connect requests, used to prevent repeated attempts while
  // waiting for Shill and to trigger callbacks on eventual success or failure.
  std::map<std::string, ConnectRequest> pending_requests_;
  scoped_ptr<ConnectRequest> queued_connect_;

  // Track certificate loading state.
  bool logged_in_;
  bool certificates_loaded_;

  DISALLOW_COPY_AND_ASSIGN(NetworkConnectionHandler);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_NETWORK_CONNECTION_HANDLER_H_
