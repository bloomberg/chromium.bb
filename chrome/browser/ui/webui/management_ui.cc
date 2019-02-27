// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/management_ui.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/dark_mode_handler.h"
#include "chrome/browser/ui/webui/management_ui_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/grit/chromium_strings.h"
#include "ui/chromeos/devicetype_utils.h"
#endif  // defined(OS_CHROMEOS)

namespace {

#if defined(OS_CHROMEOS)

base::string16 GetChromeOSManagementPageTitle() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  const auto device_type = ui::GetChromeOSDeviceTypeResourceId();
  if (!connector->IsEnterpriseManaged()) {
    return l10n_util::GetStringFUTF16(IDS_MANAGEMENT_TITLE,
                                      l10n_util::GetStringUTF16(device_type));
  }

  std::string display_domain = connector->GetEnterpriseDisplayDomain();

  if (display_domain.empty()) {
    if (!connector->IsActiveDirectoryManaged()) {
      return l10n_util::GetStringFUTF16(IDS_MANAGEMENT_TITLE_MANAGED,
                                        l10n_util::GetStringUTF16(device_type));
    }
    display_domain = connector->GetRealm();
  }

  return l10n_util::GetStringFUTF16(IDS_MANAGEMENT_TITLE_MANAGED_BY,
                                    l10n_util::GetStringUTF16(device_type),
                                    base::UTF8ToUTF16(display_domain));
}

#else   // if defined(OS_CHROMEOS)

// TODO(ydago): Fill in the function.
base::string16 GetBrowserManagementPageTitle() {
  return l10n_util::GetStringUTF16(IDS_MANAGEMENT_TITLE);
}
#endif  // defined(OS_CHROMEOS)

content::WebUIDataSource* CreateManagementUIHtmlSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIManagementHost);
#if defined(OS_CHROMEOS)
  source->AddString("title", GetChromeOSManagementPageTitle());
#else
  source->AddString("title", GetBrowserManagementPageTitle());
#endif  // defined(OS_CHROMEOS)
#if defined(OS_CHROMEOS)
  source->AddLocalizedString("deviceReporting",
                             IDS_MANAGEMENT_DEVICE_REPORTING);
  source->AddLocalizedString("deviceConfiguration",
                             IDS_MANAGEMENT_DEVICE_CONFIGURATION);
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
#endif
  source->AddLocalizedString("browserReporting",
                             IDS_MANAGEMENT_BROWSER_REPORTING);
  source->AddLocalizedString("browserReportingExplanation",
                             IDS_MANAGEMENT_BROWSER_REPORTING_EXPLANATION);
  source->AddLocalizedString("extensionReporting",
                             IDS_MANAGEMENT_EXTENSION_REPORTING);
  source->AddLocalizedString("extensionsInstalled",
                             IDS_MANAGEMENT_EXTENSIONS_INSTALLED);
  source->AddLocalizedString("extensionName", IDS_MANAGEMENT_EXTENSIONS_NAME);
  source->AddLocalizedString("extensionPermissions",
                             IDS_MANAGEMENT_EXTENSIONS_PERMISSIONS);
  source->AddLocalizedString("localTrustRoots",
                             IDS_MANAGEMENT_LOCAL_TRUST_ROOTS);
  source->AddLocalizedString("managementTrustRootsNotConfigured",
                             IDS_MANAGEMENT_TRUST_ROOTS_NOT_CONFIGURED);
  source->AddLocalizedString("managementDesktopMonitoringNotice",
                             IDS_MANAGEMENT_DESKTOP_MONITORING_NOTICE);
  source->AddLocalizedString("managementTitle", IDS_MANAGEMENT_TITLE);

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
      kManagementExtensionReportExtensionsAndPolicies,
      IDS_MANAGEMENT_EXTENSION_REPORT_EXTENSIONS_AND_POLICIES);

  source->AddLocalizedString(
      kManagementExtensionReportSafeBrowsingWarnings,
      IDS_MANAGEMENT_EXTENSION_REPORT_SAFE_BROWSING_WARNINGS);
  source->AddLocalizedString(kManagementExtensionReportPerfCrash,
                             IDS_MANAGEMENT_EXTENSION_REPORT_PERF_CRASH);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if defined(OS_CHROMEOS)
  source->AddLocalizedString("managementTrustRootsConfigured",
                             IDS_MANAGEMENT_TRUST_ROOTS_CONFIGURED);
#endif  // defined(OS_CHROMEOS)
  source->SetJsonPath("strings.js");
  // Add required resources.
  source->AddResourcePath("management_browser_proxy.html",
                          IDR_MANAGEMENT_BROWSER_PROXY_HTML);
  source->AddResourcePath("management_browser_proxy.js",
                          IDR_MANAGEMENT_BROWSER_PROXY_JS);
  source->AddResourcePath("management_ui.html", IDR_MANAGEMENT_UI_HTML);
  source->AddResourcePath("management_ui.js", IDR_MANAGEMENT_UI_JS);
  source->AddResourcePath("icons.html", IDR_MANAGEMENT_ICONS_HTML);
  source->SetDefaultResource(IDR_MANAGEMENT_HTML);
  source->UseGzip();
  return source;
}

}  // namespace

// static
base::RefCountedMemory* ManagementUI::GetFaviconResourceBytes(
    ui::ScaleFactor scale_factor) {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
      IDR_MANAGEMENT_FAVICON, scale_factor);
}

ManagementUI::ManagementUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<ManagementUIHandler>());
  content::WebUIDataSource* source = CreateManagementUIHtmlSource();
  DarkModeHandler::Initialize(web_ui, source);
  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), source);
}

ManagementUI::~ManagementUI() {}
