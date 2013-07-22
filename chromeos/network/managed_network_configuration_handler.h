 // Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_MANAGED_NETWORK_CONFIGURATION_HANDLER_H_
#define CHROMEOS_NETWORK_MANAGED_NETWORK_CONFIGURATION_HANDLER_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_handler_callbacks.h"
#include "chromeos/network/network_profile_observer.h"
#include "chromeos/network/onc/onc_constants.h"

namespace base {
class DictionaryValue;
class ListValue;
}

namespace chromeos {

class NetworkConfigurationHandler;
class NetworkProfileHandler;
class NetworkStateHandler;
class NetworkUIData;

// The ManagedNetworkConfigurationHandler class is used to create and configure
// networks in ChromeOS using ONC and takes care of network policies.
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

class CHROMEOS_EXPORT ManagedNetworkConfigurationHandler
    : public NetworkProfileObserver {
 public:
  typedef std::map<std::string, const base::DictionaryValue*> GuidToPolicyMap;
  typedef std::map<std::string, GuidToPolicyMap> UserToPoliciesMap;

  virtual ~ManagedNetworkConfigurationHandler();

  // Returns the NetworkUIData parsed from the UIData property of
  // |shill_dictionary|. If parsing fails or the field doesn't exist, returns
  // NULL.
  static scoped_ptr<NetworkUIData> GetUIData(
      const base::DictionaryValue& shill_dictionary);

  // Provides the properties of the network with |service_path| to |callback|.
  void GetProperties(
      const std::string& service_path,
      const network_handler::DictionaryResultCallback& callback,
      const network_handler::ErrorCallback& error_callback) const;

  // Provides the managed properties of the network with |service_path| to
  // |callback|. |userhash| is only used to ensure that the user's policy is
  // already applied.
  void GetManagedProperties(
      const std::string& userhash,
      const std::string& service_path,
      const network_handler::DictionaryResultCallback& callback,
      const network_handler::ErrorCallback& error_callback);

  // Sets the user's settings of an already configured network with
  // |service_path|. A network can be initially configured by calling
  // CreateConfiguration or if it is managed by a policy. The given properties
  // will be merged with the existing settings, and it won't clear any existing
  // properties.
  void SetProperties(
      const std::string& service_path,
      const base::DictionaryValue& user_settings,
      const base::Closure& callback,
      const network_handler::ErrorCallback& error_callback) const;

  // Initially configures an unconfigured network with the given user settings
  // and returns the new identifier to |callback| if successful. Fails if the
  // network was already configured by a call to this function or because of a
  // policy. The new configuration will be owned by user |userhash|. If
  // |userhash| is empty, the new configuration will be shared.
  void CreateConfiguration(
      const std::string& userhash,
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

  // Only to be called by NetworkConfigurationUpdater or from tests.  Sets
  // |network_configs_onc| as the current policy of |onc_source|. The network
  // configurations of the policy will be applied (not necessarily immediately)
  // to Shill's profiles and enforced in future configurations until the policy
  // associated with |onc_source| is changed again with this function. For
  // device policies, |userhash| must be empty.
  void SetPolicy(onc::ONCSource onc_source,
                 const std::string& userhash,
                 const base::ListValue& network_configs_onc);

  // Returns the user policy for user |userhash| or device policy, which has
  // |guid|. If |userhash| is empty, only looks for a device policy. If such
  // doesn't exist, returns NULL. Sets |onc_source| accordingly.
  const base::DictionaryValue* FindPolicyByGUID(
      const std::string userhash,
      const std::string& guid,
      onc::ONCSource* onc_source) const;

  // NetworkProfileObserver overrides
  virtual void OnProfileAdded(const NetworkProfile& profile) OVERRIDE;
  virtual void OnProfileRemoved(const NetworkProfile& profile) OVERRIDE;

  NetworkConfigurationHandler* network_configuration_handler() {
    return network_configuration_handler_;
  }

 private:
  friend class NetworkHandler;
  friend class ManagedNetworkConfigurationHandlerTest;
  class PolicyApplicator;

  ManagedNetworkConfigurationHandler();

  void Init(NetworkStateHandler* network_state_handler,
            NetworkProfileHandler* network_profile_handler,
            NetworkConfigurationHandler* network_configuration_handler);


  void GetManagedPropertiesCallback(
      const network_handler::DictionaryResultCallback& callback,
      const network_handler::ErrorCallback& error_callback,
      const std::string& service_path,
      const base::DictionaryValue& shill_properties);

  const GuidToPolicyMap* GetPoliciesForUser(const std::string& userhash) const;
  const GuidToPolicyMap* GetPoliciesForProfile(
      const NetworkProfile& profile) const;

  // The DictionaryValues of the nested maps are owned by this class and are
  // explicitly deleted where necessary. If present, the empty string maps to
  // the device policy.
  UserToPoliciesMap policies_by_user_;

  // Local references to the associated handler instances.
  NetworkStateHandler* network_state_handler_;
  NetworkProfileHandler* network_profile_handler_;
  NetworkConfigurationHandler* network_configuration_handler_;

  // For Shill client callbacks
  base::WeakPtrFactory<ManagedNetworkConfigurationHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ManagedNetworkConfigurationHandler);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_MANAGED_NETWORK_CONFIGURATION_HANDLER_H_
