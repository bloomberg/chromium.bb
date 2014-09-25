// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/user_experiment.h"

#include <windows.h>
#include <sddl.h>
#include <wtsapi32.h>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"
#include "chrome/common/attrition_experiments.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/product.h"
#include "content/public/common/result_codes.h"

#pragma comment(lib, "wtsapi32.lib")

namespace installer {

namespace {

// The following strings are the possible outcomes of the toast experiment
// as recorded in the |client| field.
const wchar_t kToastExpControlGroup[] =        L"01";
const wchar_t kToastExpCancelGroup[] =         L"02";
const wchar_t kToastExpUninstallGroup[] =      L"04";
const wchar_t kToastExpTriesOkGroup[] =        L"18";
const wchar_t kToastExpTriesErrorGroup[] =     L"28";
const wchar_t kToastActiveGroup[] =            L"40";
const wchar_t kToastUDDirFailure[] =           L"40";
const wchar_t kToastExpBaseGroup[] =           L"80";

// Substitute the locale parameter in uninstall URL with whatever
// Google Update tells us is the locale. In case we fail to find
// the locale, we use US English.
base::string16 LocalizeUrl(const wchar_t* url) {
  base::string16 language;
  if (!GoogleUpdateSettings::GetLanguage(&language))
    language = L"en-US";  // Default to US English.
  return ReplaceStringPlaceholders(url, language.c_str(), NULL);
}

base::string16 GetWelcomeBackUrl() {
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
  base::win::ScopedHandle file(::CreateFileW(path, 0, share, NULL,
      OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL));
  if (!file.IsValid())
    return -1;

  FILETIME time;
  return ::GetFileTime(file.Get(), NULL, NULL, &time) ?
      FileTimeToHours(time) : -1;
}

// Returns the time in hours since the last write to the user data directory.
// A return value of 14 means that the directory was last written 14 hours ago.
// Returns -1 if there was an error retrieving the directory.
int GetUserDataDirectoryWriteAgeInHours() {
  base::FilePath user_data_dir;
  if (!PathService::Get(chrome::DIR_USER_DATA, &user_data_dir))
    return -1;
  int dir_time = GetDirectoryWriteTimeInHours(user_data_dir.value().c_str());
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
bool LaunchSetup(CommandLine* cmd_line, bool system_level_toast) {
  const CommandLine& current_cmd_line = *CommandLine::ForCurrentProcess();

  // Propagate --verbose-logging to the invoked setup.exe.
  if (current_cmd_line.HasSwitch(switches::kVerboseLogging))
    cmd_line->AppendSwitch(switches::kVerboseLogging);

  // Re-add the system level toast flag.
  if (system_level_toast) {
    cmd_line->AppendSwitch(switches::kSystemLevel);
    cmd_line->AppendSwitch(switches::kSystemLevelToast);

    // Re-add the toast result key. We need to do this because Setup running as
    // system passes the key to Setup running as user, but that child process
    // does not perform the actual toasting, it launches another Setup (as user)
    // to do so. That is the process that needs the key.
    std::string key(switches::kToastResultsKey);
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
bool FixDACLsForExecute(const base::FilePath& exe) {
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
  base::string16 new_sddl(sddl);
  ::LocalFree(sddl);
  sd = NULL;
  // See MSDN for the  security descriptor definition language (SDDL) syntax,
  // in our case we add "A;" generic read 'GR' and generic execute 'GX' for
  // the nt\authenticated_users 'AU' group, that becomes:
  const wchar_t kAllowACE[] = L"(A;;GRGX;;;AU)";
  // We should check that there are no special ACES for the group we
  // are interested, which is nt\authenticated_users.
  if (base::string16::npos != new_sddl.find(L";AU)"))
    return false;
  // Specific ACEs (not inherited) need to go to the front. It is ok if we
  // are the very first one.
  size_t pos_insert = new_sddl.find(L"(");
  if (base::string16::npos == pos_insert)
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
bool LaunchSetupAsConsoleUser(CommandLine* cmd_line) {
  // Convey to the invoked setup.exe that it's operating on a system-level
  // installation.
  cmd_line->AppendSwitch(switches::kSystemLevel);

  // Propagate --verbose-logging to the invoked setup.exe.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kVerboseLogging))
    cmd_line->AppendSwitch(switches::kVerboseLogging);

  // Get the Google Update results key, and pass it on the command line to
  // the child process.
  int key = GoogleUpdateSettings::DuplicateGoogleUpdateSystemClientKey();
  cmd_line->AppendSwitchASCII(switches::kToastResultsKey,
                              base::IntToString(key));

  if (base::win::GetVersion() > base::win::VERSION_XP) {
    // Make sure that in Vista and Above we have the proper DACLs so
    // the interactive user can launch it.
    if (!FixDACLsForExecute(cmd_line->GetProgram()))
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
  VLOG(1) << __FUNCTION__ << " launching " << cmd_line->GetCommandLineString();
  bool launched = base::LaunchProcess(*cmd_line, options, NULL);
  ::CloseHandle(user_token);
  VLOG(1) << __FUNCTION__ << "   result: " << launched;
  return launched;
}

// A helper function that writes to HKLM if the handle was passed through the
// command line, but HKCU otherwise. |experiment_group| is the value to write
// and |last_write| is used when writing to HKLM to determine whether to close
// the handle when done.
void SetClient(const base::string16& experiment_group, bool last_write) {
  static int reg_key_handle = -1;
  if (reg_key_handle == -1) {
    // If a specific Toast Results key handle (presumably to our HKLM key) was
    // passed in to the command line (such as for system level installs), we use
    // it. Otherwise, we write to the key under HKCU.
    const CommandLine& cmd_line = *CommandLine::ForCurrentProcess();
    if (cmd_line.HasSwitch(switches::kToastResultsKey)) {
      // Get the handle to the key under HKLM.
      base::StringToInt(
          cmd_line.GetSwitchValueNative(switches::kToastResultsKey),
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

}  // namespace

bool CreateExperimentDetails(int flavor, ExperimentDetails* experiment) {
  struct FlavorDetails {
    int heading_id;
    int flags;
  };
  // Maximum number of experiment flavors we support.
  static const int kMax = 4;
  // This struct determines which experiment flavors we show for each locale and
  // brand.
  //
  // Plugin infobar experiment:
  // The experiment in 2011 used PIxx codes.
  //
  // Inactive user toast experiment:
  // The experiment in Dec 2009 used TGxx and THxx.
  // The experiment in Feb 2010 used TKxx and TLxx.
  // The experiment in Apr 2010 used TMxx and TNxx.
  // The experiment in Oct 2010 used TVxx TWxx TXxx TYxx.
  // The experiment in Feb 2011 used SJxx SKxx SLxx SMxx.
  // The experiment in Mar 2012 used ZAxx ZBxx ZCxx.
  // The experiment in Jan 2013 uses DAxx.
  using namespace attrition_experiments;

  static const struct UserExperimentSpecs {
    const wchar_t* locale;  // Locale to show this experiment for (* for all).
    const wchar_t* brands;  // Brand codes show this experiment for (* for all).
    int control_group;      // Size of the control group, in percentages.
    const wchar_t* prefix;  // The two letter experiment code. The second letter
                            // will be incremented with the flavor.
    FlavorDetails flavors[kMax];
  } kExperiments[] = {
    // The first match from top to bottom is used so this list should be ordered
    // most-specific rule first.
    { L"*", L"GGRV",  // All locales, GGRV is enterprise.
      0,              // 0 percent control group.
      L"EA",          // Experiment is EAxx, EBxx, etc.
      // No flavors means no experiment.
      { { 0, 0 },
        { 0, 0 },
        { 0, 0 },
        { 0, 0 }
      }
    },
    { L"*", L"*",     // All locales, all brands.
      5,              // 5 percent control group.
      L"DA",          // Experiment is DAxx.
      // One single flavor.
      { { IDS_TRY_TOAST_HEADING3, kToastUiMakeDefault },
        { 0, 0 },
        { 0, 0 },
        { 0, 0 }
      }
    }
  };

  base::string16 locale;
  GoogleUpdateSettings::GetLanguage(&locale);
  if (locale.empty() || (locale == base::ASCIIToWide("en")))
    locale = base::ASCIIToWide("en-US");

  base::string16 brand;
  if (!GoogleUpdateSettings::GetBrand(&brand))
    brand = base::ASCIIToWide("");  // Could still be viable for catch-all rules

  for (int i = 0; i < arraysize(kExperiments); ++i) {
    if (kExperiments[i].locale != locale &&
        kExperiments[i].locale != base::ASCIIToWide("*"))
      continue;

    std::vector<base::string16> brand_codes;
    base::SplitString(kExperiments[i].brands, L',', &brand_codes);
    if (brand_codes.empty())
      return false;
    for (std::vector<base::string16>::iterator it = brand_codes.begin();
         it != brand_codes.end(); ++it) {
      if (*it != brand && *it != L"*")
        continue;
      // We have found our match.
      const UserExperimentSpecs& match = kExperiments[i];
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

// There are three scenarios when this function is called:
// 1- Is a per-user-install and it updated: perform the experiment
// 2- Is a system-install and it updated : relaunch as the interactive user
// 3- It has been re-launched from the #2 case. In this case we enter
//    this function with |system_install| true and a REENTRY_SYS_UPDATE status.
void LaunchBrowserUserExperiment(const CommandLine& base_cmd_line,
                                 InstallStatus status,
                                 bool system_level) {
  if (system_level) {
    if (NEW_VERSION_UPDATED == status) {
      CommandLine cmd_line(base_cmd_line);
      cmd_line.AppendSwitch(switches::kSystemLevelToast);
      // We need to relaunch as the interactive user.
      LaunchSetupAsConsoleUser(&cmd_line);
      return;
    }
  } else {
    if (status != NEW_VERSION_UPDATED && status != REENTRY_SYS_UPDATE) {
      // We are not updating or in re-launch. Exit.
      return;
    }
  }

  // The |flavor| value ends up being processed by TryChromeDialogView to show
  // different experiments.
  ExperimentDetails experiment;
  if (!CreateExperimentDetails(-1, &experiment)) {
    VLOG(1) << "Failed to get experiment details.";
    return;
  }
  int flavor = experiment.flavor;
  base::string16 base_group = experiment.prefix;

  base::string16 brand;
  if (GoogleUpdateSettings::GetBrand(&brand) && (brand == L"CHXX")) {
    // Testing only: the user automatically qualifies for the experiment.
    VLOG(1) << "Experiment qualification bypass";
  } else {
    // Check that the user was not already drafted in this experiment.
    base::string16 client;
    GoogleUpdateSettings::GetClient(&client);
    if (client.size() > 2) {
      if (base_group == client.substr(0, 2)) {
        VLOG(1) << "User already participated in this experiment";
        return;
      }
    }
    const bool experiment_enabled = false;
    if (!experiment_enabled) {
      VLOG(1) << "Toast experiment is disabled.";
      return;
    }

    // Check browser usage inactivity by the age of the last-write time of the
    // relevant chrome user data directory.
    const int kThirtyDays = 30 * 24;
    const int dir_age_hours = GetUserDataDirectoryWriteAgeInHours();
    if (dir_age_hours < 0) {
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
  CommandLine cmd_line(base_cmd_line);
  cmd_line.AppendSwitchASCII(switches::kInactiveUserToast,
                             base::IntToString(flavor));
  cmd_line.AppendSwitchASCII(switches::kExperimentGroup,
                             base::UTF16ToASCII(base_group));
  LaunchSetup(&cmd_line, system_level);
}

// User qualifies for the experiment. To test, use --try-chrome-again=|flavor|
// as a parameter to chrome.exe.
void InactiveUserToastExperiment(int flavor,
                                 const base::string16& experiment_group,
                                 const Product& product,
                                 const base::FilePath& application_path) {
  // Add the 'welcome back' url for chrome to show.
  CommandLine options(CommandLine::NO_PROGRAM);
  options.AppendSwitchNative(::switches::kTryChromeAgain,
      base::IntToString16(flavor));
  // Prepend the url with a space.
  base::string16 url(GetWelcomeBackUrl());
  options.AppendArg("--");
  options.AppendArgNative(url);
  // The command line should now have the url added as:
  // "chrome.exe -- <url>"
  DCHECK_NE(base::string16::npos,
            options.GetCommandLineString().find(L" -- " + url));

  // Launch chrome now. It will show the toast UI.
  int32 exit_code = 0;
  if (!product.LaunchChromeAndWait(application_path, options, &exit_code))
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
  }
  // Write to the |client| key for the last time.
  SetClient(experiment_group + outcome, true);

  if (outcome != kToastExpUninstallGroup)
    return;
  // The user wants to uninstall. This is a best effort operation. Note that
  // we waited for chrome to exit so the uninstall would not detect chrome
  // running.
  bool system_level_toast = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kSystemLevelToast);

  CommandLine cmd(InstallUtil::GetChromeUninstallCmd(
                      system_level_toast, product.distribution()->GetType()));
  base::LaunchProcess(cmd, base::LaunchOptions(), NULL);
}

}  // namespace installer
