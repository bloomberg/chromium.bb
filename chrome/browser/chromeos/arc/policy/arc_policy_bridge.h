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
#include "components/arc/common/policy.mojom.h"
#include "components/arc/connection_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace content {
class BrowserContext;
}  // namespace content

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

class ArcPolicyBridge : public KeyedService,
                        public ConnectionObserver<mojom::PolicyInstance>,
                        public mojom::PolicyHost,
                        public policy::PolicyService::Observer {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcPolicyBridge* GetForBrowserContext(
      content::BrowserContext* context);

  ArcPolicyBridge(content::BrowserContext* context,
                  ArcBridgeService* bridge_service);
  ArcPolicyBridge(content::BrowserContext* context,
                  ArcBridgeService* bridge_service,
                  policy::PolicyService* policy_service);
  ~ArcPolicyBridge() override;

  void OverrideIsManagedForTesting(bool is_managed);

  // ConnectionObserver<mojom::PolicyInstance> overrides.
  void OnConnectionReady() override;
  void OnConnectionClosed() override;

  // PolicyHost overrides.
  void GetPolicies(GetPoliciesCallback callback) override;
  void ReportCompliance(const std::string& request,
                        ReportComplianceCallback callback) override;

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
      base::OnceCallback<void(const std::string&)> callback,
      std::unique_ptr<base::Value> parsed_json);

  void UpdateComplianceReportMetrics(const base::DictionaryValue* report);

  content::BrowserContext* const context_;
  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.

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
