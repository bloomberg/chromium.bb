// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/management_ui_handler.h"

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/profiles/profile_util.h"
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#include "chrome/browser/profiles/profile.h"

#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/device_status_collector.h"
#include "chrome/browser/chromeos/policy/policy_cert_service.h"
#include "chrome/browser/chromeos/policy/policy_cert_service_factory.h"
#include "chrome/browser/chromeos/policy/status_uploader.h"
#include "chrome/browser/chromeos/policy/system_log_uploader.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/grit/chromium_strings.h"
#include "components/user_manager/user_manager.h"
#endif  // defined(OS_CHROMEOS)

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#include "chrome/common/extensions/permissions/chrome_permission_message_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest.h"
#include "extensions/common/permissions/permission_message_provider.h"
#include "extensions/common/permissions/permissions_data.h"

const char kOnPremReportingExtensionStableId[] =
    "emahakmocgideepebncgnmlmliepgpgb";
const char kOnPremReportingExtensionBetaId[] =
    "kigjhoekjcpdfjpimbdjegmgecmlicaf";
const char kCloudReportingExtensionId[] = "oempjldejiginopiohodkdoklcjklbaa";
const char kPolicyKeyReportMachineIdData[] = "report_machine_id_data";
const char kPolicyKeyReportUserIdData[] = "report_user_id_data";
const char kPolicyKeyReportVersionData[] = "report_version_data";
const char kPolicyKeyReportPolicyData[] = "report_policy_data";
const char kPolicyKeyReportExtensionsData[] = "report_extensions_data";
const char kPolicyKeyReportSafeBrowsingData[] = "report_safe_browsing_data";
const char kPolicyKeyReportSystemTelemetryData[] =
    "report_system_telemetry_data";
const char kPolicyKeyReportUserBrowsingData[] = "report_user_browsing_data";

const char kManagementExtensionReportMachineName[] =
    "managementExtensionReportMachineName";
const char kManagementExtensionReportMachineNameAddress[] =
    "managementExtensionReportMachineNameAddress";
const char kManagementExtensionReportUsername[] =
    "managementExtensionReportUsername";
const char kManagementExtensionReportVersion[] =
    "managementExtensionReportVersion";
const char kManagementExtensionReportPolicies[] =
    "managementExtensionReportPolicies";
const char kManagementExtensionReportExtensionsPlugin[] =
    "managementExtensionReportExtensionsPlugin";
const char kManagementExtensionReportExtensionsAndPolicies[] =
    "managementExtensionReportExtensionsAndPolicies";
const char kManagementExtensionReportSafeBrowsingWarnings[] =
    "managementExtensionReportSafeBrowsingWarnings";
const char kManagementExtensionReportPerfCrash[] =
    "managementExtensionReportPerfCrash";

const char kReportingTypeDevice[] = "device";
const char kReportingTypeExtensions[] = "extensions";
const char kReportingTypeSecurity[] = "security";
const char kReportingTypeUser[] = "user";

enum class ReportingType { kDevice, kExtensions, kSecurity, kUser };

#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

const char kManagementLogUploadEnabled[] = "managementLogUploadEnabled";
const char kManagementReportActivityTimes[] = "managementReportActivityTimes";
const char kManagementReportHardwareStatus[] = "managementReportHardwareStatus";
const char kManagementReportNetworkInterfaces[] =
    "managementReportNetworkInterfaces";
const char kManagementReportUsers[] = "managementReportUsers";

