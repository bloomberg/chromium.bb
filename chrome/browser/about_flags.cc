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

enum { kOsMac = 1 << 0, kOsWin = 1 << 1, kOsLinux = 1 << 2 , kOsCrOS = 1 << 3 };

unsigned kOsAll = kOsMac | kOsWin | kOsLinux | kOsCrOS;

struct Experiment {
  // The internal name of the experiment. This is never shown to the user.
  // It _is_ however stored in the prefs file, so you shouldn't change the
  // name of existing flags.
  const char* internal_name;

  // String id of the message containing the experiment's name.
  int visible_name_id;

  // String id of the message containing the experiment's description.
  int visible_description_id;

  // The platforms the experiment is available on
  // Needs to be more than a compile-time #ifdef because of profile sync.
  unsigned supported_platforms;  // bitmask

  // The commandline parameter that's added when this lab is active. This is
  // different from |internal_name| so that the commandline flag can be
  // renamed without breaking the prefs file.
  const char* command_line;
};

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
    "vertical-tabs",  // Do not change; see above.
    IDS_FLAGS_SIDE_TABS_NAME,
    IDS_FLAGS_SIDE_TABS_DESCRIPTION,
    // TODO(thakis): Move sidetabs to about:flags on ChromeOS
    // http://crbug.com/57634
    kOsWin,
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
    "cloud-print-proxy", // Do not change; see above.
    IDS_FLAGS_CLOUD_PRINT_PROXY_NAME,
    IDS_FLAGS_CLOUD_PRINT_PROXY_DESCRIPTION,
    kOsWin,
    switches::kEnableCloudPrintProxy
  },
  {
    "match-preview",  // Do not change; see above.
    IDS_FLAGS_INSTANT_NAME,
    IDS_FLAGS_INSTANT_DESCRIPTION,
    kOsMac,
    switches::kEnableMatchPreview
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
    "dns-server",
    IDS_FLAGS_DNS_SERVER_NAME,
    IDS_FLAGS_DNS_SERVER_DESCRIPTION,
    kOsLinux,
    switches::kDnsServer
  },
  {
    "secure-infobars",  // Do not change; see above
    IDS_FLAGS_SECURE_INFOBARS_NAME,
    IDS_FLAGS_SECURE_INFOBARS_DESCRIPTION,
    kOsAll,
    switches::kEnableSecureInfoBars
  }
};

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
  for (size_t i = 0; i < arraysize(kExperiments); ++i)
    known_experiments.insert(kExperiments[i].internal_name);

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

}  // namespace

void ConvertFlagsToSwitches(PrefService* prefs, CommandLine* command_line) {
  FlagsState::instance()->ConvertFlagsToSwitches(prefs, command_line);
}

ListValue* GetFlagsExperimentsData(PrefService* prefs) {
  std::set<std::string> enabled_experiments;
  GetSanitizedEnabledFlags(prefs, &enabled_experiments);

  int current_platform = GetCurrentPlatform();

  ListValue* experiments_data = new ListValue();
  for (size_t i = 0; i < arraysize(kExperiments); ++i) {
    const Experiment& experiment = kExperiments[i];
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

//////////////////////////////////////////////////////////////////////////////
// FlagsState implementation.

namespace {

void FlagsState::ConvertFlagsToSwitches(
    PrefService* prefs, CommandLine* command_line) {
  if (command_line->HasSwitch(switches::kNoExperiments))
    return;

  std::set<std::string> enabled_experiments;
  GetSanitizedEnabledFlags(prefs, &enabled_experiments);

  std::map<std::string, const Experiment*> experiments;
  for (size_t i = 0; i < arraysize(kExperiments); ++i)
    experiments[kExperiments[i].internal_name] = &kExperiments[i];

  command_line->AppendSwitch(switches::kFlagSwitchesBegin);
  flags_switches_.insert(switches::kFlagSwitchesBegin);
  for (std::set<std::string>::iterator it = enabled_experiments.begin();
       it != enabled_experiments.end();
       ++it) {
    const std::string& experiment_name = *it;
    std::map<std::string, const Experiment*>::iterator experiment =
        experiments.find(experiment_name);
    DCHECK(experiment != experiments.end());
    if (experiment == experiments.end())
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
}  // namespace testing

}  // namespace about_flags
