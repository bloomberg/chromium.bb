// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines specific implementation of BrowserDistribution class for
// Google Chrome.

#include "chrome/installer/util/google_chrome_distribution.h"

#include <windows.h>
#include <wtsapi32.h>
#include <msi.h>
#include <sddl.h>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/win/registry.h"
#include "base/win/windows_version.h"
#include "chrome/common/attrition_experiments.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/installer/util/channel_info.h"
#include "chrome/installer/util/product.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/l10n_string_util.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/installer/util/wmi.h"
#include "content/common/json_value_serializer.h"

#include "installer_util_strings.h"  // NOLINT

#pragma comment(lib, "wtsapi32.lib")

namespace {

const wchar_t kChromeGuid[] = L"{8A69D345-D564-463c-AFF1-A69D9E530F96}";
const wchar_t kBrowserAppId[] = L"Chrome";

// The following strings are the possible outcomes of the toast experiment
// as recorded in the |client| field.
const wchar_t kToastExpControlGroup[] =      L"01";
const wchar_t kToastExpCancelGroup[] =       L"02";
const wchar_t kToastExpUninstallGroup[] =    L"04";
const wchar_t kToastExpTriesOkGroup[] =      L"18";
const wchar_t kToastExpTriesErrorGroup[] =   L"28";
const wchar_t kToastActiveGroup[] =          L"40";
const wchar_t kToastUDDirFailure[] =         L"40";
const wchar_t kToastExpBaseGroup[] =         L"80";

// Substitute the locale parameter in uninstall URL with whatever
// Google Update tells us is the locale. In case we fail to find
// the locale, we use US English.
std::wstring LocalizeUrl(const wchar_t* url) {
  std::wstring language;
  if (!GoogleUpdateSettings::GetLanguage(&language))
    language = L"en-US";  // Default to US English.
  return ReplaceStringPlaceholders(url, language.c_str(), NULL);
}

std::wstring GetUninstallSurveyUrl() {
  const wchar_t kSurveyUrl[] = L"http://www.google.com/support/chrome/bin/"
                               L"request.py?hl=$1&contact_type=uninstall";
  return LocalizeUrl(kSurveyUrl);
}

std::wstring GetWelcomeBackUrl() {
  const wchar_t kWelcomeUrl[] = L"http://www.google.com/chrome/intl/$1/"
                                L"welcomeback-new.html";
  return LocalizeUrl(kWelcomeUrl);
}

// Converts FILETIME to hours. FILETIME times are absolute times in
// 100 nanosecond units. For example 5:30 pm of June 15, 2009 is 3580464.
int FileTimeToHours(const FILETIME& time) {
  const ULONGLONG k100sNanoSecsToHours = 10000000LL * 60 * 60;
  ULARGE_INTEGER uli = {time.dwLowDateTime, time.dwHighDateTime};
  return static_cast<int>(uli.QuadPart / k100sNanoSecsToHours);
}

// Returns the directory last write time in hours since January 1, 1601.
// Returns -1 if there was an error retrieving the directory time.
int GetDirectoryWriteTimeInHours(const wchar_t* path) {
  // To open a directory you need to pass FILE_FLAG_BACKUP_SEMANTICS.
  DWORD share = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
  HANDLE file = ::CreateFileW(path, 0, share, NULL, OPEN_EXISTING,
                              FILE_FLAG_BACKUP_SEMANTICS, NULL);
  if (INVALID_HANDLE_VALUE == file)
    return -1;
  FILETIME time;
  if (!::GetFileTime(file, NULL, NULL, &time))
    return -1;
  return FileTimeToHours(time);
}

// Returns the directory last-write time age in hours, relative to current
// time, so if it returns 14 it means that the directory was last written 14
// hours ago. Returns -1 if there was an error retrieving the directory.
int GetDirectoryWriteAgeInHours(const wchar_t* path) {
  int dir_time = GetDirectoryWriteTimeInHours(path);
  if (dir_time < 0)
    return dir_time;
  FILETIME time;
  GetSystemTimeAsFileTime(&time);
  int now_time = FileTimeToHours(time);
  if (dir_time >= now_time)
    return 0;
  return (now_time - dir_time);
}

// Launches setup.exe (located at |setup_path|) with |cmd_line|.
// If system_level_toast is true, appends --system-level-toast.
// If handle to experiment result key was given at startup, re-add it.
// Does not wait for the process to terminate.
bool LaunchSetup(CommandLine cmd_line, bool system_level_toast) {
  // Re-add the system level toast flag.
  if (system_level_toast) {
    cmd_line.AppendSwitch(installer::switches::kSystemLevelToast);

    // Re-add the toast result key. We need to do this because Setup running as
    // system passes the key to Setup running as user, but that child process
    // does not perform the actual toasting, it launches another Setup (as user)
    // to do so. That is the process that needs the key.
    const CommandLine& current_cmd_line = *CommandLine::ForCurrentProcess();
    std::string key(installer::switches::kToastResultsKey);
    std::string toast_key = current_cmd_line.GetSwitchValueASCII(key);
    if (!toast_key.empty()) {
      cmd_line.AppendSwitchASCII(key, toast_key);

      // Use handle inheritance to make sure the duplicated toast results key
      // gets inherited by the child process.
      base::LaunchOptions options;
      options.inherit_handles = true;
      return base::LaunchProcess(cmd_line, options, NULL);
    }
  }

  return base::LaunchProcess(cmd_line, base::LaunchOptions(), NULL);
}

// For System level installs, setup.exe lives in the system temp, which
// is normally c:\windows\temp. In many cases files inside this folder
// are not accessible for execution by regular user accounts.
// This function changes the permisions so that any authenticated user
// can launch |exe| later on. This function should only be called if the
// code is running at the system level.
bool FixDACLsForExecute(const FilePath& exe) {
  // The general strategy to is to add an ACE to the exe DACL the quick
  // and dirty way: a) read the DACL b) convert it to sddl string c) add the
  // new ACE to the string d) convert sddl string back to DACL and finally
  // e) write new dacl.
  char buff[1024];
  DWORD len = sizeof(buff);
  PSECURITY_DESCRIPTOR sd = reinterpret_cast<PSECURITY_DESCRIPTOR>(buff);
  if (!::GetFileSecurityW(exe.value().c_str(), DACL_SECURITY_INFORMATION,
                          sd, len, &len)) {
    return false;
  }
  wchar_t* sddl = 0;
  if (!::ConvertSecurityDescriptorToStringSecurityDescriptorW(sd,
      SDDL_REVISION_1, DACL_SECURITY_INFORMATION, &sddl, NULL))
    return false;
  std::wstring new_sddl(sddl);
  ::LocalFree(sddl);
  sd = NULL;
  // See MSDN for the  security descriptor definition language (SDDL) syntax,
  // in our case we add "A;" generic read 'GR' and generic execute 'GX' for
  // the nt\authenticated_users 'AU' group, that becomes:
  const wchar_t kAllowACE[] = L"(A;;GRGX;;;AU)";
  // We should check that there are no special ACES for the group we
  // are interested, which is nt\authenticated_users.
  if (std::wstring::npos != new_sddl.find(L";AU)"))
    return false;
  // Specific ACEs (not inherited) need to go to the front. It is ok if we
  // are the very first one.
  size_t pos_insert = new_sddl.find(L"(");
  if (std::wstring::npos == pos_insert)
    return false;
  // All good, time to change the dacl.
  new_sddl.insert(pos_insert, kAllowACE);
  if (!::ConvertStringSecurityDescriptorToSecurityDescriptorW(new_sddl.c_str(),
      SDDL_REVISION_1, &sd, NULL))
    return false;
  bool rv = ::SetFileSecurityW(exe.value().c_str(), DACL_SECURITY_INFORMATION,
                               sd) == TRUE;
  ::LocalFree(sd);
  return rv;
}

// This function launches setup as the currently logged-in interactive
// user that is the user whose logon session is attached to winsta0\default.
// It assumes that currently we are running as SYSTEM in a non-interactive
// windowstation.
// The function fails if there is no interactive session active, basically
// the computer is on but nobody has logged in locally.
// Remote Desktop sessions do not count as interactive sessions; running this
// method as a user logged in via remote desktop will do nothing.
bool LaunchSetupAsConsoleUser(const FilePath& setup_path,
                              const std::string& flag) {
  CommandLine cmd_line(setup_path);
  cmd_line.AppendSwitch(flag);

  // Get the Google Update results key, and pass it on the command line to
  // the child process.
  int key = GoogleUpdateSettings::DuplicateGoogleUpdateSystemClientKey();
  cmd_line.AppendSwitchASCII(installer::switches::kToastResultsKey,
                             base::IntToString(key));

  if (base::win::GetVersion() > base::win::VERSION_XP) {
    // Make sure that in Vista and Above we have the proper DACLs so
    // the interactive user can launch it.
    if (!FixDACLsForExecute(setup_path))
      NOTREACHED();
  }

  DWORD console_id = ::WTSGetActiveConsoleSessionId();
  if (console_id == 0xFFFFFFFF)
    return false;
  HANDLE user_token;
  if (!::WTSQueryUserToken(console_id, &user_token))
    return false;
  // Note: Handle inheritance must be true in order for the child process to be
  // able to use the duplicated handle above (Google Update results).
  base::LaunchOptions options;
  options.as_user = user_token;
  options.inherit_handles = true;
  bool launched = base::LaunchProcess(cmd_line, options, NULL);
  ::CloseHandle(user_token);
  return launched;
}

// The plugin infobar experiment is just setting the client registry value
// to one of four possible values from 10% of the elegible population, which
// is defined as active users that have opted-in for sending stats.
// Chrome reads this value and modifies the plugin blocking and infobar
// behavior accordingly.
bool DoInfobarPluginsExperiment(int dir_age_hours) {
  std::wstring client;
  if (!GoogleUpdateSettings::GetClient(&client))
    return false;
  // Make sure the user is not already in this experiment.
  if ((client.size() > 3) && (client[0] == L'P') && (client[1] == L'I'))
    return false;
  if (!GoogleUpdateSettings::GetCollectStatsConsent())
    return false;
  if (dir_age_hours > (24 * 14))
    return false;
  if (base::RandInt(0, 9)) {
    GoogleUpdateSettings::SetClient(
        attrition_experiments::kNotInPluginExperiment);
    return false;
  }

  const wchar_t* buckets[] = {
    attrition_experiments::kPluginNoBlockNoOOD,
    attrition_experiments::kPluginNoBlockDoOOD,
    attrition_experiments::kPluginDoBlockNoOOD,
    attrition_experiments::kPluginDoBlockDoOOD
  };

  size_t group = base::RandInt(0, arraysize(buckets)-1);
  GoogleUpdateSettings::SetClient(buckets[group]);
  VLOG(1) << "Plugin infobar experiment group: " << group;
  return true;
}
}  // namespace