namespace {

bool IsProfileManaged(Profile* profile) {
  return policy::ProfilePolicyConnectorFactory::IsProfileManaged(profile);
}

std::string GetAccountDomain(Profile* profile) {
  return gaia::ExtractDomainName(profile->GetProfileUserName());
}

#if defined(OS_CHROMEOS)

void AddChromeOSReportingDevice(base::Value* report_sources) {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();

  // Only check for report status in managed environment.
  if (!connector->IsEnterpriseManaged())
    return;

  policy::DeviceCloudPolicyManagerChromeOS* manager =
      connector->GetDeviceCloudPolicyManager();

  if (!manager)
    return;

  if (manager->GetSystemLogUploader()->upload_enabled()) {
    report_sources->GetList().push_back(
        base::Value(kManagementLogUploadEnabled));
  }

  const policy::DeviceStatusCollector* collector =
      manager->GetStatusUploader()->device_status_collector();

  if (collector->report_hardware_status()) {
    report_sources->GetList().push_back(
        base::Value(kManagementReportHardwareStatus));
  }
}

void AddChromeOSReportingSecurity(base::Value* report_sources) {}

void AddChromeOSReportingUserActivity(base::Value* report_sources) {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();

  // Only check for report status in managed environment.
  if (!connector->IsEnterpriseManaged())
    return;

  policy::DeviceCloudPolicyManagerChromeOS* manager =
      connector->GetDeviceCloudPolicyManager();

  if (!manager)
    return;

  const policy::DeviceStatusCollector* collector =
      manager->GetStatusUploader()->device_status_collector();

  if (collector->report_activity_times()) {
    report_sources->GetList().push_back(
        base::Value(kManagementReportActivityTimes));
  }
  if (collector->report_users()) {
    report_sources->GetList().push_back(base::Value(kManagementReportUsers));
  }
}

void AddChromeOSReportingWeb(base::Value* report_sources) {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();

  // Only check for report status in managed environment.
  if (!connector->IsEnterpriseManaged())
    return;

  policy::DeviceCloudPolicyManagerChromeOS* manager =
      connector->GetDeviceCloudPolicyManager();

  if (!manager)
    return;

  const policy::DeviceStatusCollector* collector =
      manager->GetStatusUploader()->device_status_collector();

  if (collector->report_network_interfaces()) {
    report_sources->GetList().push_back(
        base::Value(kManagementReportNetworkInterfaces));
  }
}

void AddChromeOSReportingInfo(base::Value* report_sources) {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();

  // Only check for report status in managed environment.
  if (!connector->IsEnterpriseManaged())
    return;

  policy::DeviceCloudPolicyManagerChromeOS* manager =
      connector->GetDeviceCloudPolicyManager();

  if (!manager)
    return;

  if (manager->GetSystemLogUploader()->upload_enabled()) {
    report_sources->GetList().push_back(
        base::Value(kManagementLogUploadEnabled));
  }

  const policy::DeviceStatusCollector* collector =
      manager->GetStatusUploader()->device_status_collector();

  if (collector->report_activity_times()) {
    report_sources->GetList().push_back(
        base::Value(kManagementReportActivityTimes));
  }
  if (collector->report_hardware_status()) {
    report_sources->GetList().push_back(
        base::Value(kManagementReportHardwareStatus));
  }
  if (collector->report_network_interfaces()) {
    report_sources->GetList().push_back(
        base::Value(kManagementReportNetworkInterfaces));
  }
  if (collector->report_users()) {
    report_sources->GetList().push_back(base::Value(kManagementReportUsers));
  }
}
#endif  // defined(OS_CHROMEOS)

#if BUILDFLAG(ENABLE_EXTENSIONS)
std::vector<base::Value> GetPermissionsForExtension(
    scoped_refptr<const extensions::Extension> extension) {
  std::vector<base::Value> permission_messages;
  // Only consider force installed extensions
  if (!extensions::Manifest::IsPolicyLocation(extension->location()))
    return permission_messages;

  extensions::PermissionIDSet permissions =
      extensions::PermissionMessageProvider::Get()->GetAllPermissionIDs(
          extension->permissions_data()->active_permissions(),
          extension->GetType());

  const extensions::PermissionMessages messages =
      extensions::PermissionMessageProvider::Get()
          ->GetPowerfulPermissionMessages(permissions);

  for (const auto& message : messages)
    permission_messages.push_back(base::Value(message.message()));

  return permission_messages;
}

base::Value GetPowerfulExtensions(const extensions::ExtensionSet& extensions) {
  base::Value powerful_extensions(base::Value::Type::LIST);

  for (const auto& extension : extensions) {
    std::vector<base::Value> permission_messages =
        GetPermissionsForExtension(extension);

    // Only show extension on page if there is at least one permission
    // message to show.
    if (!permission_messages.empty()) {
      std::unique_ptr<base::DictionaryValue> extension_to_add =
          extensions::util::GetExtensionInfo(extension.get());
      extension_to_add->SetKey("permissions",
                               base::Value(std::move(permission_messages)));
      powerful_extensions.GetList().push_back(std::move(*extension_to_add));
    }
  }

  return powerful_extensions;
}

const char* GetReportingTypeValue(ReportingType reportingType) {
  switch (reportingType) {
    case ReportingType::kDevice:
      return kReportingTypeDevice;
    case ReportingType::kExtensions:
      return kReportingTypeExtensions;
    case ReportingType::kSecurity:
      return kReportingTypeSecurity;
    case ReportingType::kUser:
      return kReportingTypeUser;
    default:
      return kReportingTypeSecurity;
  }
}

#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

}  // namespace

