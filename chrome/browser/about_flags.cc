// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/about_flags.h"

#include <algorithm>
#include <iterator>
#include <map>
#include <set>

#include "app/l10n_util.h"
#include "base/command_line.h"
#include "base/singleton.h"
#include "base/values.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"

namespace about_flags {

namespace {

const unsigned kOsAll = kOsMac | kOsWin | kOsLinux | kOsCrOS;

// Names for former Chrome OS Labs experiments, shared with prefs migration
// code.
const char kMediaPlayerExperimentName[] = "media-player";
const char kAdvancedFileSystemExperimentName[] = "advanced-file-system";
const char kVerticalTabsExperimentName[] = "vertical-tabs";

const Experiment kExperiments[] = {
  {
    "expose-for-tabs",  // Do not change; see above.
    IDS_FLAGS_TABPOSE_NAME,
    IDS_FLAGS_TABPOSE_DESCRIPTION,
    kOsMac,
#if defined(OS_MACOSX)
    // The switch exists only on OS X.
    switches::kEnableExposeForTabs
#else
    ""
#endif
  },
  {
    kMediaPlayerExperimentName,
    IDS_FLAGS_MEDIA_PLAYER_NAME,
    IDS_FLAGS_MEDIA_PLAYER_DESCRIPTION,
    kOsCrOS,
#if defined(OS_CHROMEOS)
    // The switch exists only on Chrome OS.
    switches::kEnableMediaPlayer
#else
    ""
#endif
  },
  {
    kAdvancedFileSystemExperimentName,
    IDS_FLAGS_ADVANCED_FS_NAME,
    IDS_FLAGS_ADVANCED_FS_DESCRIPTION,
    kOsCrOS,
#if defined(OS_CHROMEOS)
    // The switch exists only on Chrome OS.
    switches::kEnableAdvancedFileSystem
#else
    ""
#endif
  },
  {
    kVerticalTabsExperimentName,
    IDS_FLAGS_SIDE_TABS_NAME,
    IDS_FLAGS_SIDE_TABS_DESCRIPTION,
    kOsWin | kOsCrOS,
    switches::kEnableVerticalTabs
  },
  {
    "tabbed-options",  // Do not change; see above.
    IDS_FLAGS_TABBED_OPTIONS_NAME,
    IDS_FLAGS_TABBED_OPTIONS_DESCRIPTION,
    kOsWin | kOsLinux | kOsMac,  // Enabled by default on CrOS.
    switches::kEnableTabbedOptions
  },
  {
    "remoting",  // Do not change; see above.
    IDS_FLAGS_REMOTING_NAME,
#if defined(OS_WIN)
    // Windows only supports host functionality at the moment.
    IDS_FLAGS_REMOTING_HOST_DESCRIPTION,
#elif defined(OS_LINUX)  // Also true for CrOS.
    // Linux only supports client functionality at the moment.
    IDS_FLAGS_REMOTING_CLIENT_DESCRIPTION,
#else
    // On other platforms, this lab isn't available at all.
    0,
#endif
    kOsWin | kOsLinux | kOsCrOS,
    switches::kEnableRemoting
  },
  {
    "disable-outdated-plugins",  // Do not change; see above.
    IDS_FLAGS_DISABLE_OUTDATED_PLUGINS_NAME,
    IDS_FLAGS_DISABLE_OUTDATED_PLUGINS_DESCRIPTION,
    kOsAll,
    switches::kDisableOutdatedPlugins
  },
  {
    "xss-auditor",  // Do not change; see above.
    IDS_FLAGS_XSS_AUDITOR_NAME,
    IDS_FLAGS_XSS_AUDITOR_DESCRIPTION,
    kOsAll,
    switches::kEnableXSSAuditor
  },
  {
    "background-webapps", // Do not change; see above
    IDS_FLAGS_BACKGROUND_WEBAPPS_NAME,
    IDS_FLAGS_BACKGROUND_WEBAPPS_DESCRIPTION,
    kOsAll,
    switches::kEnableBackgroundMode
  },
  {
    "conflicting-modules-check", // Do not change; see above.
    IDS_FLAGS_CONFLICTS_CHECK_NAME,
    IDS_FLAGS_CONFLICTS_CHECK_DESCRIPTION,
    kOsWin,
    switches::kConflictingModulesCheck
  },
  {
    "cloud-print-proxy", // Do not change; see above.
    IDS_FLAGS_CLOUD_PRINT_PROXY_NAME,
    IDS_FLAGS_CLOUD_PRINT_PROXY_DESCRIPTION,
    kOsWin,
    switches::kEnableCloudPrintProxy
  },
  {
    "match-preview",  // Do not change; see above.
    IDS_FLAGS_PREDICTIVE_INSTANT_NAME,
    IDS_FLAGS_PREDICTIVE_INSTANT_DESCRIPTION,
    kOsMac,
    switches::kEnablePredictiveInstant
  },
  // FIXME(scheib): Add Flags entry for accelerated Compositing,
  // or pull it and the strings in generated_resources.grd by Dec 2010
  // {
  //   "gpu-compositing", // Do not change; see above
  //   IDS_FLAGS_ACCELERATED_COMPOSITING_NAME,
  //   IDS_FLAGS_ACCELERATED_COMPOSITING_DESCRIPTION,
  //   kOsAll,
  //   switches::kDisableAcceleratedCompositing
  // },
  {
    "gpu-canvas-2d", // Do not change; see above
    IDS_FLAGS_ACCELERATED_CANVAS_2D_NAME,
    IDS_FLAGS_ACCELERATED_CANVAS_2D_DESCRIPTION,
    kOsWin | kOsLinux | kOsCrOS,
    switches::kEnableAccelerated2dCanvas
  },
  // FIXME(scheib): Add Flags entry for WebGL,
  // or pull it and the strings in generated_resources.grd by Dec 2010
  // {
  //   "webgl", // Do not change; see above
  //   IDS_FLAGS_WEBGL_NAME,
  //   IDS_FLAGS_WEBGL_DESCRIPTION,
  //   kOsAll,
  //   switches::kDisableExperimentalWebGL
  // }
  {
    "print-preview",  // Do not change; see above
    IDS_FLAGS_PRINT_PREVIEW_NAME,
    IDS_FLAGS_PRINT_PREVIEW_DESCRIPTION,
    kOsAll,
    switches::kEnablePrintPreview
  },
  {
    "enable-nacl",   // Do not change; see above.
    IDS_FLAGS_ENABLE_NACL_NAME,
    IDS_FLAGS_ENABLE_NACL_DESCRIPTION,
    kOsAll,
    switches::kEnableNaCl
  },
  {
    "dns-server",
    IDS_FLAGS_DNS_SERVER_NAME,
    IDS_FLAGS_DNS_SERVER_DESCRIPTION,
    kOsLinux,
    switches::kDnsServer
  },
  {
    "page-prerender",
    IDS_FLAGS_PAGE_PRERENDER_NAME,
    IDS_FLAGS_PAGE_PRERENDER_DESCRIPTION,
    kOsAll,
    switches::kEnablePagePrerender
  },
  {
    "confirm-to-quit",   // Do not change; see above.
    IDS_FLAGS_CONFIRM_TO_QUIT_NAME,
    IDS_FLAGS_CONFIRM_TO_QUIT_DESCRIPTION,
    kOsMac,
    switches::kEnableConfirmToQuit
  },
  {
    "snap-start",   // Do not change; see above.
    IDS_FLAGS_SNAP_START_NAME,
    IDS_FLAGS_SNAP_START_DESCRIPTION,
    kOsAll,
    switches::kEnableSnapStart
  },
  {
    "extension-apis",   // Do not change; see above.
    IDS_FLAGS_EXPERIMENTAL_EXTENSION_APIS_NAME,
    IDS_FLAGS_EXPERIMENTAL_EXTENSION_APIS_DESCRIPTION,
    kOsAll,
    switches::kEnableExperimentalExtensionApis
  },
  {
    "click-to-play",   // Do not change; see above.
    IDS_FLAGS_CLICK_TO_PLAY_NAME,
    IDS_FLAGS_CLICK_TO_PLAY_DESCRIPTION,
    kOsAll,
    switches::kEnableClickToPlay
  },
};

const Experiment* experiments = kExperiments;
size_t num_experiments = arraysize(kExperiments);

// Stores and encapsulates the little state that about:flags has.
class FlagsState {
 public:
  FlagsState() : needs_restart_(false) {}
  void ConvertFlagsToSwitches(PrefService* prefs, CommandLine* command_line);
  bool IsRestartNeededToCommitChanges();
  void SetExperimentEnabled(
      PrefService* prefs, const std::string& internal_name, bool enable);
  void RemoveFlagsSwitches(
      std::map<std::string, CommandLine::StringType>* switch_list);
  void reset();

