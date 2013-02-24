// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <shlwapi.h>

#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/environment.h"
#include "base/file_util.h"
#include "base/file_version_info.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/rand_util.h"  // For PreRead experiment.
#include "base/sha1.h"  // For PreRead experiment.
#include "base/string16.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "chrome/app/breakpad_win.h"
#include "chrome/app/client_util.h"
#include "chrome/app/image_pre_reader_win.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/env_vars.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/channel_info.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/util_constants.h"

namespace {
// The entry point signature of chrome.dll.
typedef int (*DLL_MAIN)(HINSTANCE, sandbox::SandboxInterfaceInfo*);

typedef void (*RelaunchChromeBrowserWithNewCommandLineIfNeededFunc)();

// Gets chrome version according to the load path. |exe_path| must be the
// backslash terminated directory of the current chrome.exe.
bool GetChromeVersion(const wchar_t* exe_dir, const wchar_t* key_path,
                      string16* version) {
  HKEY reg_root = InstallUtil::IsPerUserInstall(exe_dir) ? HKEY_CURRENT_USER :
                                                           HKEY_LOCAL_MACHINE;
  bool success = false;

  base::win::RegKey key(reg_root, key_path, KEY_QUERY_VALUE);
  if (key.Valid()) {
    // If 'new_chrome.exe' is present it means chrome was auto-updated while
    // running. We need to consult the opv value so we can load the old dll.
    // TODO(cpu) : This is solving the same problem as the environment variable
    // so one of them will eventually be deprecated.
    string16 new_chrome_exe(exe_dir);
    new_chrome_exe.append(installer::kChromeNewExe);
    if (::PathFileExistsW(new_chrome_exe.c_str()) &&
        key.ReadValue(google_update::kRegOldVersionField,
                      version) == ERROR_SUCCESS) {
      success = true;
    } else {
      success = (key.ReadValue(google_update::kRegVersionField,
                               version) == ERROR_SUCCESS);
    }
  }

  return success;
}

// Not generic, we only handle strings up to 128 chars.
bool EnvQueryStr(const wchar_t* key_name, string16* value) {
  wchar_t out[128];
  DWORD count = sizeof(out)/sizeof(out[0]);
  DWORD rv = ::GetEnvironmentVariableW(key_name, out, count);
  if ((rv == 0) || (rv >= count))
    return false;
  *value = out;
  return true;
}

#if defined(OS_WIN)
#if defined(GOOGLE_CHROME_BUILD)
// These constants are used by the PreRead experiment.
const wchar_t kPreReadRegistryValue[] = L"PreReadExperimentGroup";
const int kPreReadExpiryYear = 2012;
const int kPreReadExpiryMonth = 7;
const int kPreReadExpiryDay = 1;

// These are control values.
const DWORD kPreReadExperimentFull = 100;
const DWORD kPreReadExperimentNone = 0;

// Modulate these for different experiments. These values should be a multiple
// of 5 between 0 and 100, exclusive.
const DWORD kPreReadExperimentA = 25;
const DWORD kPreReadExperimentB = 40;

void StaticAssertions() {
  COMPILE_ASSERT(
      kPreReadExperimentA <= 100, kPreReadExperimentA_exceeds_100);
  COMPILE_ASSERT(
      kPreReadExperimentA % 5 == 0, kPreReadExperimentA_is_not_a_multiple_of_5);
  COMPILE_ASSERT(
      kPreReadExperimentB <= 100, kPreReadExperimentB_exceeds_100);
  COMPILE_ASSERT(
      kPreReadExperimentB % 5 == 0, kPreReadExperimentB_is_not_a_multiple_of_5);
}

bool PreReadExperimentShouldRun() {
  base::Time::Exploded exploded = { 0 };
  exploded.year = kPreReadExpiryYear;
  exploded.month = kPreReadExpiryMonth;
  exploded.day_of_month = kPreReadExpiryDay;

  base::Time expiration_time = base::Time::FromLocalExploded(exploded);

  // Get the build time. This code is copied from
  // base::FieldTrial::GetBuildTime. We can't use MetricsLogBase::GetBuildTime
  // because that's in seconds since Unix epoch, which base::Time can't use.
  base::Time build_time;
  const char* kDateTime = __DATE__ " " __TIME__;
  bool result = base::Time::FromString(kDateTime, &build_time);
  DCHECK(result);

  // If the experiment is expired, don't run it.
  return (build_time <= expiration_time);
}

// For channels with small populations we just divide the population evenly
// across the 4 buckets.
DWORD GetSmallPopulationPreReadBucket(double rand_unit) {
  DCHECK_GE(rand_unit, 0.0);
  DCHECK_LT(rand_unit, 1.0);
  if (rand_unit < 0.25 || rand_unit > 1.0)
    return kPreReadExperimentFull;  // The default pre-read amount.
  if (rand_unit < 0.50)
    return kPreReadExperimentA;
  if (rand_unit < 0.75)
    return kPreReadExperimentB;
  return kPreReadExperimentNone;
}

// For channels with large populations, we allocate a small percentage of the
// population to each of the experimental buckets, and the rest to the current
// default pre-read behaviour
DWORD GetLargePopulationPreReadBucket(double rand_unit) {
  DCHECK_GE(rand_unit, 0.0);
  DCHECK_LT(rand_unit, 1.0);
  if (rand_unit < 0.97 || rand_unit > 1.0)
    return kPreReadExperimentFull;  // The default pre-read amount.
  if (rand_unit < 0.98)
    return kPreReadExperimentA;
  if (rand_unit < 0.99)
    return kPreReadExperimentB;
  return kPreReadExperimentNone;
}

// Returns true and the |pre_read_percentage| IFF the experiment should run.
// Otherwise, returns false and |pre_read_percentage| is not modified.
bool GetPreReadExperimentGroup(DWORD* pre_read_percentage) {
  DCHECK(pre_read_percentage != NULL);

  // Check if the experiment has expired.
  if (!PreReadExperimentShouldRun())
    return false;

  // Get the MetricsId of the installation. This is only set if the user has
  // opted in to reporting. Doing things this way ensures that we only enable
  // the experiment if its results are actually going to be reported.
  string16 metrics_id;
  if (!GoogleUpdateSettings::GetMetricsId(&metrics_id) || metrics_id.empty())
    return false;

  // We use the same technique as FieldTrial::HashClientId.
  unsigned char sha1_hash[base::kSHA1Length];
  base::SHA1HashBytes(
      reinterpret_cast<const unsigned char*>(metrics_id.c_str()),
      metrics_id.size() * sizeof(metrics_id[0]),
      sha1_hash);
  COMPILE_ASSERT(sizeof(uint64) < sizeof(sha1_hash), need_more_data);
  uint64* bits = reinterpret_cast<uint64*>(&sha1_hash[0]);
  double rand_unit = base::BitsToOpenEndedUnitInterval(*bits);

  // We carve up the bucket sizes based on the population of the channel.
  const string16 channel(
      GoogleUpdateSettings::GetChromeChannel(
          GoogleUpdateSettings::IsSystemInstall()));

  // For our purposes, Stable has a large population, everything else is small.
  *pre_read_percentage = (channel == installer::kChromeChannelStable ?
                              GetLargePopulationPreReadBucket(rand_unit) :
                              GetSmallPopulationPreReadBucket(rand_unit));

  return true;
}
#endif  // if defined(GOOGLE_CHROME_BUILD)
#endif  // if defined(OS_WIN)

// Expects that |dir| has a trailing backslash. |dir| is modified so it
// contains the full path that was tried. Caller must check for the return
// value not being null to determine if this path contains a valid dll.
HMODULE LoadChromeWithDirectory(string16* dir) {
  ::SetCurrentDirectoryW(dir->c_str());
  const CommandLine& cmd_line = *CommandLine::ForCurrentProcess();
  dir->append(installer::kChromeDll);

#ifndef WIN_DISABLE_PREREAD
#ifdef NDEBUG
  // Experimental pre-reading optimization
  // The idea is to pre-read a significant portion of chrome.dll in advance
  // so that subsequent hard page faults are avoided.
  //
  // Pre-read may be disabled at compile time by defining WIN_DISABLE_PREREAD,
  // but by default it is enabled in release builds. The ability to disable it
  // is useful for evaluating competing optimization techniques.
  if (!cmd_line.HasSwitch(switches::kProcessType)) {
    // The kernel brings in 8 pages for the code section at a time and 4 pages
    // for other sections. We can skip over these pages to avoid a soft page
    // fault which may not occur during code execution. However skipping 4K at
    // a time still has better performance over 32K and 16K according to data.
    // TODO(ananta): Investigate this and tune.
    const size_t kStepSize = 4 * 1024;

    // We hypothesize that pre-reading only the bytes actually touched during
    // startup should improve startup time. The Syzygy toolchain attempts to
    // optimize the binary layout of chrome.dll, rearranging the code and data
    // blocks such that temporally related blocks (i.e., code and data used in
    // startup, browser, renderer, etc) are grouped together, and that blocks
    // used early in the process lifecycle occur earlier in their sections.
    DWORD pre_read_percentage = 100;
    DWORD pre_read_step_size = kStepSize;
    bool is_eligible_for_experiment = true;

    // TODO(chrisha): This path should not be ChromeFrame specific, and it
    //     should not be hard-coded with 'Google' in the path. Rather, it should
    //     use the product name.
    base::win::RegKey key(HKEY_CURRENT_USER,
                          L"Software\\Google\\ChromeFrame",
                          KEY_QUERY_VALUE);

    // Check if there are any pre-read settings in the registry. If so, then
    // the pre-read settings have been forcibly set and this instance is not
    // eligible for the pre-read experiment.
    if (key.Valid()) {
      DWORD value = 0;
      if (key.ReadValueDW(L"PreRead", &value) == ERROR_SUCCESS) {
        is_eligible_for_experiment = false;
        pre_read_percentage = (value != 0) ? 100 : 0;
      }

      if (key.ReadValueDW(L"PreReadPercentage", &value) == ERROR_SUCCESS) {
        is_eligible_for_experiment = false;
        pre_read_percentage = value;
      }

      if (key.ReadValueDW(L"PreReadStepSize", &value) == ERROR_SUCCESS) {
        is_eligible_for_experiment = false;
        pre_read_step_size = value;
      }
      key.Close();
    }

#if defined(OS_WIN)
#if defined(GOOGLE_CHROME_BUILD)
    // The PreRead experiment is unable to use the standard FieldTrial
    // mechanism as pre-reading happens in chrome.exe prior to loading
    // chrome.dll. As such, we use a custom approach. If the experiment is
    // running (not expired) then we look to the registry for the BreakPad/UMA
    // metricsid. We use this to seed a random unit, and select a bucket
    // (percentage to pre-read) for the experiment. The selected bucket is
    // communicated to chrome.dll via an environment variable, which alerts
    // chrome.dll that the experiment is running, causing it to report
    // sub-histogram results.
    //
    // If we've read pre-read settings from the registry, then someone has
    // specifically forced their pre-read options and is not participating in
    // the experiment.
    //
    // If the experiment is running, indicate it to chrome.dll via an
    // environment variable that contains the percentage of chrome that
    // was pre-read. Allowable values are all multiples of 5 between
    // 0 and 100, inclusive.
    if (is_eligible_for_experiment &&
        GetPreReadExperimentGroup(&pre_read_percentage)) {
      DCHECK_LE(pre_read_percentage, 100U);
      DCHECK_EQ(pre_read_percentage % 5, 0U);
      scoped_ptr<base::Environment> env(base::Environment::Create());
      env->SetVar(chrome::kPreReadEnvironmentVariable,
                  base::StringPrintf("%d", pre_read_percentage));
    }
#endif  // if defined(GOOGLE_CHROME_BUILD)
#endif  // if defined(OS_WIN)

    // Clamp the DWORD percentage to fit into a uint8 that's <= 100.
    pre_read_percentage = std::min(pre_read_percentage, 100UL);

    // Perform the full or partial pre-read.
    TRACE_EVENT_BEGIN_ETW("PreReadImage", 0, "");
    ImagePreReader::PartialPreReadImage(dir->c_str(),
                                        static_cast<uint8>(pre_read_percentage),
                                        pre_read_step_size);
    TRACE_EVENT_END_ETW("PreReadImage", 0, "");
  }
#endif  // NDEBUG
#endif  // WIN_DISABLE_PREREAD

  return ::LoadLibraryExW(dir->c_str(), NULL,
                          LOAD_WITH_ALTERED_SEARCH_PATH);
}

void RecordDidRun(const string16& dll_path) {
  bool system_level = !InstallUtil::IsPerUserInstall(dll_path.c_str());
  GoogleUpdateSettings::UpdateDidRunState(true, system_level);
}

void ClearDidRun(const string16& dll_path) {
  bool system_level = !InstallUtil::IsPerUserInstall(dll_path.c_str());
  GoogleUpdateSettings::UpdateDidRunState(false, system_level);
}

}  // namespace

