// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_NETWORK_CONFIGURATION_HANDLER_H_
#define CHROMEOS_NETWORK_NETWORK_CONFIGURATION_HANDLER_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_handler_callbacks.h"

namespace base {
class DictionaryValue;
class ListValue;
}

namespace dbus {
class ObjectPath;
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

class CHROMEOS_EXPORT NetworkConfigurationHandler
    : public base::SupportsWeakPtr<NetworkConfigurationHandler> {
 public:
  ~NetworkConfigurationHandler();

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
      const network_handler::ErrorCallback& error_callback);

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

  // Creates a network with the given |properties| in the specified Shill
  // profile, and returns the new service_path to |callback| if successful.
  // kProfileProperty must be set in |properties|. See note on |callback| and
  // |error_callback|, in the class description above. This may also be used to
  // update an existing matching configuration, see Shill documentation for
  // Manager.ConfigureServiceForProfile.
  void CreateConfiguration(
      const base::DictionaryValue& properties,
      const network_handler::StringResultCallback& callback,
      const network_handler::ErrorCallback& error_callback);

  // Removes the network |service_path| from any profiles that include it.
  // See note on |callback| and |error_callback| in class description above.
  void RemoveConfiguration(
      const std::string& service_path,
      const base::Closure& callback,
      const network_handler::ErrorCallback& error_callback);

  // Changes the profile for the network |service_path| to |profile_path|.
  // See note on |callback| and |error_callback| in class description above.
  void SetNetworkProfile(const std::string& service_path,
                         const std::string& profile_path,
                         const base::Closure& callback,
                         const network_handler::ErrorCallback& error_callback);

  // Construct and initialize an instance for testing.
  static NetworkConfigurationHandler* InitializeForTest(
      NetworkStateHandler* network_state_handler);

 protected:
  friend class ClientCertResolverTest;
  friend class NetworkHandler;
  friend class NetworkConfigurationHandlerTest;
  friend class NetworkConfigurationHandlerStubTest;
  class ProfileEntryDeleter;

  NetworkConfigurationHandler();
  void Init(NetworkStateHandler* network_state_handler);

  void RunCreateNetworkCallback(
      const network_handler::StringResultCallback& callback,
      const dbus::ObjectPath& service_path);

  // Called from ProfileEntryDeleter instances when they complete causing
  // this class to delete the instance.
  void ProfileEntryDeleterCompleted(const std::string& service_path);
  bool PendingProfileEntryDeleterForTest(const std::string& service_path) {
    return profile_entry_deleters_.count(service_path);
  }

  // Invoke the callback and inform NetworkStateHandler to request an update
  // for the service after setting properties.
  void SetPropertiesSuccessCallback(const std::string& service_path,
                                    const base::Closure& callback);
  void SetPropertiesErrorCallback(
      const std::string& service_path,
      const network_handler::ErrorCallback& error_callback,
      const std::string& dbus_error_name,
      const std::string& dbus_error_message);

  // Invoke the callback and inform NetworkStateHandler to request an update
  // for the service after clearing properties.
  void ClearPropertiesSuccessCallback(
      const std::string& service_path,
      const std::vector<std::string>& names,
      const base::Closure& callback,
      const base::ListValue& result);
  void ClearPropertiesErrorCallback(
      const std::string& service_path,
      const network_handler::ErrorCallback& error_callback,
      const std::string& dbus_error_name,
      const std::string& dbus_error_message);

  // Unowned associated NetworkStateHandler* (global or test instance).
  NetworkStateHandler* network_state_handler_;

  // Map of in-progress deleter instances. Owned by this class.
  std::map<std::string, ProfileEntryDeleter*> profile_entry_deleters_;

  DISALLOW_COPY_AND_ASSIGN(NetworkConfigurationHandler);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_NETWORK_CONFIGURATION_HANDLER_H_
