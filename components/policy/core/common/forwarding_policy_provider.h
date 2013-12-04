// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_FORWARDING_POLICY_PROVIDER_H_
#define COMPONENTS_POLICY_CORE_COMMON_FORWARDING_POLICY_PROVIDER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "components/policy/core/common/configuration_policy_provider.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/policy_export.h"

namespace policy {

// A policy provider that forwards calls to another provider.
// This provider also tracks the SchemaRegistry, and becomes ready after making
// sure the delegate provider has refreshed its policies with an updated view
// of the complete schema. It is expected that the delegate's SchemaRegistry
// is a CombinedSchemaRegistry tracking the forwarding provider's registry.
class POLICY_EXPORT ForwardingPolicyProvider
    : public ConfigurationPolicyProvider,
      public ConfigurationPolicyProvider::Observer {
 public:
  // The |delegate| must outlive this provider.
  explicit ForwardingPolicyProvider(ConfigurationPolicyProvider* delegate);
  virtual ~ForwardingPolicyProvider();

  // ConfigurationPolicyProvider:
  //
  // Note that Init() and Shutdown() are not forwarded to the |delegate_|, since
  // this provider does not own it and its up to the |delegate_|'s owner to
  // initialize it and shut it down.
  //
  // Note also that this provider may have a SchemaRegistry passed in Init()
  // that doesn't match the |delegate_|'s; therefore OnSchemaRegistryUpdated()
  // and OnSchemaRegistryReady() are not forwarded either. It is assumed that
  // the |delegate_|'s SchemaRegistry contains a superset of this provider's
  // SchemaRegistry though (i.e. it's a CombinedSchemaRegistry that contains
  // this provider's SchemaRegistry).
  //
  // This provider manages its own initialization state for all policy domains
  // except POLICY_DOMAIN_CHROME, whose status is always queried from the
  // |delegate_|. RefreshPolicies() calls are also forwarded, since this
  // provider doesn't have a "real" policy source of its own.
  virtual void Init(SchemaRegistry* registry) OVERRIDE;
  virtual bool IsInitializationComplete(PolicyDomain domain) const OVERRIDE;
  virtual void RefreshPolicies() OVERRIDE;
  virtual void OnSchemaRegistryReady() OVERRIDE;
  virtual void OnSchemaRegistryUpdated(bool has_new_schemas) OVERRIDE;

  // ConfigurationPolicyProvider::Observer:
  virtual void OnUpdatePolicy(ConfigurationPolicyProvider* provider) OVERRIDE;

 private:
  enum InitializationState {
    WAITING_FOR_REGISTRY_READY,
    WAITING_FOR_REFRESH,
    READY,
  };

  ConfigurationPolicyProvider* delegate_;
  InitializationState state_;

  DISALLOW_COPY_AND_ASSIGN(ForwardingPolicyProvider);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_FORWARDING_POLICY_PROVIDER_H_