string16 GetExecutablePath() {
  wchar_t path[MAX_PATH];
  ::GetModuleFileNameW(NULL, path, MAX_PATH);
  if (!::PathRemoveFileSpecW(path))
    return string16();
  string16 exe_path(path);
  return exe_path.append(1, L'\\');
}

//=============================================================================

MainDllLoader::MainDllLoader() : dll_(NULL) {
}

MainDllLoader::~MainDllLoader() {
}

// Loading chrome is an interesting affair. First we try loading from the
// current directory to support run-what-you-compile and other development
// scenarios.
// If that fails then we look at the --chrome-version command line flag followed
// by the 'CHROME_VERSION' env variable to determine if we should stick with an
// older dll version even if a new one is available to support upgrade-in-place
// scenarios.
// If that fails then finally we look at the registry which should point us
// to the latest version. This is the expected path for the first chrome.exe
// browser instance in an installed build.
HMODULE MainDllLoader::Load(string16* out_version, string16* out_file) {
  const string16 dir(GetExecutablePath());
  *out_file = dir;
  HMODULE dll = LoadChromeWithDirectory(out_file);
  if (dll)
    return dll;

  string16 version_string;
  Version version;
  const CommandLine& cmd_line = *CommandLine::ForCurrentProcess();
  if (cmd_line.HasSwitch(switches::kChromeVersion)) {
    version_string = cmd_line.GetSwitchValueNative(switches::kChromeVersion);
    version = Version(base::WideToASCII(version_string));

    if (!version.IsValid()) {
      // If a bogus command line flag was given, then abort.
      LOG(ERROR) << "Invalid command line version: " << version_string;
      return NULL;
    }
  }

  // If no version on the command line, then look at the version resource in
  // the current module and try loading that.
  if (!version.IsValid()) {
    scoped_ptr<FileVersionInfo> file_version_info(
        FileVersionInfo::CreateFileVersionInfoForCurrentModule());
    if (file_version_info.get()) {
      version_string = file_version_info->file_version();
      version = Version(base::WideToASCII(version_string));
    }
  }

  // TODO(robertshield): in theory, these next two checks (env and registry)
  // should never be needed. Remove them when I become 100% certain this is
  // also true in practice.

  // If no version in the current module, then look in the environment.
  if (!version.IsValid()) {
    if (EnvQueryStr(base::ASCIIToWide(chrome::kChromeVersionEnvVar).c_str(),
                    &version_string)) {
      version = Version(base::WideToASCII(version_string));
      LOG_IF(ERROR, !version.IsValid()) << "Invalid environment version: "
                                        << version_string;
    }
  }

  // If no version in the environment, then look in the registry.
  if (!version.IsValid()) {
    version_string = GetVersion();
    if (version_string.empty()) {
      LOG(ERROR) << "Could not get Chrome DLL version.";
      return NULL;
    }
  }

  *out_file = dir;
  *out_version = version_string;
  out_file->append(*out_version).append(1, L'\\');
  dll = LoadChromeWithDirectory(out_file);
  if (!dll) {
    LOG(ERROR) << "Failed to load Chrome DLL from " << *out_file;
    return NULL;
  }

  return dll;
}

