// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_POLICY_ARC_POLICY_BRIDGE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_POLICY_ARC_POLICY_BRIDGE_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/arc/arc_service.h"
#include "components/arc/common/policy.mojom.h"
#include "components/arc/instance_holder.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace policy {
class PolicyMap;
}  // namespace policy

namespace arc {

class ArcBridgeService;

// Constants for the ARC certs sync mode are defined in the policy, please keep
// its in sync.
enum ArcCertsSyncMode : int32_t {
  // Certificates sync is disabled.
  SYNC_DISABLED = 0,
  // Copy of CA certificates is enabled.
  COPY_CA_CERTS = 1
};

class ArcPolicyBridge : public ArcService,
                        public InstanceHolder<mojom::PolicyInstance>::Observer,
                        public mojom::PolicyHost,
                        public policy::PolicyService::Observer {
 public:
  explicit ArcPolicyBridge(ArcBridgeService* bridge_service);
  ArcPolicyBridge(ArcBridgeService* bridge_service,
                  policy::PolicyService* policy_service);
  ~ArcPolicyBridge() override;

  void OverrideIsManagedForTesting(bool is_managed);

  // InstanceHolder<mojom::PolicyInstance>::Observer overrides.
  void OnInstanceReady() override;
  void OnInstanceClosed() override;

  // PolicyHost overrides.
  void GetPolicies(const GetPoliciesCallback& callback) override;
  void ReportCompliance(const std::string& request,
                        const ReportComplianceCallback& callback) override;

  // PolicyService::Observer overrides.
  void OnPolicyUpdated(const policy::PolicyNamespace& ns,
                       const policy::PolicyMap& previous,
                       const policy::PolicyMap& current) override;

 private:
  void InitializePolicyService();

  // Returns the current policies for ARC, in the JSON dump format.
  std::string GetCurrentJSONPolicies() const;

  // Called when the compliance report from ARC is parsed.
  void OnReportComplianceParseSuccess(
      const ArcPolicyBridge::ReportComplianceCallback& callback,
      std::unique_ptr<base::Value> parsed_json);

  void UpdateComplianceReportMetrics(const base::DictionaryValue* report);

  mojo::Binding<PolicyHost> binding_;
  policy::PolicyService* policy_service_ = nullptr;
  bool is_managed_ = false;

  // Hash of the policies that were up to date when ARC started.
  std::string initial_policies_hash_;
  // Whether the UMA metric for the first successfully obtained compliance
  // report was already reported.
  bool first_compliance_timing_reported_ = false;
  // Hash of the policies that were up to date when the most recent policy
  // update notification was sent to ARC.
  std::string update_notification_policies_hash_;
  // The time of the policy update notification sent when the policy with hash
  // equal to |update_notification_policy_hash_| was active.
  base::Time update_notification_time_;
  // Whether the UMA metric for the successfully obtained compliance report
  // since the most recent policy update notificaton was already reported.
  bool compliance_since_update_timing_reported_ = false;

  // Must be the last member.
  base::WeakPtrFactory<ArcPolicyBridge> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcPolicyBridge);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_POLICY_ARC_POLICY_BRIDGE_H_