ManagementUIHandler::ManagementUIHandler() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  reporting_extension_ids_ = {kOnPremReportingExtensionStableId,
                              kOnPremReportingExtensionBetaId,
                              kCloudReportingExtensionId};
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
}

ManagementUIHandler::~ManagementUIHandler() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  RemoveObservers();
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
}

void ManagementUIHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getDeviceManagementStatus",
      base::BindRepeating(&ManagementUIHandler::HandleGetDeviceManagementStatus,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getExtensions",
      base::BindRepeating(&ManagementUIHandler::HandleGetExtensions,
                          base::Unretained(this)));
#if defined(OS_CHROMEOS)
  web_ui()->RegisterMessageCallback(
      "getLocalTrustRootsInfo",
      base::BindRepeating(&ManagementUIHandler::HandleGetLocalTrustRootsInfo,
                          base::Unretained(this)));
#endif  // defined(OS_CHROMEOS)
  web_ui()->RegisterMessageCallback(
      "getReportingDevice",
      base::BindRepeating(&ManagementUIHandler::HandleGetReportingDevice,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getReportingInfo",
      base::BindRepeating(&ManagementUIHandler::HandleGetReportingInfo,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getReportingSecurity",
      base::BindRepeating(&ManagementUIHandler::HandleGetReportingSecurity,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getReportingUserActivity",
      base::BindRepeating(&ManagementUIHandler::HandleGetReportingUserActivity,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getReportingWeb",
      base::BindRepeating(&ManagementUIHandler::HandleGetReportingWeb,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "initBrowserReportingInfo",
      base::BindRepeating(&ManagementUIHandler::HandleInitBrowserReportingInfo,
                          base::Unretained(this)));
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
void ManagementUIHandler::OnJavascriptDisallowed() {
  RemoveObservers();
}

void ManagementUIHandler::AddExtensionReportingInfo(
    base::Value* report_sources) {
  const extensions::Extension* cloud_reporting_extension =
      GetEnabledExtension(kCloudReportingExtensionId);

  const policy::PolicyService* policy_service = GetPolicyService();

  const policy::PolicyNamespace
      on_prem_reporting_extension_stable_policy_namespace =
          policy::PolicyNamespace(policy::POLICY_DOMAIN_EXTENSIONS,
                                  kOnPremReportingExtensionStableId);
  const policy::PolicyNamespace
      on_prem_reporting_extension_beta_policy_namespace =
          policy::PolicyNamespace(policy::POLICY_DOMAIN_EXTENSIONS,
                                  kOnPremReportingExtensionBetaId);

  const policy::PolicyMap& on_prem_reporting_extension_stable_policy_map =
      policy_service->GetPolicies(
          on_prem_reporting_extension_stable_policy_namespace);
  const policy::PolicyMap& on_prem_reporting_extension_beta_policy_map =
      policy_service->GetPolicies(
          on_prem_reporting_extension_beta_policy_namespace);

  const policy::PolicyMap* policy_maps[] = {
      &on_prem_reporting_extension_stable_policy_map,
      &on_prem_reporting_extension_beta_policy_map};

  const bool cloud_reporting_extension_installed =
      cloud_reporting_extension != nullptr;
  const struct {
    const char* policy_key;
    const char* message;
    const ReportingType reporting_type;
    const bool enabled_by_default;
  } report_definitions[] = {
      {kPolicyKeyReportMachineIdData, kManagementExtensionReportMachineName,
       ReportingType::kDevice, cloud_reporting_extension_installed},
      {kPolicyKeyReportMachineIdData,
       kManagementExtensionReportMachineNameAddress, ReportingType::kDevice,
       false},
      {kPolicyKeyReportVersionData, kManagementExtensionReportVersion,
       ReportingType::kDevice, cloud_reporting_extension_installed},
      {kPolicyKeyReportSystemTelemetryData, kManagementExtensionReportPerfCrash,
       ReportingType::kDevice, false},
      {kPolicyKeyReportUserIdData, kManagementExtensionReportUsername,
       ReportingType::kUser, cloud_reporting_extension_installed},
      {kPolicyKeyReportSafeBrowsingData,
       kManagementExtensionReportSafeBrowsingWarnings, ReportingType::kSecurity,
       cloud_reporting_extension_installed},
      {kPolicyKeyReportPolicyData, kManagementExtensionReportPolicies,
       ReportingType::kExtensions, cloud_reporting_extension_installed},
      {kPolicyKeyReportExtensionsData,
       kManagementExtensionReportExtensionsPlugin, ReportingType::kExtensions,
       cloud_reporting_extension_installed},
      {nullptr, kManagementExtensionReportExtensionsAndPolicies,
       ReportingType::kExtensions, false},
  };

  std::unordered_set<const char*> enabled_messages;

  for (auto& report_definition : report_definitions) {
    if (report_definition.enabled_by_default) {
      enabled_messages.insert(report_definition.message);
    } else if (report_definition.policy_key) {
      for (const policy::PolicyMap* policy_map : policy_maps) {
        const base::Value* policy_value =
            policy_map->GetValue(report_definition.policy_key);
        if (policy_value && policy_value->is_bool() &&
            policy_value->GetBool()) {
          enabled_messages.insert(report_definition.message);
          break;
        }
      }
    }
  }

  // The message with more data collected for kPolicyKeyReportMachineIdData
  // trumps the one with less data.
  if (enabled_messages.find(kManagementExtensionReportMachineNameAddress) !=
      enabled_messages.end()) {
    enabled_messages.erase(kManagementExtensionReportMachineName);
  }

  // When there extensions and policies reported, use the message combining
  // both.
  if (enabled_messages.find(kManagementExtensionReportPolicies) !=
          enabled_messages.end() &&
      enabled_messages.find(kManagementExtensionReportExtensionsPlugin) !=
          enabled_messages.end()) {
    enabled_messages.erase(kManagementExtensionReportPolicies);
    enabled_messages.erase(kManagementExtensionReportExtensionsPlugin);
    enabled_messages.insert(kManagementExtensionReportExtensionsAndPolicies);
  }

  for (auto& report_definition : report_definitions) {
    if (enabled_messages.find(report_definition.message) ==
        enabled_messages.end()) {
      continue;
    }

    base::Value data(base::Value::Type::DICTIONARY);
    data.SetKey("messageId", base::Value(report_definition.message));
    data.SetKey(
        "reportingType",
        base::Value(GetReportingTypeValue(report_definition.reporting_type)));
    report_sources->GetList().push_back(std::move(data));
  }
}

const policy::PolicyService* ManagementUIHandler::GetPolicyService() const {
  return policy::ProfilePolicyConnectorFactory::GetForBrowserContext(
             Profile::FromWebUI(web_ui()))
      ->policy_service();
}

const extensions::Extension* ManagementUIHandler::GetEnabledExtension(
    const std::string& extensionId) const {
  return extensions::ExtensionRegistry::Get(Profile::FromWebUI(web_ui()))
      ->GetExtensionById(kCloudReportingExtensionId,
                         extensions::ExtensionRegistry::ENABLED);
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

base::string16 ManagementUIHandler::GetEnterpriseManagementStatusString() {
  auto* profile = Profile::FromWebUI(web_ui());
  const bool account_managed = IsProfileManaged(profile);
  const std::string account_domain = GetAccountDomain(profile);
  bool profile_associated_with_gaia_account = true;
#if defined(OS_CHROMEOS)
  profile_associated_with_gaia_account =
      chromeos::IsProfileAssociatedWithGaiaAccount(profile);
#endif  // defined(OS_CHROMEOS)

  bool device_managed = false;
  std::string device_domain;
#if defined(OS_CHROMEOS)
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  device_managed = connector->IsEnterpriseManaged();
  if (device_managed)
    device_domain = connector->GetEnterpriseDisplayDomain();
  if (device_domain.empty() && connector->IsActiveDirectoryManaged())
    device_domain = connector->GetRealm();
#endif  // defined(OS_CHROMEOS)

  bool primary_user_managed = false;
  std::string primary_user_account_domain;
#if defined(OS_CHROMEOS)
  auto* primary_user = user_manager::UserManager::Get()->GetPrimaryUser();
  if (primary_user) {
    auto* primary_profile =
        chromeos::ProfileHelper::Get()->GetProfileByUser(primary_user);
    if (primary_profile) {
      primary_user_managed = IsProfileManaged(primary_profile);
      primary_user_account_domain = GetAccountDomain(primary_profile);
    }
  }
#endif  // defined(OS_CHROMEOS)

  if (device_managed) {
    DCHECK(!device_domain.empty());
    if (account_managed) {
      if (device_domain == account_domain ||
          !profile_associated_with_gaia_account) {
        return l10n_util::GetStringFUTF16(
            IDS_MANAGEMENT_DEVICE_AND_ACCOUNT_MANAGED_BY,
            base::UTF8ToUTF16(device_domain));
      }
      DCHECK(!account_domain.empty());
      return l10n_util::GetStringFUTF16(
          IDS_MANAGEMENT_DEVICE_MANAGED_BY_ACCOUNT_MANAGED_BY,
          base::UTF8ToUTF16(device_domain), base::UTF8ToUTF16(account_domain));
    }
    return l10n_util::GetStringFUTF16(IDS_MANAGEMENT_DEVICE_MANAGED_BY,
                                      base::UTF8ToUTF16(device_domain));
  }

  if (account_managed) {
    return l10n_util::GetStringFUTF16(IDS_MANAGEMENT_ACCOUNT_MANAGED_BY,
                                      base::UTF8ToUTF16(account_domain));
  }

  if (primary_user_managed) {
    return l10n_util::GetStringFUTF16(
        IDS_MANAGEMENT_ACCOUNT_MANAGED_BY,
        base::UTF8ToUTF16(primary_user_account_domain));
  }

  return l10n_util::GetStringUTF16(IDS_MANAGEMENT_DEVICE_NOT_MANAGED);
}

void ManagementUIHandler::HandleGetDeviceManagementStatus(
    const base::ListValue* args) {
  base::RecordAction(base::UserMetricsAction("ManagementPageViewed"));
  AllowJavascript();
  base::Value managed_string(GetEnterpriseManagementStatusString());
  ResolveJavascriptCallback(args->GetList()[0] /* callback_id */,
                            managed_string);
}

void ManagementUIHandler::HandleGetExtensions(const base::ListValue* args) {
  AllowJavascript();
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // List of all enabled extensions
  const extensions::ExtensionSet& extensions =
      extensions::ExtensionRegistry::Get(Profile::FromWebUI(web_ui()))
          ->enabled_extensions();

  base::Value powerful_extensions = GetPowerfulExtensions(extensions);

  ResolveJavascriptCallback(args->GetList()[0] /* callback_id */,
                            powerful_extensions);
#else
  ResolveJavascriptCallback(args->GetList()[0] /* callback_id */,
                            base::Value(base::Value::Type::LIST));
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
}

#if defined(OS_CHROMEOS)
void ManagementUIHandler::HandleGetLocalTrustRootsInfo(
    const base::ListValue* args) {
  CHECK_EQ(1U, args->GetSize());
  base::Value trust_roots_configured(false);
  AllowJavascript();

  policy::PolicyCertService* policy_service =
      policy::PolicyCertServiceFactory::GetForProfile(
          Profile::FromWebUI(web_ui()));
  if (policy_service && policy_service->has_policy_certificates())
    trust_roots_configured = base::Value(true);

  ResolveJavascriptCallback(args->GetList()[0] /* callback_id */,
                            trust_roots_configured);
}
#endif  // defined(OS_CHROMEOS)

void ManagementUIHandler::HandleGetReportingDevice(
    const base::ListValue* args) {
  base::Value report_sources(base::Value::Type::LIST);
  AllowJavascript();

// Only Chrome OS devices report status.
#if defined(OS_CHROMEOS)
  AddChromeOSReportingDevice(&report_sources);
#endif  // defined(OS_CHROMEOS)

  ResolveJavascriptCallback(args->GetList()[0] /* callback_id */,
                            report_sources);
}

void ManagementUIHandler::HandleGetReportingInfo(const base::ListValue* args) {
  base::Value report_sources(base::Value::Type::LIST);
  AllowJavascript();

// Only Chrome OS devices report status.
#if defined(OS_CHROMEOS)
  AddChromeOSReportingInfo(&report_sources);
#endif  // defined(OS_CHROMEOS)

  ResolveJavascriptCallback(args->GetList()[0] /* callback_id */,
                            report_sources);
}

void ManagementUIHandler::HandleGetReportingSecurity(
    const base::ListValue* args) {
  base::Value report_sources(base::Value::Type::LIST);
  AllowJavascript();

// Only Chrome OS devices report status.
#if defined(OS_CHROMEOS)
  AddChromeOSReportingSecurity(&report_sources);
#endif  // defined(OS_CHROMEOS)

  ResolveJavascriptCallback(args->GetList()[0] /* callback_id */,
                            report_sources);
}

void ManagementUIHandler::HandleGetReportingUserActivity(
    const base::ListValue* args) {
  base::Value report_sources(base::Value::Type::LIST);
  AllowJavascript();

// Only Chrome OS devices report status.
#if defined(OS_CHROMEOS)
  AddChromeOSReportingUserActivity(&report_sources);
#endif  // defined(OS_CHROMEOS)

  ResolveJavascriptCallback(args->GetList()[0] /* callback_id */,
                            report_sources);
}

void ManagementUIHandler::HandleGetReportingWeb(const base::ListValue* args) {
  base::Value report_sources(base::Value::Type::LIST);
  AllowJavascript();

// Only Chrome OS devices report status.
#if defined(OS_CHROMEOS)
  AddChromeOSReportingWeb(&report_sources);
#endif  // defined(OS_CHROMEOS)

  ResolveJavascriptCallback(args->GetList()[0] /* callback_id */,
                            report_sources);
}

void ManagementUIHandler::HandleInitBrowserReportingInfo(
    const base::ListValue* args) {
  base::Value report_sources(base::Value::Type::LIST);
  AllowJavascript();
#if BUILDFLAG(ENABLE_EXTENSIONS)
  AddExtensionReportingInfo(&report_sources);
  AddObservers();
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
  ResolveJavascriptCallback(args->GetList()[0] /* callback_id */,
                            report_sources);
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
void ManagementUIHandler::NotifyBrowserReportingInfoUpdated() {
  base::Value report_sources(base::Value::Type::LIST);
  AddExtensionReportingInfo(&report_sources);
  FireWebUIListener("browser-reporting-info-updated", report_sources);
}

void ManagementUIHandler::OnExtensionLoaded(
    content::BrowserContext* /*browser_context*/,
    const extensions::Extension* extension) {
  if (reporting_extension_ids_.find(extension->id()) !=
      reporting_extension_ids_.end()) {
    NotifyBrowserReportingInfoUpdated();
  }
}

void ManagementUIHandler::OnExtensionUnloaded(
    content::BrowserContext* /*browser_context*/,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason /*reason*/) {
  if (reporting_extension_ids_.find(extension->id()) !=
      reporting_extension_ids_.end()) {
    NotifyBrowserReportingInfoUpdated();
  }
}

void ManagementUIHandler::OnPolicyUpdated(
    const policy::PolicyNamespace& ns,
    const policy::PolicyMap& /*previous*/,
    const policy::PolicyMap& /*current*/) {
  const policy::PolicyNamespace
      on_prem_reporting_extension_stable_policy_namespace =
          policy::PolicyNamespace(policy::POLICY_DOMAIN_EXTENSIONS,
                                  kOnPremReportingExtensionStableId);
  const policy::PolicyNamespace
      on_prem_reporting_extension_beta_policy_namespace =
          policy::PolicyNamespace(policy::POLICY_DOMAIN_EXTENSIONS,
                                  kOnPremReportingExtensionBetaId);

  if (ns == on_prem_reporting_extension_stable_policy_namespace ||
      ns == on_prem_reporting_extension_beta_policy_namespace) {
    return;
  }

  NotifyBrowserReportingInfoUpdated();
}

void ManagementUIHandler::AddObservers() {
  if (has_observers_)
    return;

  has_observers_ = true;

  extensions::ExtensionRegistry::Get(Profile::FromWebUI(web_ui()))
      ->AddObserver(this);

  policy::PolicyService* policy_service =
      policy::ProfilePolicyConnectorFactory::GetForBrowserContext(
          Profile::FromWebUI(web_ui()))
          ->policy_service();
  policy_service->AddObserver(policy::POLICY_DOMAIN_EXTENSIONS, this);
}

void ManagementUIHandler::RemoveObservers() {
  if (!has_observers_)
    return;

  has_observers_ = false;

  extensions::ExtensionRegistry::Get(Profile::FromWebUI(web_ui()))
      ->RemoveObserver(this);

  policy::PolicyService* policy_service =
      policy::ProfilePolicyConnectorFactory::GetForBrowserContext(
          Profile::FromWebUI(web_ui()))
          ->policy_service();
  policy_service->RemoveObserver(policy::POLICY_DOMAIN_EXTENSIONS, this);
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
