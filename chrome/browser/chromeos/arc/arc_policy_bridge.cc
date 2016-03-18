// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_policy_bridge.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/user_manager/user.h"
#include "mojo/public/cpp/bindings/string.h"

namespace arc {

ArcPolicyBridge::ArcPolicyBridge(ArcBridgeService* bridge_service)
    : ArcService(bridge_service), binding_(this) {
  VLOG(1) << "ArcPolicyBridge::ArcPolicyBridge";
  arc_bridge_service()->AddObserver(this);
}

ArcPolicyBridge::~ArcPolicyBridge() {
  VLOG(1) << "ArcPolicyBridge::~ArcPolicyBridge";
  arc_bridge_service()->RemoveObserver(this);
}

void ArcPolicyBridge::OnPolicyInstanceReady() {
  VLOG(1) << "ArcPolicyBridge::OnPolicyInstanceReady";
  const user_manager::User* const primary_user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  Profile* const profile =
      chromeos::ProfileHelper::Get()->GetProfileByUser(primary_user);
  policy_service_ =
      policy::ProfilePolicyConnectorFactory::GetForBrowserContext(profile)
          ->policy_service();
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
  // TODO(phweiss): Get actual policies
  const std::string stub_policies =
      "{\"applications\":[{\"packageName\":"
      "\"com.android.chrome\",\"installType\":\"REQUIRED\",\"lockTaskAllowed\":"
      "false,\"permissionGrants\":[]}]}";
  callback.Run(mojo::String(stub_policies));
}

void ArcPolicyBridge::OnPolicyUpdated(const policy::PolicyNamespace& ns,
                                      const policy::PolicyMap& previous,
                                      const policy::PolicyMap& current) {
  VLOG(1) << "ArcPolicyBridge::OnPolicyUpdated";
  DCHECK(arc_bridge_service()->policy_instance());
  arc_bridge_service()->policy_instance()->OnPolicyUpdated();
}

}  // namespace arc
