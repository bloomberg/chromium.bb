// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/policy/arc_policy_bridge.h"

#include <memory>
#include <utility>

#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "chromeos/network/onc/onc_utils.h"
#include "components/arc/arc_bridge_service.h"
#include "components/onc/onc_constants.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/safe_json/safe_json_parser.h"
#include "components/user_manager/user.h"
#include "mojo/public/cpp/bindings/string.h"

namespace arc {

namespace {

constexpr char kArcGlobalAppRestrictions[] = "globalAppRestrictions";
constexpr char kArcCaCerts[] = "caCerts";
constexpr char kNonComplianceDetails[] = "nonComplianceDetails";
constexpr char kNonComplianceReason[] = "nonComplianceReason";
constexpr char kPolicyCompliantJson[] = "{ \"policyCompliant\": true }";
constexpr char kPolicyNonCompliantJson[] = "{ \"policyCompliant\": false }";
// Value from CloudDPS NonComplianceDetail.NonComplianceReason enum.
constexpr int kAppNotInstalled = 5;

// invert_bool_value: If the Chrome policy and the ARC policy with boolean value
// have opposite semantics, set this to true so the bool is inverted before
// being added. Otherwise, set it to false.
void MapBoolToBool(const std::string& arc_policy_name,
                   const std::string& policy_name,
                   const policy::PolicyMap& policy_map,
                   bool invert_bool_value,
                   base::DictionaryValue* filtered_policies) {
  const base::Value* const policy_value = policy_map.GetValue(policy_name);
  if (!policy_value)
    return;
  if (!policy_value->IsType(base::Value::TYPE_BOOLEAN)) {
    LOG(ERROR) << "Policy " << policy_name << " is not a boolean.";
    return;
  }
  bool bool_value;
  policy_value->GetAsBoolean(&bool_value);
  filtered_policies->SetBoolean(arc_policy_name,
                                bool_value != invert_bool_value);
}

// int_true: value of Chrome OS policy for which arc policy is set to true.
// It is set to false for all other values.
void MapIntToBool(const std::string& arc_policy_name,
                  const std::string& policy_name,
                  const policy::PolicyMap& policy_map,
                  int int_true,
                  base::DictionaryValue* filtered_policies) {
  const base::Value* const policy_value = policy_map.GetValue(policy_name);
  if (!policy_value)
    return;
  if (!policy_value->IsType(base::Value::TYPE_INTEGER)) {
    LOG(ERROR) << "Policy " << policy_name << " is not an integer.";
    return;
  }
  int int_value;
  policy_value->GetAsInteger(&int_value);
  filtered_policies->SetBoolean(arc_policy_name, int_value == int_true);
}

void AddGlobalAppRestriction(const std::string& arc_app_restriction_name,
                             const std::string& policy_name,
                             const policy::PolicyMap& policy_map,
                             base::DictionaryValue* filtered_policies) {
  const base::Value* const policy_value = policy_map.GetValue(policy_name);
  if (policy_value) {
    base::DictionaryValue* global_app_restrictions = nullptr;
    if (!filtered_policies->GetDictionary(kArcGlobalAppRestrictions,
                                          &global_app_restrictions)) {
      global_app_restrictions = new base::DictionaryValue();
      filtered_policies->Set(kArcGlobalAppRestrictions,
                             global_app_restrictions);
    }
    global_app_restrictions->SetWithoutPathExpansion(
        arc_app_restriction_name, policy_value->CreateDeepCopy());
  }
}

void AddOncCaCertsToPolicies(const policy::PolicyMap& policy_map,
                             base::DictionaryValue* filtered_policies) {
  const base::Value* const policy_value =
      policy_map.GetValue(policy::key::kArcCertificatesSyncMode);
  int32_t mode = ArcCertsSyncMode::SYNC_DISABLED;

  // Old certs should be uninstalled if the sync is disabled or policy is not
  // set.
  if (!policy_value || !policy_value->GetAsInteger(&mode) ||
      mode != ArcCertsSyncMode::COPY_CA_CERTS) {
    return;
  }

  // Importing CA certificates from device policy is not allowed.
  // Import only from user policy.
  const base::Value* onc_policy_value =
      policy_map.GetValue(policy::key::kOpenNetworkConfiguration);
  if (!onc_policy_value) {
    VLOG(1) << "onc policy is not set.";
    return;
  }
  std::string onc_blob;
  if (!onc_policy_value->GetAsString(&onc_blob)) {
    LOG(ERROR) << "Value of onc policy has invalid format.";
    return;
  }

  base::ListValue certificates;
  {
    base::ListValue unused_network_configs;
    base::DictionaryValue unused_global_network_config;
    if (!chromeos::onc::ParseAndValidateOncForImport(
            onc_blob, onc::ONCSource::ONC_SOURCE_USER_POLICY,
            "" /* no passphrase */, &unused_network_configs,
            &unused_global_network_config, &certificates)) {
      LOG(ERROR) << "Value of onc policy has invalid format =" << onc_blob;
    }
  }

  std::unique_ptr<base::ListValue> ca_certs(
      base::MakeUnique<base::ListValue>());
  for (const auto& entry : certificates) {
    const base::DictionaryValue* certificate = nullptr;
    if (!entry->GetAsDictionary(&certificate)) {
      DLOG(FATAL) << "Value of a certificate entry is not a dictionary "
                  << "value.";
      continue;
    }

    std::string cert_type;
    certificate->GetStringWithoutPathExpansion(::onc::certificate::kType,
                                               &cert_type);
    if (cert_type != ::onc::certificate::kAuthority)
      continue;

    const base::ListValue* trust_list = nullptr;
    if (!certificate->GetListWithoutPathExpansion(
            ::onc::certificate::kTrustBits, &trust_list)) {
      continue;
    }

    bool web_trust_flag = false;
    for (const auto& list_val : *trust_list) {
      std::string trust_type;
      if (!list_val->GetAsString(&trust_type))
        NOTREACHED();

      if (trust_type == ::onc::certificate::kWeb) {
        // "Web" implies that the certificate is to be trusted for SSL
        // identification.
        web_trust_flag = true;
        break;
      }
    }
    if (!web_trust_flag)
      continue;

    std::string x509_data;
    if (!certificate->GetStringWithoutPathExpansion(::onc::certificate::kX509,
                                                    &x509_data)) {
      continue;
    }

    base::DictionaryValue data;
    data.SetString("X509", x509_data);
    ca_certs->Append(data.CreateDeepCopy());
  }
  filtered_policies->Set(kArcCaCerts, std::move(ca_certs));
}

std::string GetFilteredJSONPolicies(const policy::PolicyMap& policy_map) {
  base::DictionaryValue filtered_policies;
  // Parse ArcPolicy as JSON string before adding other policies to the
  // dictionary.
  const base::Value* const app_policy_value =
      policy_map.GetValue(policy::key::kArcPolicy);
  if (app_policy_value) {
    std::string app_policy_string;
    app_policy_value->GetAsString(&app_policy_string);
    std::unique_ptr<base::DictionaryValue> app_policy_dict =
        base::DictionaryValue::From(base::JSONReader::Read(app_policy_string));
    if (app_policy_dict) {
      // Need a deep copy of all values here instead of doing a swap, because
      // JSONReader::Read constructs a dictionary whose StringValues are
      // JSONStringValues which are based on StringPiece instead of string.
      filtered_policies.MergeDictionary(app_policy_dict.get());
    } else {
      LOG(ERROR) << "Value of ArcPolicy has invalid format: "
                 << app_policy_string;
    }
  }

  // Keep them sorted by the ARC policy names.
  MapBoolToBool("cameraDisabled", policy::key::kVideoCaptureAllowed, policy_map,
                true, &filtered_policies);
  MapBoolToBool("debuggingFeaturesDisabled",
                policy::key::kDeveloperToolsDisabled, policy_map, false,
                &filtered_policies);
  MapBoolToBool("screenCaptureDisabled", policy::key::kDisableScreenshots,
                policy_map, false, &filtered_policies);
  MapIntToBool("shareLocationDisabled", policy::key::kDefaultGeolocationSetting,
               policy_map, 2 /*BlockGeolocation*/, &filtered_policies);
  MapBoolToBool("unmuteMicrophoneDisabled", policy::key::kAudioCaptureAllowed,
                policy_map, true, &filtered_policies);
  MapBoolToBool("mountPhysicalMediaDisabled",
                policy::key::kExternalStorageDisabled, policy_map, false,
                &filtered_policies);

  // Add global app restrictions.
  AddGlobalAppRestriction("com.android.browser:URLBlacklist",
                          policy::key::kURLBlacklist, policy_map,
                          &filtered_policies);
  AddGlobalAppRestriction("com.android.browser:URLWhitelist",
                          policy::key::kURLWhitelist, policy_map,
                          &filtered_policies);

  // Add CA certificates.
  AddOncCaCertsToPolicies(policy_map, &filtered_policies);

  std::string policy_json;
  JSONStringValueSerializer serializer(&policy_json);
  serializer.Serialize(filtered_policies);
  return policy_json;
}

}  // namespace

ArcPolicyBridge::ArcPolicyBridge(ArcBridgeService* bridge_service)
    : ArcService(bridge_service), binding_(this) {
  VLOG(2) << "ArcPolicyBridge::ArcPolicyBridge";
  arc_bridge_service()->policy()->AddObserver(this);
}

ArcPolicyBridge::ArcPolicyBridge(ArcBridgeService* bridge_service,
                                 policy::PolicyService* policy_service)
    : ArcService(bridge_service),
      binding_(this),
      policy_service_(policy_service) {
  VLOG(2) << "ArcPolicyBridge::ArcPolicyBridge(bridge_service, policy_service)";
  arc_bridge_service()->policy()->AddObserver(this);
}

ArcPolicyBridge::~ArcPolicyBridge() {
  VLOG(2) << "ArcPolicyBridge::~ArcPolicyBridge";
  arc_bridge_service()->policy()->RemoveObserver(this);
}

// static
void ArcPolicyBridge::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kArcPolicyCompliant, false);
}

