// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines specific implementation of BrowserDistribution class for
// Google Chrome.

#include "chrome/installer/util/google_chrome_distribution.h"

#include <windows.h>
#include <msi.h>
#include <shellapi.h>

#include <utility>

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "base/win/windows_version.h"
#include "chrome/common/chrome_icon_resources_win.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/installer/util/app_registration_data.h"
#include "chrome/installer/util/channel_info.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/installer_util_strings.h"
#include "chrome/installer/util/l10n_string_util.h"
#include "chrome/installer/util/uninstall_metrics.h"
#include "chrome/installer/util/updating_app_registration_data.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/installer/util/wmi.h"
#include "third_party/crashpad/crashpad/client/crash_report_database.h"
#include "third_party/crashpad/crashpad/client/settings.h"

namespace {

const wchar_t kChromeGuid[] = L"{8A69D345-D564-463c-AFF1-A69D9E530F96}";
const wchar_t kBrowserAppId[] = L"Chrome";
const wchar_t kBrowserProgIdPrefix[] = L"ChromeHTML";
const wchar_t kBrowserProgIdDesc[] = L"Chrome HTML Document";
const wchar_t kCommandExecuteImplUuid[] =
    L"{5C65F4B0-3651-4514-B207-D10CB699B14B}";

// Substitute the locale parameter in uninstall URL with whatever
// Google Update tells us is the locale. In case we fail to find
// the locale, we use US English.
base::string16 LocalizeUrl(const wchar_t* url) {
  base::string16 language;
  if (!GoogleUpdateSettings::GetLanguage(&language))
    language = L"en-US";  // Default to US English.
  return base::ReplaceStringPlaceholders(url, language.c_str(), NULL);
}

base::string16 GetUninstallSurveyUrl() {
  const wchar_t kSurveyUrl[] = L"https://support.google.com/chrome/"
                               L"contact/chromeuninstall3?hl=$1";
  return LocalizeUrl(kSurveyUrl);
}

bool NavigateToUrlWithEdge(const base::string16& url) {
  base::string16 protocol_url = L"microsoft-edge:" + url;
  SHELLEXECUTEINFO info = { sizeof(info) };
  info.fMask = SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
  info.lpVerb = L"open";
  info.lpFile = protocol_url.c_str();
  info.nShow = SW_SHOWNORMAL;
  if (::ShellExecuteEx(&info))
    return true;
  PLOG(ERROR) << "Failed to launch Edge for uninstall survey";
  return false;
}

void NavigateToUrlWithIExplore(const base::string16& url) {
  base::FilePath iexplore;
  if (!PathService::Get(base::DIR_PROGRAM_FILES, &iexplore))
    return;

  iexplore = iexplore.AppendASCII("Internet Explorer");
  iexplore = iexplore.AppendASCII("iexplore.exe");

  base::string16 command = L"\"" + iexplore.value() + L"\" " + url;

  int pid = 0;
  // The reason we use WMI to launch the process is because the uninstall
  // process runs inside a Job object controlled by the shell. As long as there
  // are processes running, the shell will not close the uninstall applet. WMI
  // allows us to escape from the Job object so the applet will close.
  installer::WMIProcess::Launch(command, &pid);
}

}  // namespace

GoogleChromeDistribution::GoogleChromeDistribution()
    : BrowserDistribution(CHROME_BROWSER,
                          std::unique_ptr<AppRegistrationData>(
                              new UpdatingAppRegistrationData(kChromeGuid))) {}

GoogleChromeDistribution::GoogleChromeDistribution(
    std::unique_ptr<AppRegistrationData> app_reg_data)
    : BrowserDistribution(CHROME_BROWSER, std::move(app_reg_data)) {}

void GoogleChromeDistribution::DoPostUninstallOperations(
    const Version& version,
    const base::FilePath& local_data_path,
    const base::string16& distribution_data) {
  // Send the Chrome version and OS version as params to the form.
  // It would be nice to send the locale, too, but I don't see an
  // easy way to get that in the existing code. It's something we
  // can add later, if needed.
  // We depend on installed_version.GetString() not having spaces or other
  // characters that need escaping: 0.2.13.4. Should that change, we will
  // need to escape the string before using it in a URL.
  const base::string16 kVersionParam = L"crversion";
  const base::string16 kOSParam = L"os";

  const base::win::OSInfo* os_info = base::win::OSInfo::GetInstance();
  base::win::OSInfo::VersionNumber version_number = os_info->version_number();
  base::string16 os_version =
      base::StringPrintf(L"%d.%d.%d", version_number.major,
                         version_number.minor, version_number.build);

  base::string16 url = GetUninstallSurveyUrl() + L"&" + kVersionParam + L"=" +
                       base::ASCIIToUTF16(version.GetString()) + L"&" +
                       kOSParam + L"=" + os_version;

  base::string16 uninstall_metrics;
  if (installer::ExtractUninstallMetricsFromFile(local_data_path,
                                                 &uninstall_metrics)) {
    // The user has opted into anonymous usage data collection, so append
    // metrics and distribution data.
    url += uninstall_metrics;
    if (!distribution_data.empty()) {
      url += L"&";
      url += distribution_data;
    }
  }

  if (os_info->version() >= base::win::VERSION_WIN10 &&
      NavigateToUrlWithEdge(url)) {
    return;
  }
  NavigateToUrlWithIExplore(url);
}

base::string16 GoogleChromeDistribution::GetActiveSetupGuid() {
  return GetAppGuid();
}

