// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/sw_reporter_installer_win.h"

#include <stdint.h>

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/base_paths.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/win/registry.h"
#include "base/win/windows_version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/safe_browsing/srt_fetcher_win.h"
#include "chrome/browser/safe_browsing/srt_field_trial_win.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/update_client/update_client.h"
#include "components/update_client/utils.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_thread.h"

namespace component_updater {

namespace {

using safe_browsing::SwReporterInvocation;

// These values are used to send UMA information and are replicated in the
// histograms.xml file, so the order MUST NOT CHANGE.
enum SRTCompleted {
  SRT_COMPLETED_NOT_YET = 0,
  SRT_COMPLETED_YES = 1,
  SRT_COMPLETED_LATER = 2,
  SRT_COMPLETED_MAX,
};

// CRX hash. The extension id is: gkmgaooipdjhmangpemjhigmamcehddo. The hash was
// generated in Python with something like this:
// hashlib.sha256().update(open("<file>.crx").read()[16:16+294]).digest().
const uint8_t kSha256Hash[] = {0x6a, 0xc6, 0x0e, 0xe8, 0xf3, 0x97, 0xc0, 0xd6,
                               0xf4, 0xc9, 0x78, 0x6c, 0x0c, 0x24, 0x73, 0x3e,
                               0x05, 0xa5, 0x62, 0x4b, 0x2e, 0xc7, 0xb7, 0x1c,
                               0x5f, 0xea, 0xf0, 0x88, 0xf6, 0x97, 0x9b, 0xc7};

const base::FilePath::CharType kSwReporterExeName[] =
    FILE_PATH_LITERAL("software_reporter_tool.exe");

constexpr char kSessionIdSwitch[] = "session-id";

// SRT registry keys and value names.
const wchar_t kCleanerSuffixRegistryKey[] = L"Cleaner";
const wchar_t kExitCodeValueName[] = L"ExitCode";
const wchar_t kUploadResultsValueName[] = L"UploadResults";
const wchar_t kVersionValueName[] = L"Version";

constexpr base::Feature kExperimentalEngineFeature{
    "ExperimentalSwReporterEngine", base::FEATURE_DISABLED_BY_DEFAULT};
constexpr base::Feature kExperimentalEngineAllArchsFeature{
    "ExperimentalSwReporterEngineOnAllArchitectures",
    base::FEATURE_DISABLED_BY_DEFAULT
};

void SRTHasCompleted(SRTCompleted value) {
  UMA_HISTOGRAM_ENUMERATION("SoftwareReporter.Cleaner.HasCompleted", value,
                            SRT_COMPLETED_MAX);
}

void ReportUploadsWithUma(const base::string16& upload_results) {
  base::WStringTokenizer tokenizer(upload_results, L";");
  int failure_count = 0;
  int success_count = 0;
  int longest_failure_run = 0;
  int current_failure_run = 0;
  bool last_result = false;
  while (tokenizer.GetNext()) {
    if (tokenizer.token() == L"0") {
      ++failure_count;
      ++current_failure_run;
      last_result = false;
    } else {
      ++success_count;
      current_failure_run = 0;
      last_result = true;
    }

    if (current_failure_run > longest_failure_run)
      longest_failure_run = current_failure_run;
  }

  UMA_HISTOGRAM_COUNTS_100("SoftwareReporter.UploadFailureCount",
                           failure_count);
  UMA_HISTOGRAM_COUNTS_100("SoftwareReporter.UploadSuccessCount",
                           success_count);
  UMA_HISTOGRAM_COUNTS_100("SoftwareReporter.UploadLongestFailureRun",
                           longest_failure_run);
  UMA_HISTOGRAM_BOOLEAN("SoftwareReporter.LastUploadResult", last_result);
}

void ReportExperimentError(SwReporterExperimentError error) {
  UMA_HISTOGRAM_ENUMERATION("SoftwareReporter.ExperimentErrors", error,
                            SW_REPORTER_EXPERIMENT_ERROR_MAX);
}

// Once the Software Reporter is downloaded, schedules it to run sometime after
// the current browser startup is complete. (This is the default
// |reporter_runner| function passed to the |SwReporterInstallerTraits|
// constructor in |RegisterSwReporterComponent| below.)
void RunSwReportersAfterStartup(
    const safe_browsing::SwReporterQueue& invocations,
    const base::Version& version) {
  content::BrowserThread::PostAfterStartupTask(
      FROM_HERE, base::ThreadTaskRunnerHandle::Get(),
      base::Bind(&safe_browsing::RunSwReporters, invocations, version));
}

// Ensures |str| contains only alphanumeric characters and characters from
// |extras|, and is not longer than |max_length|.
bool ValidateString(const std::string& str,
                    const std::string& extras,
                    size_t max_length) {
  return str.size() <= max_length &&
         std::all_of(str.cbegin(), str.cend(), [&extras](char c) {
           return base::IsAsciiAlpha(c) || base::IsAsciiDigit(c) ||
                  extras.find(c) != std::string::npos;
         });
}

std::string GenerateSessionId() {
  std::string session_id;
  base::Base64Encode(base::RandBytesAsString(30), &session_id);
  DCHECK(!session_id.empty());
  return session_id;
}

// Add |behaviour_flag| to |supported_behaviours| if |behaviour_name| is found
// in the dictionary. Returns false on error.
bool GetOptionalBehaviour(
    const base::DictionaryValue* invocation_params,
    base::StringPiece behaviour_name,
    SwReporterInvocation::Behaviours behaviour_flag,
    SwReporterInvocation::Behaviours* supported_behaviours) {
  DCHECK(invocation_params);
  DCHECK(supported_behaviours);

  // Parameters enabling behaviours are optional, but if present must be
  // boolean.
  const base::Value* value = nullptr;
  if (invocation_params->Get(behaviour_name, &value)) {
    bool enable_behaviour = false;
    if (!value->GetAsBoolean(&enable_behaviour)) {
      ReportExperimentError(SW_REPORTER_EXPERIMENT_ERROR_BAD_PARAMS);
      return false;
    }
    if (enable_behaviour)
      *supported_behaviours |= behaviour_flag;
  }
  return true;
}

// Reads the command-line params and an UMA histogram suffix from the manifest,
// and launch the SwReporter with those parameters. If anything goes wrong the
// SwReporter should not be run at all.
void RunExperimentalSwReporter(const base::FilePath& exe_path,
                               const base::Version& version,
                               std::unique_ptr<base::DictionaryValue> manifest,
                               const SwReporterRunner& reporter_runner) {
  // The experiment requires launch_params so if they aren't present just
  // return. This isn't an error because the user could get into the experiment
  // group before they've downloaded the experiment component.
  base::Value* launch_params = nullptr;
  if (!manifest->Get("launch_params", &launch_params))
    return;

  const base::ListValue* parameter_list = nullptr;
  if (!launch_params->GetAsList(&parameter_list) || parameter_list->empty()) {
    ReportExperimentError(SW_REPORTER_EXPERIMENT_ERROR_BAD_PARAMS);
    return;
  }

  const std::string session_id = GenerateSessionId();

  safe_browsing::SwReporterQueue invocations;
  for (const auto& iter : *parameter_list) {
    const base::DictionaryValue* invocation_params = nullptr;
    if (!iter->GetAsDictionary(&invocation_params)) {
      ReportExperimentError(SW_REPORTER_EXPERIMENT_ERROR_BAD_PARAMS);
      return;
    }

    // Max length of the registry and histogram suffix. Fairly arbitrary: the
    // Windows registry accepts much longer keys, but we need to display this
    // string in histograms as well.
    constexpr size_t kMaxSuffixLength = 80;

    // The suffix must be an alphanumeric string. (Empty is fine as long as the
    // "suffix" key is present.)
    std::string suffix;
    if (!invocation_params->GetString("suffix", &suffix) ||
        !ValidateString(suffix, std::string(), kMaxSuffixLength)) {
      ReportExperimentError(SW_REPORTER_EXPERIMENT_ERROR_BAD_PARAMS);
      return;
    }

    // Build a command line for the reporter out of the executable path and the
    // arguments from the manifest. (The "arguments" key must be present, but
    // it's ok if it's an empty list or a list of empty strings.)
    const base::ListValue* arguments = nullptr;
    if (!invocation_params->GetList("arguments", &arguments)) {
      ReportExperimentError(SW_REPORTER_EXPERIMENT_ERROR_BAD_PARAMS);
      return;
    }

    std::vector<base::string16> argv = {exe_path.value()};
    for (const auto& value : *arguments) {
      base::string16 argument;
      if (!value->GetAsString(&argument)) {
        ReportExperimentError(SW_REPORTER_EXPERIMENT_ERROR_BAD_PARAMS);
        return;
      }
      if (!argument.empty())
        argv.push_back(argument);
    }

    base::CommandLine command_line(argv);

    // Add a random session id to link experimental reporter runs together.
    command_line.AppendSwitchASCII(kSessionIdSwitch, session_id);

    const std::string experiment_group =
        variations::GetVariationParamValueByFeature(
            kExperimentalEngineFeature, "experiment_group_for_reporting");
    command_line.AppendSwitchNative("engine-experiment-group",
                                    experiment_group.empty()
                                        ? L"missing_experiment_group"
                                        : base::UTF8ToUTF16(experiment_group));

    // Add the histogram suffix to the command-line as well, so that the
    // reporter will add the same suffix to registry keys where it writes
    // metrics.
    if (!suffix.empty())
      command_line.AppendSwitchASCII("registry-suffix", suffix);

    SwReporterInvocation::Behaviours supported_behaviours = 0;
    if (!GetOptionalBehaviour(invocation_params, "prompt",
                              SwReporterInvocation::BEHAVIOUR_TRIGGER_PROMPT,
                              &supported_behaviours))
      return;
    if (!GetOptionalBehaviour(
            invocation_params, "allow-reporter-logs",
            SwReporterInvocation::BEHAVIOUR_ALLOW_SEND_REPORTER_LOGS,
            &supported_behaviours))
      return;

    auto invocation = SwReporterInvocation::FromCommandLine(command_line);
    invocation.suffix = suffix;
    invocation.supported_behaviours = supported_behaviours;
    invocations.push(invocation);
  }

  DCHECK(!invocations.empty());
  reporter_runner.Run(invocations, version);
}

}  // namespace

SwReporterInstallerTraits::SwReporterInstallerTraits(
    const SwReporterRunner& reporter_runner,
    bool is_experimental_engine_supported)
    : reporter_runner_(reporter_runner),
      is_experimental_engine_supported_(is_experimental_engine_supported) {}

SwReporterInstallerTraits::~SwReporterInstallerTraits() {}

bool SwReporterInstallerTraits::VerifyInstallation(
    const base::DictionaryValue& manifest,
    const base::FilePath& dir) const {
  return base::PathExists(dir.Append(kSwReporterExeName));
}

bool SwReporterInstallerTraits::SupportsGroupPolicyEnabledComponentUpdates()
    const {
  return true;
}

bool SwReporterInstallerTraits::RequiresNetworkEncryption() const {
  return false;
}

update_client::CrxInstaller::Result SwReporterInstallerTraits::OnCustomInstall(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);
}

