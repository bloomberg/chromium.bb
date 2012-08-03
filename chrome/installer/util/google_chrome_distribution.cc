// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines specific implementation of BrowserDistribution class for
// Google Chrome.

#include "chrome/installer/util/google_chrome_distribution.h"

#include <vector>
#include <windows.h>
#include <wtsapi32.h>
#include <msi.h>
#include <sddl.h>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/process_util.h"
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
#include "chrome/common/net/test_server_locations.h"
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

#include "installer_util_strings.h"  // NOLINT

#pragma comment(lib, "wtsapi32.lib")

namespace {

const wchar_t kChromeGuid[] = L"{8A69D345-D564-463c-AFF1-A69D9E530F96}";
const wchar_t kBrowserAppId[] = L"Chrome";
const wchar_t kCommandExecuteImplUuid[] =
    L"{5C65F4B0-3651-4514-B207-D10CB699B14B}";
const wchar_t kDelegateExecuteLibUuid[] =
    L"{4E805ED8-EBA0-4601-9681-12815A56EBFD}";
const wchar_t kDelegateExecuteLibVersion[] = L"1.0";
const wchar_t kICommandExecuteImplUuid[] =
    L"{0BA0D4E9-2259-4963-B9AE-A839F7CB7544}";

// The following strings are the possible outcomes of the toast experiment
// as recorded in the |client| field.
const wchar_t kToastExpControlGroup[] =        L"01";
const wchar_t kToastExpCancelGroup[] =         L"02";
const wchar_t kToastExpUninstallGroup[] =      L"04";
const wchar_t kToastExpTriesOkGroup[] =        L"18";
const wchar_t kToastExpTriesErrorGroup[] =     L"28";
const wchar_t kToastExpTriesOkDefaultGroup[] = L"48";
const wchar_t kToastActiveGroup[] =            L"40";
const wchar_t kToastUDDirFailure[] =           L"40";
const wchar_t kToastExpBaseGroup[] =           L"80";

// Substitute the locale parameter in uninstall URL with whatever
// Google Update tells us is the locale. In case we fail to find
// the locale, we use US English.
string16 LocalizeUrl(const wchar_t* url) {
  string16 language;
  if (!GoogleUpdateSettings::GetLanguage(&language))
    language = L"en-US";  // Default to US English.
  return ReplaceStringPlaceholders(url, language.c_str(), NULL);
}

string16 GetUninstallSurveyUrl() {
  const wchar_t kSurveyUrl[] = L"http://www.google.com/support/chrome/bin/"
                               L"request.py?hl=$1&contact_type=uninstall";
  return LocalizeUrl(kSurveyUrl);
}

string16 GetWelcomeBackUrl() {
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
  if (!::GetFileTime(file, NULL, NULL, &time)) {
    ::CloseHandle(file);
    return -1;
  }

  ::CloseHandle(file);
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
// |cmd_line| may be modified as a result of this call.
bool LaunchSetup(CommandLine* cmd_line,
                 const installer::Product& product,
                 bool system_level_toast) {
  const CommandLine& current_cmd_line = *CommandLine::ForCurrentProcess();

  // Propagate --verbose-logging to the invoked setup.exe.
  if (current_cmd_line.HasSwitch(installer::switches::kVerboseLogging))
    cmd_line->AppendSwitch(installer::switches::kVerboseLogging);

  // Pass along product-specific options.
  product.AppendProductFlags(cmd_line);

  // Re-add the system level toast flag.
  if (system_level_toast) {
    cmd_line->AppendSwitch(installer::switches::kSystemLevel);
    cmd_line->AppendSwitch(installer::switches::kSystemLevelToast);

    // Re-add the toast result key. We need to do this because Setup running as
    // system passes the key to Setup running as user, but that child process
    // does not perform the actual toasting, it launches another Setup (as user)
    // to do so. That is the process that needs the key.
    std::string key(installer::switches::kToastResultsKey);
    std::string toast_key = current_cmd_line.GetSwitchValueASCII(key);
    if (!toast_key.empty()) {
      cmd_line->AppendSwitchASCII(key, toast_key);

      // Use handle inheritance to make sure the duplicated toast results key
      // gets inherited by the child process.
      base::LaunchOptions options;
      options.inherit_handles = true;
      return base::LaunchProcess(*cmd_line, options, NULL);
    }
  }

  return base::LaunchProcess(*cmd_line, base::LaunchOptions(), NULL);
}

// For System level installs, setup.exe lives in the system temp, which
// is normally c:\windows\temp. In many cases files inside this folder
// are not accessible for execution by regular user accounts.
// This function changes the permissions so that any authenticated user
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
  string16 new_sddl(sddl);
  ::LocalFree(sddl);
  sd = NULL;
  // See MSDN for the  security descriptor definition language (SDDL) syntax,
  // in our case we add "A;" generic read 'GR' and generic execute 'GX' for
  // the nt\authenticated_users 'AU' group, that becomes:
  const wchar_t kAllowACE[] = L"(A;;GRGX;;;AU)";
  // We should check that there are no special ACES for the group we
  // are interested, which is nt\authenticated_users.
  if (string16::npos != new_sddl.find(L";AU)"))
    return false;
  // Specific ACEs (not inherited) need to go to the front. It is ok if we
  // are the very first one.
  size_t pos_insert = new_sddl.find(L"(");
  if (string16::npos == pos_insert)
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
                              const installer::Product& product,
                              const std::string& flag) {
  CommandLine cmd_line(setup_path);
  cmd_line.AppendSwitch(flag);

  // Pass along product-specific options.
  product.AppendProductFlags(&cmd_line);

  // Convey to the invoked setup.exe that it's operating on a system-level
  // installation.
  cmd_line.AppendSwitch(installer::switches::kSystemLevel);

  // Propagate --verbose-logging to the invoked setup.exe.
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          installer::switches::kVerboseLogging)) {
    cmd_line.AppendSwitch(installer::switches::kVerboseLogging);
  }

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
  if (console_id == 0xFFFFFFFF) {
    PLOG(ERROR) << __FUNCTION__ << " failed to get active session id";
    return false;
  }
  HANDLE user_token;
  if (!::WTSQueryUserToken(console_id, &user_token)) {
    PLOG(ERROR) << __FUNCTION__ << " failed to get user token for console_id "
                << console_id;
    return false;
  }
  // Note: Handle inheritance must be true in order for the child process to be
  // able to use the duplicated handle above (Google Update results).
  base::LaunchOptions options;
  options.as_user = user_token;
  options.inherit_handles = true;
  options.empty_desktop_name = true;
  VLOG(1) << __FUNCTION__ << " launching " << cmd_line.GetCommandLineString();
  bool launched = base::LaunchProcess(cmd_line, options, NULL);
  ::CloseHandle(user_token);
  VLOG(1) << __FUNCTION__ << "   result: " << launched;
  return launched;
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
    const DictionaryValue* uninstall_metrics_dict, string16* metrics) {
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
    string16* uninstall_metrics_string) {
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
    const DictionaryValue& root,
    string16* uninstall_metrics_string) {
  // Make sure that the user wants us reporting metrics. If not, don't
  // add our uninstall metrics.
  bool metrics_reporting_enabled = false;
  if (!root.GetBoolean(prefs::kMetricsReportingEnabled,
                       &metrics_reporting_enabled) ||
      !metrics_reporting_enabled) {
    return false;
  }

  const DictionaryValue* uninstall_metrics_dict;
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
    const string16& distribution_data) {
  // Send the Chrome version and OS version as params to the form.
  // It would be nice to send the locale, too, but I don't see an
  // easy way to get that in the existing code. It's something we
  // can add later, if needed.
  // We depend on installed_version.GetString() not having spaces or other
  // characters that need escaping: 0.2.13.4. Should that change, we will
  // need to escape the string before using it in a URL.
  const string16 kVersionParam = L"crversion";
  const string16 kOSParam = L"os";
  base::win::OSInfo::VersionNumber version_number =
      base::win::OSInfo::GetInstance()->version_number();
  string16 os_version = base::StringPrintf(L"%d.%d.%d",
      version_number.major, version_number.minor, version_number.build);

  FilePath iexplore;
  if (!PathService::Get(base::DIR_PROGRAM_FILES, &iexplore))
    return;

  iexplore = iexplore.AppendASCII("Internet Explorer");
  iexplore = iexplore.AppendASCII("iexplore.exe");

  string16 command = iexplore.value() + L" " + GetUninstallSurveyUrl() +
      L"&" + kVersionParam + L"=" + UTF8ToWide(version.GetString()) + L"&" +
      kOSParam + L"=" + os_version;

  string16 uninstall_metrics;
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

string16 GoogleChromeDistribution::GetAppGuid() {
  return product_guid();
}

string16 GoogleChromeDistribution::GetBaseAppName() {
  // I'd really like to return L ## PRODUCT_FULLNAME_STRING; but that's no good
  // since it'd be "Chromium" in a non-Chrome build, which isn't at all what I
  // want.  Sigh.
  return L"Google Chrome";
}

string16 GoogleChromeDistribution::GetAppShortCutName() {
  const string16& app_shortcut_name =
      installer::GetLocalizedString(IDS_PRODUCT_NAME_BASE);
  return app_shortcut_name;
}

string16 GoogleChromeDistribution::GetAlternateApplicationName() {
  const string16& alt_product_name =
      installer::GetLocalizedString(IDS_OEM_MAIN_SHORTCUT_NAME_BASE);
  return alt_product_name;
}

string16 GoogleChromeDistribution::GetBaseAppId() {
  return kBrowserAppId;
}

string16 GoogleChromeDistribution::GetInstallSubDir() {
  string16 sub_dir(installer::kGoogleChromeInstallSubDir1);
  sub_dir.append(L"\\");
  sub_dir.append(installer::kGoogleChromeInstallSubDir2);
  return sub_dir;
}

string16 GoogleChromeDistribution::GetPublisherName() {
  const string16& publisher_name =
      installer::GetLocalizedString(IDS_ABOUT_VERSION_COMPANY_NAME_BASE);
  return publisher_name;
}

string16 GoogleChromeDistribution::GetAppDescription() {
  const string16& app_description =
      installer::GetLocalizedString(IDS_SHORTCUT_TOOLTIP_BASE);
  return app_description;
}

std::string GoogleChromeDistribution::GetSafeBrowsingName() {
  return "googlechrome";
}

string16 GoogleChromeDistribution::GetStateKey() {
  string16 key(google_update::kRegPathClientState);
  key.append(L"\\");
  key.append(product_guid());
  return key;
}

string16 GoogleChromeDistribution::GetStateMediumKey() {
  string16 key(google_update::kRegPathClientStateMedium);
  key.append(L"\\");
  key.append(product_guid());
  return key;
}

string16 GoogleChromeDistribution::GetStatsServerURL() {
  return L"https://clients4.google.com/firefox/metrics/collect";
}

std::string GoogleChromeDistribution::GetNetworkStatsServer() const {
  return chrome_common_net::kEchoTestServerLocation;
}

std::string GoogleChromeDistribution::GetHttpPipeliningTestServer() const {
  return chrome_common_net::kPipelineTestServerBaseUrl;
}

string16 GoogleChromeDistribution::GetDistributionData(HKEY root_key) {
  string16 sub_key(google_update::kRegPathClientState);
  sub_key.append(L"\\");
  sub_key.append(product_guid());

  base::win::RegKey client_state_key(root_key, sub_key.c_str(), KEY_READ);
  string16 result;
  string16 brand_value;
  if (client_state_key.ReadValue(google_update::kRegRLZBrandField,
                                 &brand_value) == ERROR_SUCCESS) {
    result = google_update::kRegRLZBrandField;
    result.append(L"=");
    result.append(brand_value);
    result.append(L"&");
  }

  string16 client_value;
  if (client_state_key.ReadValue(google_update::kRegClientField,
                                 &client_value) == ERROR_SUCCESS) {
    result.append(google_update::kRegClientField);
    result.append(L"=");
    result.append(client_value);
    result.append(L"&");
  }

  string16 ap_value;
  // If we fail to read the ap key, send up "&ap=" anyway to indicate
  // that this was probably a stable channel release.
  client_state_key.ReadValue(google_update::kRegApField, &ap_value);
  result.append(google_update::kRegApField);
  result.append(L"=");
  result.append(ap_value);

  return result;
}

string16 GoogleChromeDistribution::GetUninstallLinkName() {
  const string16& link_name =
      installer::GetLocalizedString(IDS_UNINSTALL_CHROME_BASE);
  return link_name;
}

string16 GoogleChromeDistribution::GetUninstallRegPath() {
  return L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\"
         L"Google Chrome";
}

string16 GoogleChromeDistribution::GetVersionKey() {
  string16 key(google_update::kRegPathClients);
  key.append(L"\\");
  key.append(product_guid());
  return key;
}

bool GoogleChromeDistribution::GetDelegateExecuteHandlerData(
    string16* handler_class_uuid,
    string16* type_lib_uuid,
    string16* type_lib_version,
    string16* interface_uuid) {
  if (handler_class_uuid)
    *handler_class_uuid = kCommandExecuteImplUuid;
  if (type_lib_uuid)
    *type_lib_uuid = kDelegateExecuteLibUuid;
  if (type_lib_version)
    *type_lib_version = kDelegateExecuteLibVersion;
  if (interface_uuid)
    *interface_uuid = kICommandExecuteImplUuid;
  return true;
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
void SetClient(const string16& experiment_group, bool last_write) {
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
  struct FlavorDetails {
    int heading_id;
    int flags;
  };
  // Maximum number of experiment flavors we support.
  static const int kMax = 4;
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
    const wchar_t* prefix;  // The two letter experiment code. The second letter
                            // will be incremented with the flavor.
    FlavorDetails flavors[kMax];
  } kExperiments[] = {
    // The first match from top to bottom is used so this list should be ordered
    // most-specific rule first.
    { L"*", L"CHMA",  // All locales, CHMA brand.
      25,             // 25 percent control group.
      L"ZA",          // Experiment is ZAxx, ZBxx, ZCxx, ZDxx etc.
      // Three flavors.
      { { IDS_TRY_TOAST_HEADING3, kDontBugMeAsButton | kUninstall | kWhyLink },
        { IDS_TRY_TOAST_HEADING3, 0 },
        { IDS_TRY_TOAST_HEADING3, kMakeDefault },
        { 0, 0 },
      }
    },
    { L"*", L"GGRV",  // All locales, GGRV is enterprise.
      0,              // 0 percent control group.
      L"EA",          // Experiment is EAxx, EBxx, etc.
      // No flavors means no experiment.
      { { 0, 0 },
        { 0, 0 },
        { 0, 0 },
        { 0, 0 }
      }
    }
  };

  string16 locale;
  GoogleUpdateSettings::GetLanguage(&locale);
  if (locale.empty() || (locale == ASCIIToWide("en")))
    locale = ASCIIToWide("en-US");

  string16 brand;
  if (!GoogleUpdateSettings::GetBrand(&brand))
    brand = ASCIIToWide("");  // Could still be viable for catch-all rules.

  for (int i = 0; i < arraysize(kExperiments); ++i) {
    if (kExperiments[i].locale != locale &&
        kExperiments[i].locale != ASCIIToWide("*"))
      continue;

    std::vector<string16> brand_codes;
    base::SplitString(kExperiments[i].brands, L',', &brand_codes);
    if (brand_codes.empty())
      return false;
    for (std::vector<string16>::iterator it = brand_codes.begin();
         it != brand_codes.end(); ++it) {
      if (*it != brand && *it != L"*")
        continue;
      // We have found our match.
      const UserExperimentDetails& match = kExperiments[i];
      // Find out how many flavors we have. Zero means no experiment.
      int num_flavors = 0;
      while (match.flavors[num_flavors].heading_id) { ++num_flavors; }
      if (!num_flavors)
        return false;

      if (flavor < 0)
        flavor = base::RandInt(0, num_flavors - 1);
      experiment->flavor = flavor;
      experiment->heading = match.flavors[flavor].heading_id;
      experiment->control_group = match.control_group;
      const wchar_t prefix[] = { match.prefix[0], match.prefix[1] + flavor, 0 };
      experiment->prefix = prefix;
      experiment->flags = match.flavors[flavor].flags;
      return true;
    }
  }

  return false;
}

// Currently we only have one experiment: the inactive user toast. Which only
// applies for users doing upgrades.

//
// There are three scenarios when this function is called:
// 1- Is a per-user-install and it updated: perform the experiment
// 2- Is a system-install and it updated : relaunch as the interactive user
// 3- It has been re-launched from the #2 case. In this case we enter
//    this function with |system_install| true and a REENTRY_SYS_UPDATE status.
void GoogleChromeDistribution::LaunchUserExperiment(
    const FilePath& setup_path, installer::InstallStatus status,
    const Version& version, const installer::Product& product,
    bool system_level) {
  VLOG(1) << "LaunchUserExperiment status: " << status << " product: "
          << product.distribution()->GetAppShortCutName()
          << " system_level: " << system_level;

  if (system_level) {
    if (installer::NEW_VERSION_UPDATED == status) {
      // We need to relaunch as the interactive user.
      LaunchSetupAsConsoleUser(setup_path, product,
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
  string16 base_group = experiment.prefix;

  string16 brand;
  if (GoogleUpdateSettings::GetBrand(&brand) && (brand == L"CHXX")) {
    // Testing only: the user automatically qualifies for the experiment.
    VLOG(1) << "Experiment qualification bypass";
  } else {
    // Check that the user was not already drafted in this experiment.
    string16 client;
    GoogleUpdateSettings::GetClient(&client);
    if (client.size() > 2) {
      if (base_group == client.substr(0, 2)) {
        VLOG(1) << "User already participated in this experiment";
        return;
      }
    }
    // Check browser usage inactivity by the age of the last-write time of the
    // most recently-used chrome user data directory.
    std::vector<FilePath> user_data_dirs;
    product.GetUserDataPaths(&user_data_dirs);
    int dir_age_hours = -1;
    for (size_t i = 0; i < user_data_dirs.size(); ++i) {
      int this_age = GetDirectoryWriteAgeInHours(
          user_data_dirs[i].value().c_str());
      if (this_age >= 0 && (dir_age_hours < 0 || this_age < dir_age_hours))
        dir_age_hours = this_age;
    }

    const bool experiment_enabled = false;
    const int kThirtyDays = 30 * 24;

    if (!experiment_enabled) {
      VLOG(1) << "Toast experiment is disabled.";
      return;
    } else if (dir_age_hours < 0) {
      // This means that we failed to find the user data dir. The most likely
      // cause is that this user has not ever used chrome at all which can
      // happen in a system-level install.
      SetClient(base_group + kToastUDDirFailure, true);
      return;
    } else if (dir_age_hours < kThirtyDays) {
      // An active user, so it does not qualify.
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
  LaunchSetup(&cmd_line, product, system_level);
}

// User qualifies for the experiment. To test, use --try-chrome-again=|flavor|
// as a parameter to chrome.exe.
void GoogleChromeDistribution::InactiveUserToastExperiment(int flavor,
    const string16& experiment_group,
    const installer::Product& installation,
    const FilePath& application_path) {
  bool has_welcome_url = (flavor == 0);
  // Possibly add a url to launch depending on the experiment flavor.
  CommandLine options(CommandLine::NO_PROGRAM);
  options.AppendSwitchNative(switches::kTryChromeAgain,
      base::IntToString16(flavor));
  if (has_welcome_url) {
    // Prepend the url with a space.
    string16 url(GetWelcomeBackUrl());
    options.AppendArg("--");
    options.AppendArgNative(url);
    // The command line should now have the url added as:
    // "chrome.exe -- <url>"
    DCHECK_NE(string16::npos,
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

  if (outcome == kToastExpTriesOkGroup) {
    // User tried chrome, but if it had the default group button it belongs
    // to a different outcome group.
    UserExperiment experiment;
    if (GetExperimentDetails(&experiment, flavor)) {
      outcome = experiment.flags & kMakeDefault ? kToastExpTriesOkDefaultGroup :
                                                  kToastExpTriesOkGroup;
    }
  }

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
