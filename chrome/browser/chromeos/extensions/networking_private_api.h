// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These classes implement the chrome.networkingPrivate JavaScript extension
// API.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_NETWORKING_PRIVATE_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_NETWORKING_PRIVATE_API_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chromeos/dbus/dbus_method_call_status.h"

// Implements the chrome.networkingPrivate.getProperties method.
class NetworkingPrivateGetPropertiesFunction : public AsyncExtensionFunction {
 public:
  NetworkingPrivateGetPropertiesFunction() {}
  DECLARE_EXTENSION_FUNCTION("networkingPrivate.getProperties",
                             NETWORKINGPRIVATE_GETPROPERTIES);

 protected:
  virtual ~NetworkingPrivateGetPropertiesFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  // Callback if talking to ShillServiceClient directly.
  // TODO(pneubeck): Remove once the ManagedNetworkConfigurationHandler is
  // stable.
  void ResultCallback(const std::string& service_path,
                      chromeos::DBusMethodCallStatus call_status,
                      const base::DictionaryValue& result);

  // Callbacks if talking to ManagedNetworkConfigurationHandler.
  void GetPropertiesSuccess(const std::string& service_path,
                            const base::DictionaryValue& result);
  void GetPropertiesFailed(const std::string& error_name,
                           scoped_ptr<base::DictionaryValue> error_data);
  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateGetPropertiesFunction);
};

// Implements the chrome.networkingPrivate.getVisibleNetworks method.
class NetworkingPrivateGetVisibleNetworksFunction
    : public AsyncExtensionFunction {
 public:
  NetworkingPrivateGetVisibleNetworksFunction() {}
  DECLARE_EXTENSION_FUNCTION("networkingPrivate.getVisibleNetworks",
                              NETWORKINGPRIVATE_GETVISIBLENETWORKS);

 protected:
  virtual ~NetworkingPrivateGetVisibleNetworksFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

  // Gets called when all the results are in.
  void SendResultCallback(const std::string& error,
                          scoped_ptr<base::ListValue> result_list);

 private:

  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateGetVisibleNetworksFunction);
};

// Implements the chrome.networkingPrivate.startConnect method.
class NetworkingPrivateStartConnectFunction : public AsyncExtensionFunction {
 public:
  NetworkingPrivateStartConnectFunction() {}
  DECLARE_EXTENSION_FUNCTION("networkingPrivate.startConnect",
                             NETWORKINGPRIVATE_STARTCONNECT);

 protected:
  virtual ~NetworkingPrivateStartConnectFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  // Called when the request to connect succeeds. Doesn't mean that the connect
  // itself succeeded, just that the request did.
  void ConnectionStartSuccess();

  void ConnectionStartFailed(const std::string& error_name,
                             const std::string& error_message);

  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateStartConnectFunction);
};

// Implements the chrome.networkingPrivate.startDisconnect method.
class NetworkingPrivateStartDisconnectFunction
    : public AsyncExtensionFunction {
 public:
  NetworkingPrivateStartDisconnectFunction() {}
  DECLARE_EXTENSION_FUNCTION("networkingPrivate.startDisconnect",
                             NETWORKINGPRIVATE_STARTDISCONNECT);

 protected:
  virtual ~NetworkingPrivateStartDisconnectFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  // Called when the request to disconnect succeeds. Doesn't mean that the
  // disconnect itself succeeded, just that the request did.
  void DisconnectionStartSuccess();

  void DisconnectionStartFailed(const std::string& error_name,
                                const std::string& error_message);

  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateStartDisconnectFunction);
};

// Implements the chrome.networkingPrivate.verifyDestination method.
class NetworkingPrivateVerifyDestinationFunction
    : public AsyncExtensionFunction {
 public:
  NetworkingPrivateVerifyDestinationFunction() {}
  DECLARE_EXTENSION_FUNCTION("networkingPrivate.verifyDestination",
                             NETWORKINGPRIVATE_VERIFYDESTINATION);

 protected:
  virtual ~NetworkingPrivateVerifyDestinationFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

  void ResultCallback(bool result);
  void ErrorCallback(const std::string& error_name, const std::string& error);

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateVerifyDestinationFunction);
};

// Implements the chrome.networkingPrivate.verifyAndEncryptCredentials method.
class NetworkingPrivateVerifyAndEncryptCredentialsFunction
    : public AsyncExtensionFunction {
 public:
  NetworkingPrivateVerifyAndEncryptCredentialsFunction() {}
  DECLARE_EXTENSION_FUNCTION("networkingPrivate.verifyAndEncryptCredentials",
                             NETWORKINGPRIVATE_VERIFYANDENCRYPTCREDENTIALS);

 protected:
  virtual ~NetworkingPrivateVerifyAndEncryptCredentialsFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

  void ResultCallback(const std::string& result);
  void ErrorCallback(const std::string& error_name, const std::string& error);

 private:
  DISALLOW_COPY_AND_ASSIGN(
      NetworkingPrivateVerifyAndEncryptCredentialsFunction);
};

// Implements the chrome.networkingPrivate.verifyAndEncryptData method.
class NetworkingPrivateVerifyAndEncryptDataFunction
    : public AsyncExtensionFunction {
 public:
  NetworkingPrivateVerifyAndEncryptDataFunction() {}
  DECLARE_EXTENSION_FUNCTION("networkingPrivate.verifyAndEncryptData",
                             NETWORKINGPRIVATE_VERIFYANDENCRYPTDATA);

 protected:
  virtual ~NetworkingPrivateVerifyAndEncryptDataFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

  void ResultCallback(const std::string& result);
  void ErrorCallback(const std::string& error_name, const std::string& error);

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateVerifyAndEncryptDataFunction);
};

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_NETWORKING_PRIVATE_API_H_