void ArcPolicyBridge::OverrideIsManagedForTesting(bool is_managed) {
  is_managed_ = is_managed;
}

void ArcPolicyBridge::OnInstanceReady() {
  VLOG(1) << "ArcPolicyBridge::OnPolicyInstanceReady";
  if (policy_service_ == nullptr) {
    InitializePolicyService();
  }
  policy_service_->AddObserver(policy::POLICY_DOMAIN_CHROME, this);

  mojom::PolicyInstance* const policy_instance =
      arc_bridge_service()->policy()->GetInstanceForMethod("Init");
  DCHECK(policy_instance);
  policy_instance->Init(binding_.CreateInterfacePtrAndBind());
}

void ArcPolicyBridge::OnInstanceClosed() {
  VLOG(1) << "ArcPolicyBridge::OnPolicyInstanceClosed";
  policy_service_->RemoveObserver(policy::POLICY_DOMAIN_CHROME, this);
  policy_service_ = nullptr;
}

void ArcPolicyBridge::GetPolicies(const GetPoliciesCallback& callback) {
  VLOG(1) << "ArcPolicyBridge::GetPolicies";
  if (!is_managed_) {
    callback.Run(std::string());
    return;
  }
  const policy::PolicyNamespace policy_namespace(policy::POLICY_DOMAIN_CHROME,
                                                 std::string());
  const policy::PolicyMap& policy_map =
      policy_service_->GetPolicies(policy_namespace);
  callback.Run(GetFilteredJSONPolicies(policy_map));
}

