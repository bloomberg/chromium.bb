// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/management_ui_handler.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/device_status_collector.h"
#include "chrome/browser/chromeos/policy/status_uploader.h"
#include "chrome/browser/chromeos/policy/system_log_uploader.h"
#endif  // defined(OS_CHROMEOS)

const char kManagementLogUploadEnabled[] = "managementLogUploadEnabled";
const char kManagementReportActivityTimes[] = "managementReportActivityTimes";
const char kManagementReportHardwareStatus[] = "managementReportHardwareStatus";
const char kManagementReportNetworkInterfaces[] =
    "managementReportNetworkInterfaces";
const char kManagementReportUsers[] = "managementReportUsers";

namespace {

#if defined(OS_CHROMEOS)
base::string16 GetEnterpriseDisplayDomain(
    policy::BrowserPolicyConnectorChromeOS* connector) {
  if (!connector->IsEnterpriseManaged())
    return l10n_util::GetStringUTF16(IDS_MANAGEMENT_DEVICE_NOT_MANAGED);

  std::string display_domain = connector->GetEnterpriseDisplayDomain();

  if (display_domain.empty()) {
    if (!connector->IsActiveDirectoryManaged())
      return l10n_util::GetStringUTF16(IDS_MANAGEMENT_DEVICE_MANAGED);

    display_domain = connector->GetRealm();
  }

  return l10n_util::GetStringFUTF16(IDS_MANAGEMENT_DEVICE_MANAGED_BY,
                                    base::UTF8ToUTF16(display_domain));
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

}  // namespace

ManagementUIHandler::ManagementUIHandler() {}

ManagementUIHandler::~ManagementUIHandler() {}

void ManagementUIHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getDeviceManagementStatus",
      base::BindRepeating(&ManagementUIHandler::HandleGetDeviceManagementStatus,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getReportingInfo",
      base::BindRepeating(&ManagementUIHandler::HandleGetReportingInfo,
                          base::Unretained(this)));
}

void ManagementUIHandler::HandleGetDeviceManagementStatus(
    const base::ListValue* args) {
  AllowJavascript();

#if defined(OS_CHROMEOS)
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();

  base::Value managed_string(GetEnterpriseDisplayDomain(connector));
  ResolveJavascriptCallback(args->GetList()[0] /* callback_id */,
                            managed_string);
#else
  RejectJavascriptCallback(
      args->GetList()[0] /* callback_id */,
      base::Value("No device management status on Chrome desktop"));
#endif  // defined(OS_CHROMEOS)
}

void ManagementUIHandler::HandleGetReportingInfo(const base::ListValue* args) {
  base::Value report_sources(base::Value::Type::LIST);

// Only Chrome OS devices report status.
#if defined(OS_CHROMEOS)
  AddChromeOSReportingInfo(&report_sources);
#endif  // defined(OS_CHROMEOS)

  ResolveJavascriptCallback(args->GetList()[0] /* callback_id */,
                            report_sources);
}