GoogleChromeDistribution::GoogleChromeDistribution()
    : BrowserDistribution(CHROME_BROWSER),
      product_guid_(kChromeGuid) {
}

// The functions below are not used by the 64-bit Windows binary -
// see the comment in google_chrome_distribution_dummy.cc
#ifndef _WIN64
bool GoogleChromeDistribution::BuildUninstallMetricsString(
    DictionaryValue* uninstall_metrics_dict, std::wstring* metrics) {
  DCHECK(NULL != metrics);
  bool has_values = false;

  for (DictionaryValue::key_iterator iter(uninstall_metrics_dict->begin_keys());
       iter != uninstall_metrics_dict->end_keys(); ++iter) {
    has_values = true;
    metrics->append(L"&");
    metrics->append(UTF8ToWide(*iter));
    metrics->append(L"=");

    std::string value;
    uninstall_metrics_dict->GetStringWithoutPathExpansion(*iter, &value);
    metrics->append(UTF8ToWide(value));
  }

  return has_values;
}

bool GoogleChromeDistribution::ExtractUninstallMetricsFromFile(
    const FilePath& file_path,
    std::wstring* uninstall_metrics_string) {
  JSONFileValueSerializer json_serializer(file_path);

  std::string json_error_string;
  scoped_ptr<Value> root(json_serializer.Deserialize(NULL, NULL));
  if (!root.get())
    return false;

  // Preferences should always have a dictionary root.
  if (!root->IsType(Value::TYPE_DICTIONARY))
    return false;

  return ExtractUninstallMetrics(*static_cast<DictionaryValue*>(root.get()),
                                 uninstall_metrics_string);
}

