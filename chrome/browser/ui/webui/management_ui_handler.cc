// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/management_ui_handler.h"

#include <algorithm>
#include <memory>
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
const char kManagementExtensionReportSafeBrowsingWarnings[] =
    "managementExtensionReportSafeBrowsingWarnings";
const char kManagementExtensionReportPerfCrash[] =
    "managementExtensionReportPerfCrash";
const char kManagementExtensionReportWebsiteUsageStatistics[] =
    "managementExtensionReportTimePerSite";
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

base::string16 GetManagementPageTitle() {
  return l10n_util::GetStringFUTF16(
      IDS_MANAGEMENT_TITLE,
      l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_OS_NAME));
}

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

const char kOnPremReportingExtensionStableId[] =
    "emahakmocgideepebncgnmlmliepgpgb";
const char kOnPremReportingExtensionBetaId[] =
    "kigjhoekjcpdfjpimbdjegmgecmlicaf";
const char kCloudReportingExtensionId[] = "empjldejiginopiohodkdoklcjklbaa";
const char kPolicyKeyReportMachineIdData[] = "report_machine_id_data";
const char kPolicyKeyReportUserIdData[] = "report_user_id_data";
const char kPolicyKeyReportVersionData[] = "report_version_data";
const char kPolicyKeyReportPolicyData[] = "report_policy_data";
const char kPolicyKeyReportExtensionsData[] = "report_extensions_data";
const char kPolicyKeyReportSafeBrowsingData[] = "report_safe_browsing_data";
const char kPolicyKeyReportSystemTelemetryData[] =
    "report_system_telemetry_data";
const char kPolicyKeyReportUserBrowsingData[] = "report_user_browsing_data";

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
      base::Value extension_to_add(base::Value::Type::DICTIONARY);
      extension_to_add.SetKey("name", base::Value(extension->name()));
      extension_to_add.SetKey("permissions",
                              base::Value(std::move(permission_messages)));
      powerful_extensions.GetList().push_back(std::move(extension_to_add));
    }
  }

  return powerful_extensions;
}

#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

}  // namespace

ManagementUIHandler::ManagementUIHandler() {}

ManagementUIHandler::~ManagementUIHandler() {}

void ManagementUIHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getManagementTitle",
      base::BindRepeating(&ManagementUIHandler::HandleGetManagementTitle,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getDeviceManagementStatus",
      base::BindRepeating(&ManagementUIHandler::HandleGetDeviceManagementStatus,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getReportingDevice",
      base::BindRepeating(&ManagementUIHandler::HandleGetReportingDevice,
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
      "getReportingInfo",
      base::BindRepeating(&ManagementUIHandler::HandleGetReportingInfo,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getBrowserReportingInfo",
      base::BindRepeating(&ManagementUIHandler::HandleGetBrowserReportingInfo,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getExtensions",
      base::BindRepeating(&ManagementUIHandler::HandleGetExtensions,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getLocalTrustRootsInfo",
      base::BindRepeating(&ManagementUIHandler::HandleGetLocalTrustRootsInfo,
                          base::Unretained(this)));
}

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

void ManagementUIHandler::HandleGetManagementTitle(
    const base::ListValue* args) {
  AllowJavascript();
#if defined(OS_CHROMEOS)
  base::Value title(GetManagementPageTitle());
  ResolveJavascriptCallback(args->GetList()[0] /* callback_id */, title);
#else
  RejectJavascriptCallback(
      args->GetList()[0] /* callback_id */,
      base::Value("No device management title on Chrome desktop"));
#endif  // defined(OS_CHROMEOS)
}

void ManagementUIHandler::HandleGetDeviceManagementStatus(
    const base::ListValue* args) {
  base::RecordAction(base::UserMetricsAction("ManagementPageViewed"));
  AllowJavascript();
  base::Value managed_string(GetEnterpriseManagementStatusString());
  ResolveJavascriptCallback(args->GetList()[0] /* callback_id */,
                            managed_string);
}

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

void ManagementUIHandler::HandleGetBrowserReportingInfo(
    const base::ListValue* args) {
  base::Value report_sources(base::Value::Type::LIST);
  AllowJavascript();

// Only browsers with extensions enabled report status.
#if BUILDFLAG(ENABLE_EXTENSIONS)
  AddExtensionReportingInfo(&report_sources);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  ResolveJavascriptCallback(args->GetList()[0] /* callback_id */,
                            report_sources);
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

void ManagementUIHandler::HandleGetLocalTrustRootsInfo(
    const base::ListValue* args) {
  CHECK_EQ(1U, args->GetSize());
  base::Value trust_roots_configured(false);
  AllowJavascript();
// Only Chrome OS could have installed trusted certificates.
#if defined(OS_CHROMEOS)
  policy::PolicyCertService* policy_service =
      policy::PolicyCertServiceFactory::GetForProfile(
          Profile::FromWebUI(web_ui()));
  if (policy_service && policy_service->has_policy_certificates())
    trust_roots_configured = base::Value(true);
#endif  // defined(OS_CHROMEOS)

  ResolveJavascriptCallback(args->GetList()[0] /* callback_id */,
                            trust_roots_configured);
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
void ManagementUIHandler::AddExtensionReportingInfo(
    base::Value* report_sources) {
  std::unordered_set<std::string> messages;

  const extensions::Extension* cloud_reporting_extension =
      extensions::ExtensionRegistry::Get(Profile::FromWebUI(web_ui()))
          ->GetExtensionById(kCloudReportingExtensionId,
                             extensions::ExtensionRegistry::ENABLED);

  if (cloud_reporting_extension != nullptr) {
    messages.insert(kManagementExtensionReportMachineName);
    messages.insert(kManagementExtensionReportUsername);
    messages.insert(kManagementExtensionReportVersion);
    messages.insert(kManagementExtensionReportPolicies);
    messages.insert(kManagementExtensionReportExtensionsPlugin);
    messages.insert(kManagementExtensionReportSafeBrowsingWarnings);
  }

  policy::PolicyNamespace on_prem_reporting_extension_stable_policy_namespace =
      policy::PolicyNamespace(policy::POLICY_DOMAIN_EXTENSIONS,
                              kOnPremReportingExtensionStableId);
  policy::PolicyNamespace on_prem_reporting_extension_beta_policy_namespace =
      policy::PolicyNamespace(policy::POLICY_DOMAIN_EXTENSIONS,
                              kOnPremReportingExtensionBetaId);

  const policy::PolicyService* policyService =
      policy::ProfilePolicyConnectorFactory::GetForBrowserContext(
          Profile::FromWebUI(web_ui()))
          ->policy_service();

  const policy::PolicyMap& on_prem_reporting_extension_stable_policy_map =
      policyService->GetPolicies(
          on_prem_reporting_extension_stable_policy_namespace);
  const policy::PolicyMap& on_prem_reporting_extension_beta_policy_map =
      policyService->GetPolicies(
          on_prem_reporting_extension_beta_policy_namespace);

  const policy::PolicyMap* policy_maps[] = {
      &on_prem_reporting_extension_stable_policy_map,
      &on_prem_reporting_extension_beta_policy_map};

  static const struct {
    const char* policy_key;
    const char* message;
  } policy_keys_messages[] = {
      {kPolicyKeyReportMachineIdData,
       kManagementExtensionReportMachineNameAddress},
      {kPolicyKeyReportUserIdData, kManagementExtensionReportUsername},
      {kPolicyKeyReportVersionData, kManagementExtensionReportVersion},
      {kPolicyKeyReportPolicyData, kManagementExtensionReportPolicies},
      {kPolicyKeyReportExtensionsData,
       kManagementExtensionReportExtensionsPlugin},
      {kPolicyKeyReportSafeBrowsingData,
       kManagementExtensionReportSafeBrowsingWarnings},
      {kPolicyKeyReportSystemTelemetryData,
       kManagementExtensionReportPerfCrash},
      {kPolicyKeyReportUserBrowsingData,
       kManagementExtensionReportWebsiteUsageStatistics}};

  for (const auto* policy_map : policy_maps) {
    for (const auto& policy_key_message : policy_keys_messages) {
      const base::Value* policy_value =
          policy_map->GetValue(policy_key_message.policy_key);
      if (policy_value && policy_value->is_bool() && policy_value->GetBool())
        messages.insert(policy_key_message.message);
    }
  }

  if (messages.find(kManagementExtensionReportMachineNameAddress) !=
      messages.end()) {
    messages.erase(kManagementExtensionReportMachineName);
  }

  for (const auto& message : messages) {
    report_sources->GetList().push_back(base::Value(message));
  }
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
