// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_VPN_VPN_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_VPN_VPN_SERVICE_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/api/vpn_provider.h"

namespace base {

class DictionaryValue;
class ListValue;

}  // namespace base

namespace content {

class BrowserContext;

}  // namespace content

namespace extensions {

class EventRouter;
class ExtensionRegistry;

}  // namespace extensions

namespace chromeos {

class NetworkConfigurationHandler;
class NetworkProfileHandler;
class ShillThirdPartyVpnDriverClient;

// The class manages the VPN configurations.
class VpnService : public KeyedService,
                   public extensions::ExtensionRegistryObserver {
 public:
  using SuccessCallback = base::Callback<void()>;
  using SuccessCallbackWithHandle = base::Callback<void(int handle)>;
  using FailureCallback =
      base::Callback<void(const std::string& error_name,
                          const std::string& error_message)>;

  VpnService(content::BrowserContext* browser_context,
             extensions::ExtensionRegistry* extension_registry,
             extensions::EventRouter* event_router,
             ShillThirdPartyVpnDriverClient* shill_client,
             NetworkConfigurationHandler* network_configuration_handler,
             NetworkProfileHandler* network_profile_handler);
  ~VpnService() override;

  // ExtensionRegistryObserver:
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const extensions::Extension* extension,
                              extensions::UninstallReason reason) override;

  // Creates a new VPN configuration with |name| as the name and attaches it to
  // the extension with id |extension_id|.
  // Calls |success| or |failure| based on the outcome.
  void CreateConfiguration(const std::string& extension_id,
                           const std::string& name,
                           const SuccessCallbackWithHandle& success,
                           const FailureCallback& failure);

  // Destroys the VPN configuration with the handle |handle| after verifying
  // that it belongs to the extension with id |extension_id|.
  // Calls |success| or |failure| based on the outcome.
  void DestroyConfiguration(const std::string& extension_id,
                            int handle,
                            const SuccessCallback& success,
                            const FailureCallback& failure);

  // Sets parameters for the VPN configuration with the handle |handle| with
  // |parameters| after verifying that it belongs to the extension with id
  // |extension_id|.
  // Calls |success| or |failure| based on the outcome.
  void SetParameters(const std::string& extension_id,
                     int handle,
                     const base::DictionaryValue& parameters,
                     const SuccessCallback& success,
                     const FailureCallback& failure);

  // Sends an IP packet contained in |data| to the VPN configuration with the
  // handle |handle| after verifying that it belongs to the extension with id
  // |extension_id|.
  // Calls |success| or |failure| based on the outcome.
  void SendPacket(const std::string& extension_id,
                  int handle,
                  const std::string& data,
                  const SuccessCallback& success,
                  const FailureCallback& failure);

  // Notifies connection state |state| to the VPN configuration with the
  // handle |handle| after verifying that it belongs to the extension with id
  // |extension_id|.
  // Calls |success| or |failure| based on the outcome.
  void NotifyConnectionStateChanged(
      const std::string& extension_id,
      int handle,
      extensions::core_api::vpn_provider::VpnConnectionState state,
      const SuccessCallback& success,
      const FailureCallback& failure);

 private:
  class VpnConfiguration;

  using KeyToConfigurationMap = std::map<std::string, VpnConfiguration*>;
  using HandleToConfigurationMap = std::map<int, VpnConfiguration*>;

  // Callback used to indicate that configuration was successfully created.
  void OnCreateConfigurationSuccess(const SuccessCallbackWithHandle& callback,
                                    VpnConfiguration* configuration,
                                    const std::string& service_path);

  // Callback used to indicate that configuration creation failed.
  void OnCreateConfigurationFailure(
      const FailureCallback& callback,
      VpnConfiguration* configuration,
      const std::string& error_name,
      scoped_ptr<base::DictionaryValue> error_data);

  // Callback used to indicate that removing a configuration succeeded.
  void OnRemoveConfigurationSuccess(const SuccessCallback& callback);

  // Callback used to indicate that removing a configuration failed.
  void OnRemoveConfigurationFailure(
      const FailureCallback& callback,
      const std::string& error_name,
      scoped_ptr<base::DictionaryValue> error_data);

  // Creates and adds the configuration to the internal store.
  VpnConfiguration* CreateConfigurationInternal(const std::string& extension_id,
                                                const std::string& name,
                                                int handle);

  // Removes configuration from the internal store and destroys it.
  void DestroyConfigurationInternal(VpnConfiguration* configuration);

  // Increments handle in a safe manner and returns old handle.
  int PostIncrementHandle();

  // Finds and returns an unused handle.
  int GetNewHandle();

  // Verifies if the extension with id |extension_id| is authorized to access
  // |handle|.
  bool IsAuthorizedAndConfigurationExists(int handle,
                                          const std::string& extension_id);

  // Retrieve a VpnConfiguration from the internal store by handle.
  VpnConfiguration* GetConfigurationByHandle(int handle);

  // Send an event with name |event_name| and arguments |event_args| to the
  // extension with id |extension_id|.
  void SendSignalToExtension(const std::string& extension_id,
                             const std::string& event_name,
                             scoped_ptr<base::ListValue> event_args);

  content::BrowserContext* browser_context_;

  int next_handle_;

  extensions::ExtensionRegistry* extension_registry_;
  extensions::EventRouter* event_router_;
  ShillThirdPartyVpnDriverClient* shill_client_;
  NetworkConfigurationHandler* network_configuration_handler_;
  NetworkProfileHandler* network_profile_handler_;

  // Handle map owns the VpnConfigurations.
  HandleToConfigurationMap handle_to_configuration_map_;

  // Key map does not own the VpnConfigurations.
  KeyToConfigurationMap key_to_configuration_map_;

  base::WeakPtrFactory<VpnService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(VpnService);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_VPN_VPN_SERVICE_H_