void SwReporterInstallerTraits::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    std::unique_ptr<base::DictionaryValue> manifest) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const base::FilePath exe_path(install_dir.Append(kSwReporterExeName));
  if (IsExperimentalEngineEnabled()) {
    RunExperimentalSwReporter(exe_path, version, std::move(manifest),
                              reporter_runner_);
  } else {
    base::CommandLine command_line(exe_path);
    command_line.AppendSwitchASCII(kSessionIdSwitch, GenerateSessionId());
    auto invocation = SwReporterInvocation::FromCommandLine(command_line);
    invocation.supported_behaviours =
        SwReporterInvocation::BEHAVIOUR_LOG_TO_RAPPOR |
        SwReporterInvocation::BEHAVIOUR_LOG_EXIT_CODE_TO_PREFS |
        SwReporterInvocation::BEHAVIOUR_TRIGGER_PROMPT |
        SwReporterInvocation::BEHAVIOUR_ALLOW_SEND_REPORTER_LOGS;

    safe_browsing::SwReporterQueue invocations;
    invocations.push(invocation);
    reporter_runner_.Run(invocations, version);
  }
}

base::FilePath SwReporterInstallerTraits::GetRelativeInstallDir() const {
  return base::FilePath(FILE_PATH_LITERAL("SwReporter"));
}

