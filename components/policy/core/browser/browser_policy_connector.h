// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_BROWSER_BROWSER_POLICY_CONNECTOR_H_
#define COMPONENTS_POLICY_CORE_BROWSER_BROWSER_POLICY_CONNECTOR_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "components/policy/core/browser/configuration_policy_handler_list.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/core/common/schema_registry.h"
#include "components/policy/policy_export.h"

class PrefRegistrySimple;
class PrefService;

namespace net {
class URLRequestContextGetter;
}

namespace policy {

class ConfigurationPolicyProvider;
class DeviceManagementService;
class PolicyService;
class PolicyStatisticsCollector;

// The BrowserPolicyConnector keeps the browser-global (non Profile-specific)
// policy providers, and some shared components of the policy system.
// This is a basic implementation that gets extended by platform-specific
// subclasses.
class POLICY_EXPORT BrowserPolicyConnector {
 public:
  // Invoke Shutdown() before deleting, see below.
  virtual ~BrowserPolicyConnector();

  // Finalizes the initialization of the connector. This call can be skipped on
  // tests that don't require the full policy system running.
  virtual void Init(
      PrefService* local_state,
      scoped_refptr<net::URLRequestContextGetter> request_context) = 0;

  // Stops the policy providers and cleans up the connector before it can be
  // safely deleted. This must be invoked before the destructor and while the
  // threads are still running. The policy providers are still valid but won't
  // update anymore after this call.
  virtual void Shutdown();

  // Returns true if Init() has been called but Shutdown() hasn't been yet.
  bool is_initialized() const { return is_initialized_; }

  // Returns a handle to the Chrome schema.
  const Schema& GetChromeSchema() const;

  // Returns the global CombinedSchemaRegistry. SchemaRegistries from Profiles
  // should be tracked by the global registry, so that the global policy
  // providers also load policies for the components of each Profile.
  CombinedSchemaRegistry* GetSchemaRegistry();

  // Returns the browser-global PolicyService, that contains policies for the
  // whole browser.
  PolicyService* GetPolicyService();

  // Returns the platform-specific policy provider, if there is one.
  ConfigurationPolicyProvider* GetPlatformProvider();

  // Schedules initialization of the cloud policy backend services, if the
  // services are already constructed.
  void ScheduleServiceInitialization(int64 delay_milliseconds);

  const ConfigurationPolicyHandlerList* GetHandlerList() const;

  DeviceManagementService* device_management_service() {
    return device_management_service_.get();
  }

  // Sets a |provider| that will be included in PolicyServices returned by
  // GetPolicyService. This is a static method because local state is
  // created immediately after the connector, and tests don't have a chance to
  // inject the provider otherwise. |provider| must outlive the connector, and
  // its ownership is not taken though the connector will initialize and shut it
  // down.
  static void SetPolicyProviderForTesting(
      ConfigurationPolicyProvider* provider);

  // Check whether a user is known to be non-enterprise. Domains such as
  // gmail.com and googlemail.com are known to not be managed. Also returns
  // false if the username is empty.
  static bool IsNonEnterpriseUser(const std::string& username);

  // Returns the URL for the device management service endpoint.
  static std::string GetDeviceManagementUrl();

  // Registers refresh rate prefs.
  static void RegisterPrefs(PrefRegistrySimple* registry);

 protected:
  // Builds an uninitialized BrowserPolicyConnector.
  // Init() should be called to create and start the policy components.
  explicit BrowserPolicyConnector(
      const HandlerListFactory& handler_list_factory);

  // Helper for the public Init() that must be called by subclasses.
  void Init(PrefService* local_state,
            scoped_refptr<net::URLRequestContextGetter> request_context,
            scoped_ptr<DeviceManagementService> device_management_service);

  // Adds |provider| to the list of |policy_providers_|. Providers should
  // be added in decreasing order of priority.
  void AddPolicyProvider(scoped_ptr<ConfigurationPolicyProvider> provider);

  // Same as AddPolicyProvider(), but |provider| becomes the platform provider
  // which can be retrieved by GetPlatformProvider(). This can be called at
  // most once, and uses the same priority order as AddPolicyProvider().
  void SetPlatformPolicyProvider(
      scoped_ptr<ConfigurationPolicyProvider> provider);

 private:
  // Whether Init() but not Shutdown() has been invoked.
  bool is_initialized_;

  // Used to convert policies to preferences. The providers declared below
  // may trigger policy updates during shutdown, which will result in
  // |handler_list_| being consulted for policy translation.
  // Therefore, it's important to destroy |handler_list_| after the providers.
  scoped_ptr<ConfigurationPolicyHandlerList> handler_list_;

  // The Chrome schema. This wraps the structure generated by
  // generate_policy_source.py at compile time.
  Schema chrome_schema_;

  // The global SchemaRegistry, which will track all the other registries.
  CombinedSchemaRegistry schema_registry_;

  // The browser-global policy providers, in decreasing order of priority.
  ScopedVector<ConfigurationPolicyProvider> policy_providers_;
  ConfigurationPolicyProvider* platform_policy_provider_;

  // Must be deleted before all the policy providers.
  scoped_ptr<PolicyService> policy_service_;

  scoped_ptr<PolicyStatisticsCollector> policy_statistics_collector_;

  scoped_ptr<DeviceManagementService> device_management_service_;

  DISALLOW_COPY_AND_ASSIGN(BrowserPolicyConnector);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_BROWSER_BROWSER_POLICY_CONNECTOR_H_