bool GoogleChromeDistribution::ExtractUninstallMetrics(
    const DictionaryValue& root, std::wstring* uninstall_metrics_string) {
  // Make sure that the user wants us reporting metrics. If not, don't
  // add our uninstall metrics.
  bool metrics_reporting_enabled = false;
  if (!root.GetBoolean(prefs::kMetricsReportingEnabled,
                       &metrics_reporting_enabled) ||
      !metrics_reporting_enabled) {
    return false;
  }

  DictionaryValue* uninstall_metrics_dict;
  if (!root.HasKey(installer::kUninstallMetricsName) ||
      !root.GetDictionary(installer::kUninstallMetricsName,
                          &uninstall_metrics_dict)) {
    return false;
  }

  if (!BuildUninstallMetricsString(uninstall_metrics_dict,
                                   uninstall_metrics_string)) {
    return false;
  }

  return true;
}
#endif

void GoogleChromeDistribution::DoPostUninstallOperations(
    const Version& version,
    const FilePath& local_data_path,
    const std::wstring& distribution_data) {
  // Send the Chrome version and OS version as params to the form.
  // It would be nice to send the locale, too, but I don't see an
  // easy way to get that in the existing code. It's something we
  // can add later, if needed.
  // We depend on installed_version.GetString() not having spaces or other
  // characters that need escaping: 0.2.13.4. Should that change, we will
  // need to escape the string before using it in a URL.
  const std::wstring kVersionParam = L"crversion";
  const std::wstring kOSParam = L"os";
  base::win::OSInfo::VersionNumber version_number =
      base::win::OSInfo::GetInstance()->version_number();
  std::wstring os_version = base::StringPrintf(L"%d.%d.%d",
      version_number.major, version_number.minor, version_number.build);

  FilePath iexplore;
  if (!PathService::Get(base::DIR_PROGRAM_FILES, &iexplore))
    return;

  iexplore = iexplore.AppendASCII("Internet Explorer");
  iexplore = iexplore.AppendASCII("iexplore.exe");

  std::wstring command = iexplore.value() + L" " + GetUninstallSurveyUrl() +
      L"&" + kVersionParam + L"=" + UTF8ToWide(version.GetString()) + L"&" +
      kOSParam + L"=" + os_version;

  std::wstring uninstall_metrics;
  if (ExtractUninstallMetricsFromFile(local_data_path, &uninstall_metrics)) {
    // The user has opted into anonymous usage data collection, so append
    // metrics and distribution data.
    command += uninstall_metrics;
    if (!distribution_data.empty()) {
      command += L"&";
      command += distribution_data;
    }
  }

  int pid = 0;
  // The reason we use WMI to launch the process is because the uninstall
  // process runs inside a Job object controlled by the shell. As long as there
  // are processes running, the shell will not close the uninstall applet. WMI
  // allows us to escape from the Job object so the applet will close.
  installer::WMIProcess::Launch(command, &pid);
}

