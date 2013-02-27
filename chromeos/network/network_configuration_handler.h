// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_NETWORK_CONFIGURATION_HANDLER_H_
#define CHROMEOS_NETWORK_NETWORK_CONFIGURATION_HANDLER_H_

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

// The NetworkConfigurationHandler class is used to create and configure
// networks in ChromeOS. It mostly calls through to the Shill service API, and
// most calls are asynchronous for that reason. No calls will block on DBus
// calls.
//
// This is owned and it's lifetime managed by the Chrome startup code. It's
// basically a singleton, but with explicit lifetime management.
//
// For accessing lists of remembered networks, and other state information, see
// the class NetworkStateHandler.
//
// Note on callbacks: Because all the functions here are meant to be
// asynchronous, they all take a |callback| of some type, and an
// |error_callback|. When the operation succeeds, |callback| will be called, and
// when it doesn't, |error_callback| will be called with information about the
// error, including a symbolic name for the error and often some error message
// that is suitable for logging. None of the error message text is meant for
// user consumption.

class CHROMEOS_EXPORT NetworkConfigurationHandler {
 public:
  ~NetworkConfigurationHandler();

  // Sets the global instance. Must be called before any calls to Get().
  static void Initialize();

  // Destroys the global instance.
  static void Shutdown();

  // Gets the global instance. Initialize() must be called first.
  static NetworkConfigurationHandler* Get();

  // Gets the properties of the network with id |service_path|. See note on
  // |callback| and |error_callback|, in class description above.
  void GetProperties(
      const std::string& service_path,
      const network_handler::DictionaryResultCallback& callback,
      const network_handler::ErrorCallback& error_callback) const;

  // Sets the properties of the network with id |service_path|. This means the
  // given properties will be merged with the existing settings, and it won't
  // clear any existing properties. See note on |callback| and |error_callback|,
  // in class description above.
  void SetProperties(
      const std::string& service_path,
      const base::DictionaryValue& properties,
      const base::Closure& callback,
      const network_handler::ErrorCallback& error_callback) const;

  // Removes the properties with the given property paths. If any of them are
  // unable to be cleared, the |error_callback| will only be run once with
  // accumulated information about all of the errors as a list attached to the
  // "errors" key of the error data, and the |callback| will not be run, even
  // though some of the properties may have been cleared. If there are no
  // errors, |callback| will be run.
  void ClearProperties(const std::string& service_path,
                       const std::vector<std::string>& property_paths,
                       const base::Closure& callback,
                       const network_handler::ErrorCallback& error_callback);

  // Initiates a connection with network that has id |service_path|. See note on
  // |callback| and |error_callback|, in class description above.
  void Connect(const std::string& service_path,
               const base::Closure& callback,
               const network_handler::ErrorCallback& error_callback) const;

  // Initiates a disconnect with the network at |service_path|. See note on
  // |callback| and |error_callback|, in class description above.
  void Disconnect(const std::string& service_path,
                  const base::Closure& callback,
                  const network_handler::ErrorCallback& error_callback) const;


  // Creates a network with the given properties in the active Shill profile,
  // and returns the new service_path to |callback| if successful. See note on
  // |callback| and |error_callback|, in class description above.
  void CreateConfiguration(
      const base::DictionaryValue& properties,
      const network_handler::StringResultCallback& callback,
      const network_handler::ErrorCallback& error_callback) const;

  // Removes the network |service_path| from the remembered network list in the
  // active Shill profile. The network may still show up in the visible networks
  // after this, but no profile configuration will remain. See note on
  // |callback| and |error_callback|, in class description above.
  void RemoveConfiguration(
      const std::string& service_path,
      const base::Closure& callback,
      const network_handler::ErrorCallback& error_callback) const;

 private:
  friend class NetworkConfigurationHandlerTest;
  NetworkConfigurationHandler();

  DISALLOW_COPY_AND_ASSIGN(NetworkConfigurationHandler);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_NETWORK_CONFIGURATION_HANDLER_H_
