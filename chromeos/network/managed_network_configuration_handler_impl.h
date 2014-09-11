// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_MANAGED_NETWORK_CONFIGURATION_HANDLER_IMPL_H_
#define CHROMEOS_NETWORK_MANAGED_NETWORK_CONFIGURATION_HANDLER_IMPL_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_handler_callbacks.h"
#include "chromeos/network/network_profile_observer.h"
#include "chromeos/network/policy_applicator.h"

namespace base {
class DictionaryValue;
}

namespace chromeos {

class NetworkConfigurationHandler;
struct NetworkProfile;
class NetworkProfileHandler;
class NetworkStateHandler;

class CHROMEOS_EXPORT ManagedNetworkConfigurationHandlerImpl
    : public ManagedNetworkConfigurationHandler,
      public NetworkProfileObserver,
      public PolicyApplicator::ConfigurationHandler {
 public:
  virtual ~ManagedNetworkConfigurationHandlerImpl();

  // ManagedNetworkConfigurationHandler overrides
  virtual void AddObserver(NetworkPolicyObserver* observer) OVERRIDE;
  virtual void RemoveObserver(NetworkPolicyObserver* observer) OVERRIDE;

  virtual void GetProperties(
      const std::string& service_path,
      const network_handler::DictionaryResultCallback& callback,
      const network_handler::ErrorCallback& error_callback) OVERRIDE;

  virtual void GetManagedProperties(
      const std::string& userhash,
      const std::string& service_path,
      const network_handler::DictionaryResultCallback& callback,
      const network_handler::ErrorCallback& error_callback) OVERRIDE;

  virtual void SetProperties(
      const std::string& service_path,
      const base::DictionaryValue& user_settings,
      const base::Closure& callback,
      const network_handler::ErrorCallback& error_callback) const OVERRIDE;

  virtual void CreateConfiguration(
      const std::string& userhash,
      const base::DictionaryValue& properties,
      const network_handler::StringResultCallback& callback,
      const network_handler::ErrorCallback& error_callback) const OVERRIDE;

  virtual void RemoveConfiguration(
      const std::string& service_path,
      const base::Closure& callback,
      const network_handler::ErrorCallback& error_callback) const OVERRIDE;

  virtual void SetPolicy(
      onc::ONCSource onc_source,
      const std::string& userhash,
      const base::ListValue& network_configs_onc,
      const base::DictionaryValue& global_network_config) OVERRIDE;

  virtual const base::DictionaryValue* FindPolicyByGUID(
      const std::string userhash,
      const std::string& guid,
      onc::ONCSource* onc_source) const OVERRIDE;

  virtual const base::DictionaryValue* GetGlobalConfigFromPolicy(
      const std::string userhash) const OVERRIDE;

  virtual const base::DictionaryValue* FindPolicyByGuidAndProfile(
      const std::string& guid,
      const std::string& profile_path) const OVERRIDE;

  // NetworkProfileObserver overrides
  virtual void OnProfileAdded(const NetworkProfile& profile) OVERRIDE;
  virtual void OnProfileRemoved(const NetworkProfile& profile) OVERRIDE;

  // PolicyApplicator::ConfigurationHandler overrides
  virtual void CreateConfigurationFromPolicy(
      const base::DictionaryValue& shill_properties) OVERRIDE;

  virtual void UpdateExistingConfigurationWithPropertiesFromPolicy(
      const base::DictionaryValue& existing_properties,
      const base::DictionaryValue& new_properties) OVERRIDE;

  virtual void OnPoliciesApplied() OVERRIDE;

 private:
  friend class ClientCertResolverTest;
  friend class ManagedNetworkConfigurationHandlerTest;
  friend class NetworkConnectionHandlerTest;
  friend class NetworkHandler;

  struct Policies;
  typedef std::map<std::string, linked_ptr<Policies> > UserToPoliciesMap;
  typedef base::Callback<void(const std::string& service_path,
                              scoped_ptr<base::DictionaryValue> properties)>
      GetDevicePropertiesCallback;

  ManagedNetworkConfigurationHandlerImpl();

  // Handlers may be NULL in tests so long as they do not execute any paths
  // that require the handlers.
  void Init(NetworkStateHandler* network_state_handler,
            NetworkProfileHandler* network_profile_handler,
            NetworkConfigurationHandler* network_configuration_handler,
            NetworkDeviceHandler* network_device_handler);

  // Sends the response to the caller of GetManagedProperties.
  void SendManagedProperties(
      const std::string& userhash,
      const network_handler::DictionaryResultCallback& callback,
      const network_handler::ErrorCallback& error_callback,
      const std::string& service_path,
      scoped_ptr<base::DictionaryValue> shill_properties);

  // Sends the response to the caller of GetProperties.
  void SendProperties(
      const network_handler::DictionaryResultCallback& callback,
      const network_handler::ErrorCallback& error_callback,
      const std::string& service_path,
      scoped_ptr<base::DictionaryValue> shill_properties);

  const Policies* GetPoliciesForUser(const std::string& userhash) const;
  const Policies* GetPoliciesForProfile(const NetworkProfile& profile) const;

  void OnPolicyAppliedToNetwork(const std::string& service_path);

  // Helper method to append associated Device properties to |properties|.
  void GetDeviceStateProperties(const std::string& service_path,
                                base::DictionaryValue* properties);

  // Callback for NetworkConfigurationHandler::GetProperties requests from
  // Get{Managed}Properties. This callback fills in properties from
  // DeviceState and may request additional Device properties.
  // Note: Requesting Device properties requires an additional fetch and
  // additional copying of data, so we only do it for Cellular networks which
  // contain a lot of necessary state in the associated Device object.
  void GetPropertiesCallback(
      GetDevicePropertiesCallback send_callback,
      const std::string& service_path,
      const base::DictionaryValue& shill_properties);

  void GetDevicePropertiesSuccess(
      const std::string& service_path,
      scoped_ptr<base::DictionaryValue> network_properties,
      GetDevicePropertiesCallback send_callback,
      const std::string& device_path,
      const base::DictionaryValue& device_properties);
  void GetDevicePropertiesFailure(
      const std::string& service_path,
      scoped_ptr<base::DictionaryValue> network_properties,
      GetDevicePropertiesCallback send_callback,
      const std::string& error_name,
      scoped_ptr<base::DictionaryValue> error_data);

  // If present, the empty string maps to the device policy.
  UserToPoliciesMap policies_by_user_;

  // Local references to the associated handler instances.
  NetworkStateHandler* network_state_handler_;
  NetworkProfileHandler* network_profile_handler_;
  NetworkConfigurationHandler* network_configuration_handler_;
  NetworkDeviceHandler* network_device_handler_;

  ObserverList<NetworkPolicyObserver> observers_;

  // For Shill client callbacks
  base::WeakPtrFactory<ManagedNetworkConfigurationHandlerImpl>
      weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ManagedNetworkConfigurationHandlerImpl);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_MANAGED_NETWORK_CONFIGURATION_HANDLER_IMPL_H_