std::wstring GoogleChromeDistribution::GetAppGuid() {
  return product_guid();
}

std::wstring GoogleChromeDistribution::GetApplicationName() {
  const std::wstring& product_name =
      installer::GetLocalizedString(IDS_PRODUCT_NAME_BASE);
  return product_name;
}

std::wstring GoogleChromeDistribution::GetAlternateApplicationName() {
  const std::wstring& alt_product_name =
      installer::GetLocalizedString(IDS_OEM_MAIN_SHORTCUT_NAME_BASE);
  return alt_product_name;
}

std::wstring GoogleChromeDistribution::GetBrowserAppId() {
  return kBrowserAppId;
}

std::wstring GoogleChromeDistribution::GetInstallSubDir() {
  std::wstring sub_dir(installer::kGoogleChromeInstallSubDir1);
  sub_dir.append(L"\\");
  sub_dir.append(installer::kGoogleChromeInstallSubDir2);
  return sub_dir;
}

std::wstring GoogleChromeDistribution::GetPublisherName() {
  const std::wstring& publisher_name =
      installer::GetLocalizedString(IDS_ABOUT_VERSION_COMPANY_NAME_BASE);
  return publisher_name;
}

std::wstring GoogleChromeDistribution::GetAppDescription() {
  const std::wstring& app_description =
      installer::GetLocalizedString(IDS_SHORTCUT_TOOLTIP_BASE);
  return app_description;
}

