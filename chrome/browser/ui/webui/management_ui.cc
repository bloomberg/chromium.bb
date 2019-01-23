// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/management_ui.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/management_ui_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui.h"
#include "extensions/buildflags/buildflags.h"

namespace {

content::WebUIDataSource* CreateManagementUIHtmlSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIManagementHost);
  source->AddLocalizedString("title", IDS_MANAGEMENT_TITLE);
  source->AddLocalizedString("deviceReporting",
                             IDS_MANAGEMENT_DEVICE_REPORTING);
  source->AddLocalizedString("browserReporting",
                             IDS_MANAGEMENT_BROWSER_REPORTING);
  source->AddLocalizedString("deviceConfiguration",
                             IDS_MANAGEMENT_DEVICE_CONFIGURATION);
  source->AddLocalizedString("extensionReporting",
                             IDS_MANAGEMENT_EXTENSION_REPORTING);
  source->AddLocalizedString("extensionsInstalled",
                             IDS_MANAGEMENT_EXTENSIONS_INSTALLED);
  source->AddLocalizedString("extensionName", IDS_MANAGEMENT_EXTENSIONS_NAME);
  source->AddLocalizedString("extensionPermissions",
                             IDS_MANAGEMENT_EXTENSIONS_PERMISSIONS);
  source->AddLocalizedString(kManagementLogUploadEnabled,
                             IDS_MANAGEMENT_LOG_UPLOAD_ENABLED);
  source->AddLocalizedString(kManagementReportActivityTimes,
                             IDS_MANAGEMENT_REPORT_DEVICE_ACTIVITY_TIMES);
  source->AddLocalizedString(kManagementReportHardwareStatus,
                             IDS_MANAGEMENT_REPORT_DEVICE_HARDWARE_STATUS);
  source->AddLocalizedString(kManagementReportNetworkInterfaces,
                             IDS_MANAGEMENT_REPORT_DEVICE_NETWORK_INTERFACES);
  source->AddLocalizedString(kManagementReportUsers,
                             IDS_MANAGEMENT_REPORT_DEVICE_USERS);
  source->AddLocalizedString("localTrustRoots",
                             IDS_MANAGEMENT_LOCAL_TRUST_ROOTS);
  source->AddLocalizedString("managementTrustRootsNotConfigured",
                             IDS_MANAGEMENT_TRUST_ROOTS_NOT_CONFIGURED);
  source->AddLocalizedString("managementDesktopMonitoringNotice",
                             IDS_MANAGEMENT_DESKTOP_MONITORING_NOTICE);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  source->AddLocalizedString(kManagementExtensionReportMachineName,
                             IDS_MANAGEMENT_EXTENSION_REPORT_MACHINE_NAME);
  source->AddLocalizedString(
      kManagementExtensionReportMachineNameAddress,
      IDS_MANAGEMENT_EXTENSION_REPORT_MACHINE_NAME_ADDRESS);
  source->AddLocalizedString(kManagementExtensionReportUsername,
                             IDS_MANAGEMENT_EXTENSION_REPORT_USERNAME);
  source->AddLocalizedString(kManagementExtensionReportVersion,
                             IDS_MANAGEMENT_EXTENSION_REPORT_VERSION);
  source->AddLocalizedString(kManagementExtensionReportPolicies,
                             IDS_MANAGEMENT_EXTENSION_REPORT_POLICIES);
  source->AddLocalizedString(
      kManagementExtensionReportExtensionsPlugin,
      IDS_MANAGEMENT_EXTENSION_REPORT_EXTENSIONS_PLUGINS);
  source->AddLocalizedString(
      kManagementExtensionReportSafeBrowsingWarnings,
      IDS_MANAGEMENT_EXTENSION_REPORT_SAFE_BROWSING_WARNINGS);
  source->AddLocalizedString(kManagementExtensionReportPerfCrash,
                             IDS_MANAGEMENT_EXTENSION_REPORT_PERF_CRASH);
  source->AddLocalizedString(
      kManagementExtensionReportWebsiteUsageStatistics,
      IDS_MANAGEMENT_EXTENSION_REPORT_WEBSITE_USAGE_STATISTICS);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if defined(OS_CHROMEOS)
  source->AddLocalizedString("managementTrustRootsConfigured",
                             IDS_MANAGEMENT_TRUST_ROOTS_CONFIGURED);
#endif  // defined(OS_CHROMEOS)
  source->SetJsonPath("strings.js");
  // Add required resources.
  source->AddResourcePath("management.css", IDR_MANAGEMENT_CSS);
  source->AddResourcePath("management.js", IDR_MANAGEMENT_JS);
  source->SetDefaultResource(IDR_MANAGEMENT_HTML);
  source->UseGzip();
  return source;
}

}  // namespace

ManagementUI::ManagementUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<ManagementUIHandler>());
  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui),
                                CreateManagementUIHtmlSource());
}

ManagementUI::~ManagementUI() {}