void SwReporterInstallerTraits::GetHash(std::vector<uint8_t>* hash) const {
  DCHECK(hash);
  hash->assign(kSha256Hash, kSha256Hash + sizeof(kSha256Hash));
}

std::string SwReporterInstallerTraits::GetName() const {
  return "Software Reporter Tool";
}

update_client::InstallerAttributes
SwReporterInstallerTraits::GetInstallerAttributes() const {
  update_client::InstallerAttributes attributes;
  if (IsExperimentalEngineEnabled()) {
    // Pass the "tag" parameter to the installer; it will be used to choose
    // which binary is downloaded.
    constexpr char kTagParam[] = "tag";
    const std::string tag = variations::GetVariationParamValueByFeature(
        kExperimentalEngineFeature, kTagParam);

    // If the tag is not a valid attribute (see the regexp in
    // ComponentInstallerTraits::InstallerAttributes), set it to a valid but
    // unrecognized value so that nothing will be downloaded.
    constexpr size_t kMaxAttributeLength = 256;
    constexpr char kExtraAttributeChars[] = "-.,;+_=";
    if (tag.empty() ||
        !ValidateString(tag, kExtraAttributeChars, kMaxAttributeLength)) {
      ReportExperimentError(SW_REPORTER_EXPERIMENT_ERROR_BAD_TAG);
      attributes[kTagParam] = "missing_tag";
    } else {
      attributes[kTagParam] = tag;
    }
  }
  return attributes;
}