// Launching is a matter of loading the right dll, setting the CHROME_VERSION
// environment variable and just calling the entry point. Derived classes can
// add custom code in the OnBeforeLaunch callback.
int MainDllLoader::Launch(HINSTANCE instance,
                          sandbox::SandboxInterfaceInfo* sbox_info) {
  string16 version;
  string16 file;
  dll_ = Load(&version, &file);
  if (!dll_)
    return chrome::RESULT_CODE_MISSING_DATA;

  scoped_ptr<base::Environment> env(base::Environment::Create());
  env->SetVar(chrome::kChromeVersionEnvVar, base::WideToUTF8(version));
  // TODO(erikwright): Remove this when http://crbug.com/174953 is fixed and
  // widely deployed.
  env->UnSetVar(env_vars::kGoogleUpdateIsMachineEnvVar);

  InitCrashReporter();
  OnBeforeLaunch(file);

  DLL_MAIN entry_point =
      reinterpret_cast<DLL_MAIN>(::GetProcAddress(dll_, "ChromeMain"));
  if (!entry_point)
    return chrome::RESULT_CODE_BAD_PROCESS_TYPE;

  int rc = entry_point(instance, sbox_info);
  return OnBeforeExit(rc, file);
}

string16 MainDllLoader::GetVersion() {
  string16 reg_path(GetRegistryPath());
  string16 version_string;
  string16 dir(GetExecutablePath());
  if (!GetChromeVersion(dir.c_str(), reg_path.c_str(), &version_string))
    return string16();
  return version_string;
}