std::string GoogleChromeDistribution::GetSafeBrowsingName() {
  return "googlechrome";
}

std::wstring GoogleChromeDistribution::GetStateKey() {
  std::wstring key(google_update::kRegPathClientState);
  key.append(L"\\");
  key.append(product_guid());
  return key;
}

std::wstring GoogleChromeDistribution::GetStateMediumKey() {
  std::wstring key(google_update::kRegPathClientStateMedium);
  key.append(L"\\");
  key.append(product_guid());
  return key;
}

std::wstring GoogleChromeDistribution::GetStatsServerURL() {
  return L"https://clients4.google.com/firefox/metrics/collect";
}

std::string GoogleChromeDistribution::GetNetworkStatsServer() const {
  // TODO(rtenneti): Return the network stats server name.
  return "";
}

std::wstring GoogleChromeDistribution::GetDistributionData(HKEY root_key) {
  std::wstring sub_key(google_update::kRegPathClientState);
  sub_key.append(L"\\");
  sub_key.append(product_guid());

  base::win::RegKey client_state_key(root_key, sub_key.c_str(), KEY_READ);
  std::wstring result;
  std::wstring brand_value;
  if (client_state_key.ReadValue(google_update::kRegRLZBrandField,
                                 &brand_value) == ERROR_SUCCESS) {
    result = google_update::kRegRLZBrandField;
    result.append(L"=");
    result.append(brand_value);
    result.append(L"&");
  }

  std::wstring client_value;
  if (client_state_key.ReadValue(google_update::kRegClientField,
                                 &client_value) == ERROR_SUCCESS) {
    result.append(google_update::kRegClientField);
    result.append(L"=");
    result.append(client_value);
    result.append(L"&");
  }

  std::wstring ap_value;
  // If we fail to read the ap key, send up "&ap=" anyway to indicate
  // that this was probably a stable channel release.
  client_state_key.ReadValue(google_update::kRegApField, &ap_value);
  result.append(google_update::kRegApField);
  result.append(L"=");
  result.append(ap_value);

  return result;
}

std::wstring GoogleChromeDistribution::GetUninstallLinkName() {
  const std::wstring& link_name =
      installer::GetLocalizedString(IDS_UNINSTALL_CHROME_BASE);
  return link_name;
}

std::wstring GoogleChromeDistribution::GetUninstallRegPath() {
  return L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\"
         L"Google Chrome";
}

std::wstring GoogleChromeDistribution::GetVersionKey() {
  std::wstring key(google_update::kRegPathClients);
  key.append(L"\\");
  key.append(product_guid());
  return key;
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
      product_guid());
}

