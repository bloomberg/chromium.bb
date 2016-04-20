// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/browser_policy_connector_base.h"

#include <stddef.h>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "components/policy/core/common/configuration_policy_provider.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "policy/policy_constants.h"

namespace policy {

namespace {

// Used in BrowserPolicyConnectorBase::SetPolicyProviderForTesting.
bool g_created_policy_service = false;
ConfigurationPolicyProvider* g_testing_provider = nullptr;

}  // namespace

BrowserPolicyConnectorBase::BrowserPolicyConnectorBase(
    const HandlerListFactory& handler_list_factory)
    : is_initialized_(false), platform_policy_provider_(nullptr) {
  // GetPolicyService() must be ready after the constructor is done.
  // The connector is created very early during startup, when the browser
  // threads aren't running yet; initialize components that need local_state,
  // the system request context or other threads (e.g. FILE) at
  // InitPolicyProviders().

  // Initialize the SchemaRegistry with the Chrome schema before creating any
  // of the policy providers in subclasses.
  chrome_schema_ = Schema::Wrap(GetChromeSchemaData());
  handler_list_ = handler_list_factory.Run(chrome_schema_);
  schema_registry_.RegisterComponent(PolicyNamespace(POLICY_DOMAIN_CHROME, ""),
                                     chrome_schema_);
}

BrowserPolicyConnectorBase::~BrowserPolicyConnectorBase() {
  if (is_initialized()) {
    // Shutdown() wasn't invoked by our owner after having called
    // InitPolicyProviders(). This usually means it's an early shutdown and
    // BrowserProcessImpl::StartTearDown() wasn't invoked.
    // Cleanup properly in those cases and avoid crashing the ToastCrasher test.
    Shutdown();
  }
}

void BrowserPolicyConnectorBase::InitPolicyProviders() {
  DCHECK(!is_initialized());

  if (g_testing_provider)
    g_testing_provider->Init(GetSchemaRegistry());

  for (size_t i = 0; i < policy_providers_.size(); ++i)
    policy_providers_[i]->Init(GetSchemaRegistry());

  is_initialized_ = true;
}

void BrowserPolicyConnectorBase::Shutdown() {
  is_initialized_ = false;
  if (g_testing_provider)
    g_testing_provider->Shutdown();
  for (size_t i = 0; i < policy_providers_.size(); ++i)
    policy_providers_[i]->Shutdown();
  // Drop g_testing_provider so that tests executed with --single_process can
  // call SetPolicyProviderForTesting() again. It is still owned by the test.
  g_testing_provider = nullptr;
  g_created_policy_service = false;
}

const Schema& BrowserPolicyConnectorBase::GetChromeSchema() const {
  return chrome_schema_;
}

CombinedSchemaRegistry* BrowserPolicyConnectorBase::GetSchemaRegistry() {
  return &schema_registry_;
}

PolicyService* BrowserPolicyConnectorBase::GetPolicyService() {
  if (!policy_service_) {
    g_created_policy_service = true;
    std::vector<ConfigurationPolicyProvider*> providers;
    if (g_testing_provider) {
      providers.push_back(g_testing_provider);
    } else {
      providers.resize(policy_providers_.size());
      std::copy(policy_providers_.begin(), policy_providers_.end(),
                providers.begin());
    }
    policy_service_.reset(new PolicyServiceImpl(providers));
  }
  return policy_service_.get();
}

ConfigurationPolicyProvider* BrowserPolicyConnectorBase::GetPlatformProvider() {
  if (g_testing_provider)
    return g_testing_provider;
  return platform_policy_provider_;
}

const ConfigurationPolicyHandlerList*
BrowserPolicyConnectorBase::GetHandlerList() const {
  return handler_list_.get();
}

// static
void BrowserPolicyConnectorBase::SetPolicyProviderForTesting(
    ConfigurationPolicyProvider* provider) {
  // If this function is used by a test then it must be called before the
  // browser is created, and GetPolicyService() gets called.
  CHECK(!g_created_policy_service);
  DCHECK(!g_testing_provider);
  g_testing_provider = provider;
}

void BrowserPolicyConnectorBase::AddPolicyProvider(
    std::unique_ptr<ConfigurationPolicyProvider> provider) {
  policy_providers_.push_back(provider.release());
}

void BrowserPolicyConnectorBase::SetPlatformPolicyProvider(
    std::unique_ptr<ConfigurationPolicyProvider> provider) {
  CHECK(!platform_policy_provider_);
  platform_policy_provider_ = provider.get();
  AddPolicyProvider(std::move(provider));
}

}  // namespace policy
