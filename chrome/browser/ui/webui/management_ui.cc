// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/management_ui.h"

#include <memory>

#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/dark_mode_handler.h"
#include "chrome/browser/ui/webui/localized_string.h"
#include "chrome/browser/ui/webui/management_ui_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/safe_browsing/common/safebrowsing_constants.h"
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
    return l10n_util::GetStringFUTF16(IDS_MANAGEMENT_NOT_MANAGED_TITLE,
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

  return l10n_util::GetStringFUTF16(IDS_MANAGEMENT_TITLE_BY,
                                    l10n_util::GetStringUTF16(device_type),
                                    base::UTF8ToUTF16(display_domain));
}
#endif  // defined(OS_CHROMEOS)

content::WebUIDataSource* CreateManagementUIHtmlSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIManagementHost);

#if defined(OS_CHROMEOS)
  source->AddString("title", GetChromeOSManagementPageTitle());
#endif  // defined(OS_CHROMEOS)

  static constexpr LocalizedString kLocalizedStrings[] = {
#if defined(OS_CHROMEOS)
    {"deviceConfiguration", IDS_MANAGEMENT_DEVICE_CONFIGURATION},
    {"deviceReporting", IDS_MANAGEMENT_DEVICE_REPORTING},
    {kManagementLogUploadEnabled, IDS_MANAGEMENT_LOG_UPLOAD_ENABLED},
    {kManagementReportActivityTimes,
     IDS_MANAGEMENT_REPORT_DEVICE_ACTIVITY_TIMES},
    {kManagementReportHardwareStatus,
     IDS_MANAGEMENT_REPORT_DEVICE_HARDWARE_STATUS},
    {kManagementReportNetworkInterfaces,
     IDS_MANAGEMENT_REPORT_DEVICE_NETWORK_INTERFACES},
    {kManagementReportUsers, IDS_MANAGEMENT_REPORT_DEVICE_USERS},
    {kManagementPrinting, IDS_MANAGEMENT_REPORT_PRINTING},
#endif  // defined(OS_CHROMEOS)
    {"browserReporting", IDS_MANAGEMENT_BROWSER_REPORTING},
    {"browserReportingExplanation",
     IDS_MANAGEMENT_BROWSER_REPORTING_EXPLANATION},
    {"extensionReporting", IDS_MANAGEMENT_EXTENSION_REPORTING},
    {"extensionsInstalled", IDS_MANAGEMENT_EXTENSIONS_INSTALLED},
    {"extensionName", IDS_MANAGEMENT_EXTENSIONS_NAME},
    {"extensionPermissions", IDS_MANAGEMENT_EXTENSIONS_PERMISSIONS},
    {"localTrustRoots", IDS_MANAGEMENT_LOCAL_TRUST_ROOTS},
    {"managementTrustRootsNotConfigured",
     IDS_MANAGEMENT_TRUST_ROOTS_NOT_CONFIGURED},
#if BUILDFLAG(ENABLE_EXTENSIONS)
    {kManagementExtensionReportMachineName,
     IDS_MANAGEMENT_EXTENSION_REPORT_MACHINE_NAME},
    {kManagementExtensionReportMachineNameAddress,
     IDS_MANAGEMENT_EXTENSION_REPORT_MACHINE_NAME_ADDRESS},
    {kManagementExtensionReportUsername,
     IDS_MANAGEMENT_EXTENSION_REPORT_USERNAME},
    {kManagementExtensionReportVersion,
     IDS_MANAGEMENT_EXTENSION_REPORT_VERSION},
    {kManagementExtensionReportExtensionsPlugin,
     IDS_MANAGEMENT_EXTENSION_REPORT_EXTENSIONS_PLUGINS},
    {kManagementExtensionReportPerfCrash,
     IDS_MANAGEMENT_EXTENSION_REPORT_PERF_CRASH},
    {kManagementExtensionReportUserBrowsingData,
     IDS_MANAGEMENT_EXTENSION_REPORT_USER_BROWSING_DATA},
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
  };

  AddLocalizedStringsBulk(source, kLocalizedStrings,
                          base::size(kLocalizedStrings));

  source->AddString(kManagementExtensionReportSafeBrowsingWarnings,
                    l10n_util::GetStringFUTF16(
                        IDS_MANAGEMENT_EXTENSION_REPORT_SAFE_BROWSING_WARNINGS,
                        base::UTF8ToUTF16(safe_browsing::kSafeBrowsingUrl)));

#if BUILDFLAG(ENABLE_EXTENSIONS)

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
  content::WebUIDataSource* source = CreateManagementUIHtmlSource();
  Profile* profile = Profile::FromWebUI(web_ui);
  auto management_ui_handler = std::make_unique<ManagementUIHandler>();
  management_ui_handler->InitializeManagementContextualStrings(profile, source);
  web_ui->AddMessageHandler(std::move(management_ui_handler));
  DarkModeHandler::Initialize(web_ui, source);
  content::WebUIDataSource::Add(profile, source);
}

ManagementUI::~ManagementUI() {}