std::vector<std::string> SwReporterInstallerTraits::GetMimeTypes() const {
  return std::vector<std::string>();
}

bool SwReporterInstallerTraits::IsExperimentalEngineEnabled() const {
  return is_experimental_engine_supported_ &&
         base::FeatureList::IsEnabled(kExperimentalEngineFeature);
}

void RegisterSwReporterComponent(ComponentUpdateService* cus) {
  if (!safe_browsing::IsSwReporterEnabled())
    return;

  // Check if we have information from Cleaner and record UMA statistics.
  base::string16 cleaner_key_name(
      safe_browsing::kSoftwareRemovalToolRegistryKey);
  cleaner_key_name.append(1, L'\\').append(kCleanerSuffixRegistryKey);
  base::win::RegKey cleaner_key(
      HKEY_CURRENT_USER, cleaner_key_name.c_str(), KEY_ALL_ACCESS);
  // Cleaner is assumed to have run if we have a start time.
  if (cleaner_key.Valid()) {
    if (cleaner_key.HasValue(safe_browsing::kStartTimeValueName)) {
      // Get version number.
      if (cleaner_key.HasValue(kVersionValueName)) {
        DWORD version;
        cleaner_key.ReadValueDW(kVersionValueName, &version);
        UMA_HISTOGRAM_SPARSE_SLOWLY("SoftwareReporter.Cleaner.Version",
                                    version);
        cleaner_key.DeleteValue(kVersionValueName);
      }
      // Get start & end time. If we don't have an end time, we can assume the
      // cleaner has not completed.
      int64_t start_time_value;
      cleaner_key.ReadInt64(safe_browsing::kStartTimeValueName,
                            &start_time_value);

      bool completed = cleaner_key.HasValue(safe_browsing::kEndTimeValueName);
      SRTHasCompleted(completed ? SRT_COMPLETED_YES : SRT_COMPLETED_NOT_YET);
      if (completed) {
        int64_t end_time_value;
        cleaner_key.ReadInt64(safe_browsing::kEndTimeValueName,
                              &end_time_value);
        cleaner_key.DeleteValue(safe_browsing::kEndTimeValueName);
        base::TimeDelta run_time(
            base::Time::FromInternalValue(end_time_value) -
            base::Time::FromInternalValue(start_time_value));
        UMA_HISTOGRAM_LONG_TIMES("SoftwareReporter.Cleaner.RunningTime",
                                 run_time);
      }
      // Get exit code. Assume nothing was found if we can't read the exit code.
      DWORD exit_code = safe_browsing::kSwReporterNothingFound;
      if (cleaner_key.HasValue(kExitCodeValueName)) {
        cleaner_key.ReadValueDW(kExitCodeValueName, &exit_code);
        UMA_HISTOGRAM_SPARSE_SLOWLY("SoftwareReporter.Cleaner.ExitCode",
                                    exit_code);
        cleaner_key.DeleteValue(kExitCodeValueName);
      }
      cleaner_key.DeleteValue(safe_browsing::kStartTimeValueName);

      if (exit_code == safe_browsing::kSwReporterPostRebootCleanupNeeded ||
          exit_code ==
              safe_browsing::kSwReporterDelayedPostRebootCleanupNeeded) {
        // Check if we are running after the user has rebooted.
        base::TimeDelta elapsed(
            base::Time::Now() -
            base::Time::FromInternalValue(start_time_value));
        DCHECK_GT(elapsed.InMilliseconds(), 0);
        UMA_HISTOGRAM_BOOLEAN(
            "SoftwareReporter.Cleaner.HasRebooted",
            static_cast<uint64_t>(elapsed.InMilliseconds()) > ::GetTickCount());
      }

      if (cleaner_key.HasValue(kUploadResultsValueName)) {
        base::string16 upload_results;
        cleaner_key.ReadValue(kUploadResultsValueName, &upload_results);
        ReportUploadsWithUma(upload_results);
      }
    } else {
      if (cleaner_key.HasValue(safe_browsing::kEndTimeValueName)) {
        SRTHasCompleted(SRT_COMPLETED_LATER);
        cleaner_key.DeleteValue(safe_browsing::kEndTimeValueName);
      }
    }
  }

  // If the experiment is not explicitly enabled on all platforms, it
  // should be only enabled on x86. There's no way to check this in the
  // variations config so we'll hard-code it.
  const bool is_x86_architecture =
      base::win::OSInfo::GetInstance()->architecture() ==
      base::win::OSInfo::X86_ARCHITECTURE;
  const bool is_experimental_engine_supported =
      base::FeatureList::IsEnabled(kExperimentalEngineAllArchsFeature) ||
      is_x86_architecture;

  // Install the component.
  std::unique_ptr<ComponentInstallerTraits> traits(
      new SwReporterInstallerTraits(base::Bind(&RunSwReportersAfterStartup),
                                    is_experimental_engine_supported));
  // |cus| will take ownership of |installer| during installer->Register(cus).
  DefaultComponentInstaller* installer =
      new DefaultComponentInstaller(std::move(traits));
  installer->Register(cus, base::Closure());
}

void RegisterPrefsForSwReporter(PrefRegistrySimple* registry) {
  registry->RegisterInt64Pref(prefs::kSwReporterLastTimeTriggered, 0);
  registry->RegisterIntegerPref(prefs::kSwReporterLastExitCode, -1);
  registry->RegisterBooleanPref(prefs::kSwReporterPendingPrompt, false);
  registry->RegisterInt64Pref(prefs::kSwReporterLastTimeSentReport, 0);
}

void RegisterProfilePrefsForSwReporter(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(prefs::kSwReporterPromptVersion, "");

  registry->RegisterStringPref(prefs::kSwReporterPromptSeed, "");
}

}  // namespace component_updater