// The functions below are not used by the 64-bit Windows binary -
// see the comment in google_chrome_distribution_dummy.cc
#ifndef _WIN64
// A helper function that writes to HKLM if the handle was passed through the
// command line, but HKCU otherwise. |experiment_group| is the value to write
// and |last_write| is used when writing to HKLM to determine whether to close
// the handle when done.
void SetClient(const std::wstring& experiment_group, bool last_write) {
  static int reg_key_handle = -1;
  if (reg_key_handle == -1) {
    // If a specific Toast Results key handle (presumably to our HKLM key) was
    // passed in to the command line (such as for system level installs), we use
    // it. Otherwise, we write to the key under HKCU.
    const CommandLine& cmd_line = *CommandLine::ForCurrentProcess();
    if (cmd_line.HasSwitch(installer::switches::kToastResultsKey)) {
      // Get the handle to the key under HKLM.
      base::StringToInt(cmd_line.GetSwitchValueASCII(
          installer::switches::kToastResultsKey).c_str(),
          &reg_key_handle);
    } else {
      reg_key_handle = 0;
    }
  }

  if (reg_key_handle) {
    // Use it to write the experiment results.
    GoogleUpdateSettings::WriteGoogleUpdateSystemClientKey(
        reg_key_handle, google_update::kRegClientField, experiment_group);
    if (last_write)
      CloseHandle((HANDLE) reg_key_handle);
  } else {
    // Write to HKCU.
    GoogleUpdateSettings::SetClient(experiment_group);
  }
}

bool GoogleChromeDistribution::GetExperimentDetails(
    UserExperiment* experiment, int flavor) {
  // Maximum number of experiment flavors we support.
  const int kMax = 4;
  // This struct determines which experiment flavors we show for each locale and
  // brand.
  //
  // The big experiment in Dec 2009 used TGxx and THxx.
  // The big experiment in Feb 2010 used TKxx and TLxx.
  // The big experiment in Apr 2010 used TMxx and TNxx.
  // The big experiment in Oct 2010 used TVxx TWxx TXxx TYxx.
  // The big experiment in Feb 2011 used SJxx SKxx SLxx SMxx.
  // Note: the plugin infobar experiment uses PIxx codes.
  using namespace attrition_experiments;
  static const struct UserExperimentDetails {
    const wchar_t* locale;  // Locale to show this experiment for (* for all).
    const wchar_t* brands;  // Brand codes show this experiment for (* for all).
    int control_group;      // Size of the control group, in percentages.
    const wchar_t prefix1;  // The first letter for the experiment code.
    const wchar_t prefix2;  // The second letter for the experiment code. This
                            // will be incremented by one for each additional
                            // experiment flavor beyond the first.
    int flavors;            // Numbers of flavors for this experiment. Should
                            // always be positive and never exceed the number
                            // of headings (below).
    int headings[kMax];     // A list of IDs per experiment. 0 == no heading.
  } kExperimentFlavors[] = {
    // First in this order are the brand specific ones.
    {L"en-US", kSkype, 1, L'Z', L'A', 1, { kSkype1, 0, 0, 0 } },
    // And then we have catch-alls, like en-US (all brands).
    {L"en-US", kAll,   1, L'T', L'V', 4, { kEnUs1, kEnUs2, kEnUs3, kEnUs4} },
    // Japan has two experiments, same IDs as en-US but translated differently.
    {L"jp",    kAll,   1, L'T', L'V', 2, { kEnUs1, kEnUs2, 0, 0} },
  };

  std::wstring locale;
  std::wstring brand;

  if (!GoogleUpdateSettings::GetLanguage(&locale))
    locale = L"en-US";
  if (!GoogleUpdateSettings::GetBrand(&brand))
    return false;

  for (int i = 0; i < arraysize(kExperimentFlavors); ++i) {
    // A maximum of four flavors is supported at the moment.
    DCHECK_LE(kExperimentFlavors[i].flavors, kMax);
    DCHECK_GT(kExperimentFlavors[i].flavors, 0);
    // Make sure each experiment has valid headings.
    for (int f = 0; f < kMax; ++f) {
      if (f < kExperimentFlavors[i].flavors) {
        DCHECK_GT(kExperimentFlavors[i].headings[f], 0);
      } else {
        DCHECK_EQ(kExperimentFlavors[i].headings[f], 0);
      }
    }
    // Make sure we don't overflow on the second letter of the experiment code.
    DCHECK(kExperimentFlavors[i].prefix2 +
           kExperimentFlavors[i].flavors - 1 <= 'Z');

    if (kExperimentFlavors[i].locale != locale &&
        kExperimentFlavors[i].locale != L"*")
      continue;

    std::vector<std::wstring> brand_codes;
    base::SplitString(kExperimentFlavors[i].brands, L',', &brand_codes);
    if (brand_codes.empty())
      return false;
    for (std::vector<std::wstring>::iterator it = brand_codes.begin();
         it != brand_codes.end(); ++it) {
      if (*it != brand && *it != L"*")
        continue;

      // We have found our match.
      if (flavor < 0)
        flavor = base::RandInt(0, kExperimentFlavors[i].flavors - 1);
      experiment->flavor = flavor;
      experiment->heading = kExperimentFlavors[i].headings[flavor];
      experiment->control_group = kExperimentFlavors[i].control_group;
      experiment->prefix.resize(2);
      experiment->prefix[0] = kExperimentFlavors[i].prefix1;
      experiment->prefix[1] = kExperimentFlavors[i].prefix2 + flavor;
      return true;
    }
  }

  return false;
}