void OnReportComplianceParseSuccess(
    const ArcPolicyBridge::ReportComplianceCallback& callback,
    std::unique_ptr<base::Value> parsed_json) {
  const base::DictionaryValue* dict;
  if (!parsed_json || !parsed_json->GetAsDictionary(&dict)) {
    callback.Run(kPolicyNonCompliantJson);
    return;
  }

  const user_manager::User* const primary_user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  Profile* const profile =
      chromeos::ProfileHelper::Get()->GetProfileByUser(primary_user);
  if (!dict || !profile) {
    callback.Run(kPolicyNonCompliantJson);
    return;
  }

  // ChromeOS is 'compliant' with the report if all "nonComplianceDetails"
  // entries have APP_NOT_INSTALLED reason.
  bool compliant = true;
  const base::ListValue* value = nullptr;
  dict->GetList(kNonComplianceDetails, &value);
  if (!dict->empty() && value && !value->empty()) {
    for (const auto& entry : *value) {
      const base::DictionaryValue* entry_dict;
      int reason = 0;
      if (entry->GetAsDictionary(&entry_dict) &&
          entry_dict->GetInteger(kNonComplianceReason, &reason) &&
          reason != kAppNotInstalled) {
        compliant = false;
        break;
      }
    }
  }
  profile->GetPrefs()->SetBoolean(prefs::kArcPolicyCompliant, compliant);
  callback.Run(compliant ? kPolicyCompliantJson : kPolicyNonCompliantJson);
}

void OnReportComplianceParseFailure(
    const ArcPolicyBridge::ReportComplianceCallback& callback,
    const std::string& error) {
  callback.Run(kPolicyNonCompliantJson);
}

void ArcPolicyBridge::ReportCompliance(
    const std::string& request,
    const ReportComplianceCallback& callback) {
  VLOG(1) << "ArcPolicyBridge::ReportCompliance";
  safe_json::SafeJsonParser::Parse(
      request, base::Bind(&OnReportComplianceParseSuccess, callback),
      base::Bind(&OnReportComplianceParseFailure, callback));
}

void ArcPolicyBridge::OnPolicyUpdated(const policy::PolicyNamespace& ns,
                                      const policy::PolicyMap& previous,
                                      const policy::PolicyMap& current) {
  VLOG(1) << "ArcPolicyBridge::OnPolicyUpdated";
  auto* instance =
      arc_bridge_service()->policy()->GetInstanceForMethod("OnPolicyUpdated");
  if (!instance)
    return;
  instance->OnPolicyUpdated();
}

void ArcPolicyBridge::InitializePolicyService() {
  const user_manager::User* const primary_user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  Profile* const profile =
      chromeos::ProfileHelper::Get()->GetProfileByUser(primary_user);
  auto* profile_policy_connector =
      policy::ProfilePolicyConnectorFactory::GetForBrowserContext(profile);
  policy_service_ = profile_policy_connector->policy_service();
  is_managed_ = profile_policy_connector->IsManaged();
}

}  // namespace arc