void MainDllLoader::RelaunchChromeBrowserWithNewCommandLineIfNeeded() {
  RelaunchChromeBrowserWithNewCommandLineIfNeededFunc relaunch_function =
      reinterpret_cast<RelaunchChromeBrowserWithNewCommandLineIfNeededFunc>(
          ::GetProcAddress(dll_,
                           "RelaunchChromeBrowserWithNewCommandLineIfNeeded"));
  if (!relaunch_function) {
    LOG(ERROR) << "Could not find exported function "
               << "RelaunchChromeBrowserWithNewCommandLineIfNeeded";
  } else {
    relaunch_function();
  }
}

//=============================================================================

class ChromeDllLoader : public MainDllLoader {
 public:
  virtual string16 GetRegistryPath() {
    string16 key(google_update::kRegPathClients);
    BrowserDistribution* dist = BrowserDistribution::GetDistribution();
    key.append(L"\\").append(dist->GetAppGuid());
    return key;
  }

  virtual void OnBeforeLaunch(const string16& dll_path) {
    RecordDidRun(dll_path);
  }

  virtual int OnBeforeExit(int return_code, const string16& dll_path) {
    // NORMAL_EXIT_CANCEL is used for experiments when the user cancels
    // so we need to reset the did_run signal so omaha does not count
    // this run as active usage.
    if (chrome::RESULT_CODE_NORMAL_EXIT_CANCEL == return_code) {
      ClearDidRun(dll_path);
    }
    return return_code;
  }
};

//=============================================================================

class ChromiumDllLoader : public MainDllLoader {
 public:
  virtual string16 GetRegistryPath() {
    BrowserDistribution* dist = BrowserDistribution::GetDistribution();
    return dist->GetVersionKey();
  }
};

MainDllLoader* MakeMainDllLoader() {
#if defined(GOOGLE_CHROME_BUILD)
  return new ChromeDllLoader();
#else
  return new ChromiumDllLoader();
#endif
}