  // Returns the singleton instance of this class
  static FlagsState* instance() {
    return Singleton<FlagsState>::get();
  }

 private:
  bool needs_restart_;
  std::set<std::string> flags_switches_;

  DISALLOW_COPY_AND_ASSIGN(FlagsState);
};

#if defined(OS_CHROMEOS)
// Migrates Chrome OS Labs settings to experiments adding flags to enabled
// experiment list if the corresponding pref is on.
void MigrateChromeOSLabsPrefs(PrefService* prefs,
                              std::set<std::string>* result) {
  DCHECK(prefs);
  DCHECK(result);
  if (prefs->GetBoolean(prefs::kLabsMediaplayerEnabled))
    result->insert(kMediaPlayerExperimentName);
  if (prefs->GetBoolean(prefs::kLabsAdvancedFilesystemEnabled))
    result->insert(kAdvancedFileSystemExperimentName);
  if (prefs->GetBoolean(prefs::kUseVerticalTabs))
    result->insert(kVerticalTabsExperimentName);
  prefs->SetBoolean(prefs::kLabsMediaplayerEnabled, false);
  prefs->SetBoolean(prefs::kLabsAdvancedFilesystemEnabled, false);
  prefs->SetBoolean(prefs::kUseVerticalTabs, false);
}
#endif

// Extracts the list of enabled lab experiments from preferences and stores them
// in a set.
void GetEnabledFlags(const PrefService* prefs, std::set<std::string>* result) {
  const ListValue* enabled_experiments = prefs->GetList(
      prefs::kEnabledLabsExperiments);
  if (!enabled_experiments)
    return;

  for (ListValue::const_iterator it = enabled_experiments->begin();
       it != enabled_experiments->end();
       ++it) {
    std::string experiment_name;
    if (!(*it)->GetAsString(&experiment_name)) {
      LOG(WARNING) << "Invalid entry in " << prefs::kEnabledLabsExperiments;
      continue;
    }
    result->insert(experiment_name);
  }
}

// Takes a set of enabled lab experiments
void SetEnabledFlags(
    PrefService* prefs, const std::set<std::string>& enabled_experiments) {
  ListValue* experiments_list = prefs->GetMutableList(
      prefs::kEnabledLabsExperiments);
  if (!experiments_list)
    return;

  experiments_list->Clear();
  for (std::set<std::string>::const_iterator it = enabled_experiments.begin();
       it != enabled_experiments.end();
       ++it) {
    experiments_list->Append(new StringValue(*it));
  }
}

// Removes all experiments from prefs::kEnabledLabsExperiments that are
// unknown, to prevent this list to become very long as experiments are added
// and removed.
void SanitizeList(PrefService* prefs) {
  std::set<std::string> known_experiments;
  for (size_t i = 0; i < num_experiments; ++i)
    known_experiments.insert(experiments[i].internal_name);

  std::set<std::string> enabled_experiments;
  GetEnabledFlags(prefs, &enabled_experiments);

  std::set<std::string> new_enabled_experiments;
  std::set_intersection(
      known_experiments.begin(), known_experiments.end(),
      enabled_experiments.begin(), enabled_experiments.end(),
      std::inserter(new_enabled_experiments, new_enabled_experiments.begin()));

  SetEnabledFlags(prefs, new_enabled_experiments);
}

void GetSanitizedEnabledFlags(
    PrefService* prefs, std::set<std::string>* result) {
  SanitizeList(prefs);
  GetEnabledFlags(prefs, result);
}

// Variant of GetSanitizedEnabledFlags that also removes any flags that aren't
// enabled on the current platform.
void GetSanitizedEnabledFlagsForCurrentPlatform(
    PrefService* prefs, std::set<std::string>* result) {
  GetSanitizedEnabledFlags(prefs, result);

  // Filter out any experiments that aren't enabled on the current platform.  We
  // don't remove these from prefs else syncing to a platform with a different
  // set of experiments would be lossy.
  std::set<std::string> platform_experiments;
  int current_platform = GetCurrentPlatform();
  for (size_t i = 0; i < num_experiments; ++i) {
    if (experiments[i].supported_platforms & current_platform)
      platform_experiments.insert(experiments[i].internal_name);
  }

  std::set<std::string> new_enabled_experiments;
  std::set_intersection(
      platform_experiments.begin(), platform_experiments.end(),
      result->begin(), result->end(),
      std::inserter(new_enabled_experiments, new_enabled_experiments.begin()));

  result->swap(new_enabled_experiments);
}

}  // namespace

void ConvertFlagsToSwitches(PrefService* prefs, CommandLine* command_line) {
  FlagsState::instance()->ConvertFlagsToSwitches(prefs, command_line);
}

ListValue* GetFlagsExperimentsData(PrefService* prefs) {
  std::set<std::string> enabled_experiments;
  GetSanitizedEnabledFlags(prefs, &enabled_experiments);

  int current_platform = GetCurrentPlatform();

  ListValue* experiments_data = new ListValue();
  for (size_t i = 0; i < num_experiments; ++i) {
    const Experiment& experiment = experiments[i];
    if (!(experiment.supported_platforms & current_platform))
      continue;

    DictionaryValue* data = new DictionaryValue();
    data->SetString("internal_name", experiment.internal_name);
    data->SetString("name",
                    l10n_util::GetStringUTF16(experiment.visible_name_id));
    data->SetString("description",
                    l10n_util::GetStringUTF16(
                        experiment.visible_description_id));
    data->SetBoolean("enabled",
                      enabled_experiments.count(experiment.internal_name) > 0);

    experiments_data->Append(data);
  }
  return experiments_data;
}

bool IsRestartNeededToCommitChanges() {
  return FlagsState::instance()->IsRestartNeededToCommitChanges();
}

void SetExperimentEnabled(
    PrefService* prefs, const std::string& internal_name, bool enable) {
  FlagsState::instance()->SetExperimentEnabled(prefs, internal_name, enable);
}

void RemoveFlagsSwitches(
    std::map<std::string, CommandLine::StringType>* switch_list) {
  FlagsState::instance()->RemoveFlagsSwitches(switch_list);
}

int GetCurrentPlatform() {
#if defined(OS_MACOSX)
  return kOsMac;
#elif defined(OS_WIN)
  return kOsWin;
#elif defined(OS_CHROMEOS)  // Needs to be before the OS_LINUX check.
  return kOsCrOS;
#elif defined(OS_LINUX)
  return kOsLinux;
#else
#error Unknown platform
#endif
}

//////////////////////////////////////////////////////////////////////////////
// FlagsState implementation.

namespace {

void FlagsState::ConvertFlagsToSwitches(
    PrefService* prefs, CommandLine* command_line) {
  if (command_line->HasSwitch(switches::kNoExperiments))
    return;

  std::set<std::string> enabled_experiments;

#if defined(OS_CHROMEOS)
  // Some experiments were implemented via prefs on Chrome OS and we want to
  // seamlessly migrate these prefs to about:flags for updated users.
  MigrateChromeOSLabsPrefs(prefs, &enabled_experiments);
#endif

  GetSanitizedEnabledFlagsForCurrentPlatform(prefs, &enabled_experiments);

  std::map<std::string, const Experiment*> experiment_map;
  for (size_t i = 0; i < num_experiments; ++i)
    experiment_map[experiments[i].internal_name] = &experiments[i];

  command_line->AppendSwitch(switches::kFlagSwitchesBegin);
  flags_switches_.insert(switches::kFlagSwitchesBegin);
  for (std::set<std::string>::iterator it = enabled_experiments.begin();
       it != enabled_experiments.end();
       ++it) {
    const std::string& experiment_name = *it;
    std::map<std::string, const Experiment*>::iterator experiment =
        experiment_map.find(experiment_name);
    DCHECK(experiment != experiment_map.end());
    if (experiment == experiment_map.end())
      continue;

    command_line->AppendSwitch(experiment->second->command_line);
    flags_switches_.insert(experiment->second->command_line);
  }
  command_line->AppendSwitch(switches::kFlagSwitchesEnd);
  flags_switches_.insert(switches::kFlagSwitchesEnd);
}

bool FlagsState::IsRestartNeededToCommitChanges() {
  return needs_restart_;
}

void FlagsState::SetExperimentEnabled(
    PrefService* prefs, const std::string& internal_name, bool enable) {
  needs_restart_ = true;

  std::set<std::string> enabled_experiments;
  GetSanitizedEnabledFlags(prefs, &enabled_experiments);

  if (enable)
    enabled_experiments.insert(internal_name);
  else
    enabled_experiments.erase(internal_name);

  SetEnabledFlags(prefs, enabled_experiments);
}

void FlagsState::RemoveFlagsSwitches(
    std::map<std::string, CommandLine::StringType>* switch_list) {
  for (std::set<std::string>::const_iterator it = flags_switches_.begin();
       it != flags_switches_.end();
       ++it) {
    switch_list->erase(*it);
  }
}

void FlagsState::reset() {
  needs_restart_ = false;
  flags_switches_.clear();
}

} // namespace

namespace testing {
void ClearState() {
  FlagsState::instance()->reset();
}

void SetExperiments(const Experiment* e, size_t count) {
  if (!e) {
    experiments = kExperiments;
    num_experiments = arraysize(kExperiments);
  } else {
    experiments = e;
    num_experiments = count;
  }
}

}  // namespace testing

}  // namespace about_flags