base::string16 GoogleChromeDistribution::GetBaseAppName() {
  // I'd really like to return L ## PRODUCT_FULLNAME_STRING; but that's no good
  // since it'd be "Chromium" in a non-Chrome build, which isn't at all what I
  // want.  Sigh.
  return L"Google Chrome";
}

base::string16 GoogleChromeDistribution::GetShortcutName(
    ShortcutType shortcut_type) {
  int string_id = IDS_PRODUCT_NAME_BASE;
  switch (shortcut_type) {
    case SHORTCUT_APP_LAUNCHER:
      string_id = IDS_APP_LIST_SHORTCUT_NAME_BASE;
      break;
    default:
      DCHECK_EQ(SHORTCUT_CHROME, shortcut_type);
      break;
  }
  return installer::GetLocalizedString(string_id);
}

int GoogleChromeDistribution::GetIconIndex(ShortcutType shortcut_type) {
  if (shortcut_type == SHORTCUT_APP_LAUNCHER)
    return icon_resources::kAppLauncherIndex;
  DCHECK_EQ(SHORTCUT_CHROME, shortcut_type);
  return icon_resources::kApplicationIndex;
}

base::string16 GoogleChromeDistribution::GetBaseAppId() {
  return kBrowserAppId;
}

base::string16 GoogleChromeDistribution::GetBrowserProgIdPrefix() {
  return kBrowserProgIdPrefix;
}

base::string16 GoogleChromeDistribution::GetBrowserProgIdDesc() {
  return kBrowserProgIdDesc;
}

base::string16 GoogleChromeDistribution::GetInstallSubDir() {
  base::string16 sub_dir(installer::kGoogleChromeInstallSubDir1);
  sub_dir.append(L"\\");
  sub_dir.append(installer::kGoogleChromeInstallSubDir2);
  return sub_dir;
}

base::string16 GoogleChromeDistribution::GetPublisherName() {
  const base::string16& publisher_name =
      installer::GetLocalizedString(IDS_ABOUT_VERSION_COMPANY_NAME_BASE);
  return publisher_name;
}

base::string16 GoogleChromeDistribution::GetAppDescription() {
  const base::string16& app_description =
      installer::GetLocalizedString(IDS_SHORTCUT_TOOLTIP_BASE);
  return app_description;
}

std::string GoogleChromeDistribution::GetSafeBrowsingName() {
  return "googlechrome";
}

base::string16 GoogleChromeDistribution::GetDistributionData(HKEY root_key) {
  base::string16 sub_key(google_update::kRegPathClientState);
  sub_key.append(L"\\");
  sub_key.append(GetAppGuid());

  base::win::RegKey client_state_key(
      root_key, sub_key.c_str(), KEY_READ | KEY_WOW64_32KEY);
  base::string16 result;
  base::string16 brand_value;
  if (client_state_key.ReadValue(google_update::kRegRLZBrandField,
                                 &brand_value) == ERROR_SUCCESS) {
    result = google_update::kRegRLZBrandField;
    result.append(L"=");
    result.append(brand_value);
    result.append(L"&");
  }

  base::string16 client_value;
  if (client_state_key.ReadValue(google_update::kRegClientField,
                                 &client_value) == ERROR_SUCCESS) {
    result.append(google_update::kRegClientField);
    result.append(L"=");
    result.append(client_value);
    result.append(L"&");
  }

  base::string16 ap_value;
  // If we fail to read the ap key, send up "&ap=" anyway to indicate
  // that this was probably a stable channel release.
  client_state_key.ReadValue(google_update::kRegApField, &ap_value);
  result.append(google_update::kRegApField);
  result.append(L"=");
  result.append(ap_value);

  // Crash client id.
  base::FilePath crash_dir;
  if (chrome::GetDefaultCrashDumpLocation(&crash_dir)) {
    crashpad::UUID client_id;
    std::unique_ptr<crashpad::CrashReportDatabase> database(
        crashpad::CrashReportDatabase::InitializeWithoutCreating(crash_dir));
    if (database && database->GetSettings()->GetClientID(&client_id))
      result.append(L"&crash_client_id=").append(client_id.ToString16());
  }

  return result;
}

base::string16 GoogleChromeDistribution::GetUninstallRegPath() {
  return L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\"
         L"Google Chrome";
}

base::string16 GoogleChromeDistribution::GetIconFilename() {
  return installer::kChromeExe;
}

base::string16 GoogleChromeDistribution::GetCommandExecuteImplClsid() {
  return kCommandExecuteImplUuid;
}

// This method checks if we need to change "ap" key in Google Update to try
// full installer as fall back method in case incremental installer fails.
// - If incremental installer fails we append a magic string ("-full"), if
// it is not present already, so that Google Update server next time will send
// full installer to update Chrome on the local machine
// - If we are currently running full installer, we remove this magic
// string (if it is present) regardless of whether installer failed or not.
// There is no fall-back for full installer :)
void GoogleChromeDistribution::UpdateInstallStatus(bool system_install,
    installer::ArchiveType archive_type,
    installer::InstallStatus install_status) {
  GoogleUpdateSettings::UpdateInstallStatus(system_install,
      archive_type, InstallUtil::GetInstallReturnCode(install_status),
      GetAppGuid());
}

bool GoogleChromeDistribution::ShouldSetExperimentLabels() {
  return true;
}

bool GoogleChromeDistribution::HasUserExperiments() {
  return true;
}