// Currently we have two experiments: 1) The inactive user toast. Which only
// applies to users doing upgrades, and 2) The plugin infobar experiment
// which only applies for active users.
//
// There are three scenarios when this function is called:
// 1- Is a per-user-install and it updated: perform the experiment
// 2- Is a system-install and it updated : relaunch as the interactive user
// 3- It has been re-launched from the #2 case. In this case we enter
//    this function with |system_install| true and a REENTRY_SYS_UPDATE status.
void GoogleChromeDistribution::LaunchUserExperiment(
    const FilePath& setup_path, installer::InstallStatus status,
    const Version& version, const installer::Product& installation,
    bool system_level) {
  if (system_level) {
    if (installer::NEW_VERSION_UPDATED == status) {
      // We need to relaunch as the interactive user.
      LaunchSetupAsConsoleUser(setup_path,
                               installer::switches::kSystemLevelToast);
      return;
    }
  } else {
    if ((installer::NEW_VERSION_UPDATED != status) &&
        (installer::REENTRY_SYS_UPDATE != status)) {
      // We are not updating or in re-launch. Exit.
      return;
    }
  }

  // The |flavor| value ends up being processed by TryChromeDialogView to show
  // different experiments.
  UserExperiment experiment;
  if (!GetExperimentDetails(&experiment, -1)) {
    VLOG(1) << "Failed to get experiment details.";
    return;
  }
  int flavor = experiment.flavor;
  std::wstring base_group = experiment.prefix;

  std::wstring brand;
  if (GoogleUpdateSettings::GetBrand(&brand) && (brand == L"CHXX")) {
    // Testing only: the user automatically qualifies for the experiment.
    VLOG(1) << "Experiment qualification bypass";
  } else {
    // Check browser usage inactivity by the age of the last-write time of the
    // chrome user data directory.
    FilePath user_data_dir(installation.GetUserDataPath());

    const bool toast_experiment_enabled = false;
    const int kThirtyDays = 30 * 24;

    int dir_age_hours = GetDirectoryWriteAgeInHours(
        user_data_dir.value().c_str());
    if (!toast_experiment_enabled) {
      // Ok, no toast, but what about the plugin infobar experiment?
      if (!DoInfobarPluginsExperiment(dir_age_hours)) {
        VLOG(1) << "No infobar experiment";
      }
      return;
    } else if (dir_age_hours < 0) {
      // This means that we failed to find the user data dir. The most likely
      // cause is that this user has not ever used chrome at all which can
      // happen in a system-level install.
      SetClient(base_group + kToastUDDirFailure, true);
      return;
    } else if (dir_age_hours < kThirtyDays) {
      VLOG(1) << "Chrome used in last " << dir_age_hours << " hours";
      SetClient(base_group + kToastActiveGroup, true);
      return;
    }
    // Check to see if this user belongs to the control group.
    double control_group = 1.0 * (100 - experiment.control_group) / 100;
    if (base::RandDouble() > control_group) {
      SetClient(base_group + kToastExpControlGroup, true);
      VLOG(1) << "User is control group";
      return;
    }
  }

  VLOG(1) << "User drafted for toast experiment " << flavor;
  SetClient(base_group + kToastExpBaseGroup, false);
  // User level: The experiment needs to be performed in a different process
  // because google_update expects the upgrade process to be quick and nimble.
  // System level: We have already been relaunched, so we don't need to be
  // quick, but we relaunch to follow the exact same codepath.
  CommandLine cmd_line(setup_path);
  cmd_line.AppendSwitchASCII(installer::switches::kInactiveUserToast,
                             base::IntToString(flavor));
  cmd_line.AppendSwitchASCII(installer::switches::kExperimentGroup,
                             WideToASCII(base_group));
  LaunchSetup(cmd_line, system_level);
}

