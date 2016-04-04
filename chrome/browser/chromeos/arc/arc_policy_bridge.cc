// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_policy_bridge.h"

#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/user_manager/user.h"
#include "mojo/public/cpp/bindings/string.h"
#include "policy/policy_constants.h"

namespace arc {

ArcPolicyBridge::ArcPolicyBridge(ArcBridgeService* bridge_service)
    : ArcService(bridge_service), binding_(this) {
  VLOG(1) << "ArcPolicyBridge::ArcPolicyBridge";
  arc_bridge_service()->AddObserver(this);
}

ArcPolicyBridge::ArcPolicyBridge(ArcBridgeService* bridge_service,
                                 policy::PolicyService* policy_service)
    : ArcService(bridge_service),
      binding_(this),
      policy_service_(policy_service) {
  VLOG(1) << "ArcPolicyBridge::ArcPolicyBridge(bridge_service, policy_service)";
  arc_bridge_service()->AddObserver(this);
}

ArcPolicyBridge::~ArcPolicyBridge() {
  VLOG(1) << "ArcPolicyBridge::~ArcPolicyBridge";
  arc_bridge_service()->RemoveObserver(this);
}

void ArcPolicyBridge::OnPolicyInstanceReady() {
  VLOG(1) << "ArcPolicyBridge::OnPolicyInstanceReady";
  if (policy_service_ == nullptr) {
    InitializePolicyService();
  }
  policy_service_->AddObserver(policy::POLICY_DOMAIN_CHROME, this);

  PolicyInstance* const policy_instance =
      arc_bridge_service()->policy_instance();
  if (!policy_instance) {
    LOG(ERROR) << "OnPolicyInstanceReady called, but no policy instance found";
    return;
  }

  policy_instance->Init(binding_.CreateInterfacePtrAndBind());
}

void ArcPolicyBridge::OnPolicyInstanceClosed() {
  VLOG(1) << "ArcPolicyBridge::OnPolicyInstanceClosed";
  policy_service_->RemoveObserver(policy::POLICY_DOMAIN_CHROME, this);
  policy_service_ = nullptr;
}

void ArcPolicyBridge::GetPolicies(const GetPoliciesCallback& callback) {
  VLOG(1) << "ArcPolicyBridge::GetPolicies";
  const policy::PolicyNamespace policy_namespace(policy::POLICY_DOMAIN_CHROME,
                                                 std::string());
  const policy::PolicyMap& policy_map =
      policy_service_->GetPolicies(policy_namespace);
  const std::string json_policies = GetFilteredJSONPolicies(policy_map);
  callback.Run(mojo::String(json_policies));
}

void ArcPolicyBridge::OnPolicyUpdated(const policy::PolicyNamespace& ns,
                                      const policy::PolicyMap& previous,
                                      const policy::PolicyMap& current) {
  VLOG(1) << "ArcPolicyBridge::OnPolicyUpdated";
  DCHECK(arc_bridge_service()->policy_instance());
  arc_bridge_service()->policy_instance()->OnPolicyUpdated();
}

void ArcPolicyBridge::InitializePolicyService() {
  const user_manager::User* const primary_user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  Profile* const profile =
      chromeos::ProfileHelper::Get()->GetProfileByUser(primary_user);
  policy_service_ =
      policy::ProfilePolicyConnectorFactory::GetForBrowserContext(profile)
          ->policy_service();
}

std::string ArcPolicyBridge::GetFilteredJSONPolicies(
    const policy::PolicyMap& policy_map) {
  // TODO(phweiss): Implement general filtering mechanism when more policies
  // need to be passed on.
  // Create dictionary with the desired policies.
  base::DictionaryValue filtered_policies;
  const std::string policy_name = policy::key::kArcApplicationPolicy;
  const base::Value* const policy_value = policy_map.GetValue(policy_name);
  if (policy_value) {
    filtered_policies.Set(policy_name,
                          policy_value->CreateDeepCopy().release());
  }
  // Convert dictionary to JSON.
  std::string policy_json;
  JSONStringValueSerializer serializer(&policy_json);
  serializer.Serialize(filtered_policies);
  return policy_json;
}

}  // namespace arc
