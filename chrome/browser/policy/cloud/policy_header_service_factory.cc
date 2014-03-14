// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/policy_header_service_factory.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/policy_header_service.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_factory_chromeos.h"
#else
#include "chrome/browser/policy/cloud/user_cloud_policy_manager_factory.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#endif

namespace policy {

namespace {

class PolicyHeaderServiceWrapper : public KeyedService {
 public:
  explicit PolicyHeaderServiceWrapper(scoped_ptr<PolicyHeaderService> service)
      : policy_header_service_(service.Pass()) {}

  PolicyHeaderService* policy_header_service() const {
    return policy_header_service_.get();
  }

  virtual void Shutdown() OVERRIDE {
    // Shutdown our core object so it can unregister any observers before the
    // services we depend on are shutdown.
    policy_header_service_.reset();
  }

 private:
  scoped_ptr<PolicyHeaderService> policy_header_service_;
};

}  // namespace

PolicyHeaderServiceFactory::PolicyHeaderServiceFactory()
    : BrowserContextKeyedServiceFactory(
        "PolicyHeaderServiceFactory",
        BrowserContextDependencyManager::GetInstance()) {
#if defined(OS_CHROMEOS)
  DependsOn(UserCloudPolicyManagerFactoryChromeOS::GetInstance());
#else
  DependsOn(UserCloudPolicyManagerFactory::GetInstance());
#endif
}

PolicyHeaderServiceFactory::~PolicyHeaderServiceFactory() {
}

// static
PolicyHeaderService* PolicyHeaderServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  PolicyHeaderServiceWrapper* wrapper =
      static_cast<PolicyHeaderServiceWrapper*>(
          GetInstance()->GetServiceForBrowserContext(context, true));
  if (wrapper)
    return wrapper->policy_header_service();
  else
    return NULL;
}

KeyedService* PolicyHeaderServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
#if defined(OS_CHROMEOS)
  BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
#else
  BrowserPolicyConnector* connector =
      g_browser_process->browser_policy_connector();
#endif

  DeviceManagementService* device_management_service =
      connector->device_management_service();
#if defined(OS_CHROMEOS)
  CloudPolicyManager* manager =
      UserCloudPolicyManagerFactoryChromeOS::GetForProfile(
           Profile::FromBrowserContext(context));
#else
  CloudPolicyManager* manager =
      UserCloudPolicyManagerFactory::GetForBrowserContext(context);
#endif
  if (!manager)
    return NULL;
  CloudPolicyStore* user_store = manager->core()->store();
  CloudPolicyStore* device_store = NULL;
#if defined(OS_CHROMEOS)
  device_store = connector->GetDeviceCloudPolicyManager()->core()->store();
#endif

  scoped_ptr<PolicyHeaderService> service = make_scoped_ptr(
      new PolicyHeaderService(device_management_service->GetServerUrl(),
                              kPolicyVerificationKeyHash,
                              user_store,
                              device_store));
  return new PolicyHeaderServiceWrapper(service.Pass());
}

// static
PolicyHeaderServiceFactory* PolicyHeaderServiceFactory::GetInstance() {
  return Singleton<PolicyHeaderServiceFactory>::get();
}

}  // namespace policy