// User qualifies for the experiment. To test, use --try-chrome-again=|flavor|
// as a parameter to chrome.exe.
void GoogleChromeDistribution::InactiveUserToastExperiment(int flavor,
    const std::wstring& experiment_group,
    const installer::Product& installation,
    const FilePath& application_path) {
  bool has_welcome_url = (flavor == 0);
  // Possibly add a url to launch depending on the experiment flavor.
  CommandLine options(CommandLine::NO_PROGRAM);
  options.AppendSwitchNative(switches::kTryChromeAgain,
      base::IntToString16(flavor));
  if (has_welcome_url) {
    // Prepend the url with a space.
    std::wstring url(GetWelcomeBackUrl());
    options.AppendArg("--");
    options.AppendArgNative(url);
    // The command line should now have the url added as:
    // "chrome.exe -- <url>"
    DCHECK_NE(std::wstring::npos,
        options.GetCommandLineString().find(L" -- " + url));
  }
  // Launch chrome now. It will show the toast UI.
  int32 exit_code = 0;
  if (!installation.LaunchChromeAndWait(application_path, options, &exit_code))
    return;

  // The chrome process has exited, figure out what happened.
  const wchar_t* outcome = NULL;
  switch (exit_code) {
    case content::RESULT_CODE_NORMAL_EXIT:
      outcome = kToastExpTriesOkGroup;
      break;
    case chrome::RESULT_CODE_NORMAL_EXIT_CANCEL:
      outcome = kToastExpCancelGroup;
      break;
    case chrome::RESULT_CODE_NORMAL_EXIT_EXP2:
      outcome = kToastExpUninstallGroup;
      break;
    default:
      outcome = kToastExpTriesErrorGroup;
  };

  // Write to the |client| key for the last time.
  SetClient(experiment_group + outcome, true);

  if (outcome != kToastExpUninstallGroup)
    return;

  // The user wants to uninstall. This is a best effort operation. Note that
  // we waited for chrome to exit so the uninstall would not detect chrome
  // running.
  bool system_level_toast = CommandLine::ForCurrentProcess()->HasSwitch(
      installer::switches::kSystemLevelToast);

  CommandLine cmd(InstallUtil::GetChromeUninstallCmd(system_level_toast,
                                                     GetType()));
  base::LaunchProcess(cmd, base::LaunchOptions(), NULL);
}
#endif
