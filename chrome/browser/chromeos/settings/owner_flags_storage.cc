// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/owner_flags_storage.h"

#include "base/command_line.h"
#include "base/values.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/flags_ui/flags_ui_pref_names.h"
#include "components/ownership/owner_settings_service.h"
#include "components/prefs/pref_service.h"

namespace chromeos {
namespace about_flags {

OwnerFlagsStorage::OwnerFlagsStorage(
    PrefService* prefs,
    ownership::OwnerSettingsService* owner_settings_service)
    : flags_ui::PrefServiceFlagsStorage(prefs),
      owner_settings_service_(owner_settings_service) {
  // Make this code more unit test friendly.
  if (g_browser_process->local_state()) {
    const base::ListValue* legacy_experiments =
        g_browser_process->local_state()->GetList(
            flags_ui::prefs::kEnabledLabsExperiments);
    if (!legacy_experiments->empty()) {
      // If there are any flags set in local state migrate them to the owner's
      // prefs and device settings.
      std::set<std::string> flags;
      for (base::ListValue::const_iterator it = legacy_experiments->begin();
           it != legacy_experiments->end(); ++it) {
        std::string experiment_name;
        if (!(*it)->GetAsString(&experiment_name)) {
          LOG(WARNING) << "Invalid entry in "
                       << flags_ui::prefs::kEnabledLabsExperiments;
          continue;
        }
        flags.insert(experiment_name);
      }
      SetFlags(flags);
      g_browser_process->local_state()->ClearPref(
          flags_ui::prefs::kEnabledLabsExperiments);
    }
  }
}

OwnerFlagsStorage::~OwnerFlagsStorage() {}

bool OwnerFlagsStorage::SetFlags(const std::set<std::string>& flags) {
  PrefServiceFlagsStorage::SetFlags(flags);

  base::ListValue experiments_list;

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  ::about_flags::ConvertFlagsToSwitches(this, &command_line,
                                        flags_ui::kNoSentinels);
  base::CommandLine::StringVector switches = command_line.argv();
  for (base::CommandLine::StringVector::const_iterator it =
           switches.begin() + 1;
       it != switches.end(); ++it) {
    experiments_list.Append(new base::StringValue(*it));
  }
  owner_settings_service_->Set(kStartUpFlags, experiments_list);

  return true;
}

}  // namespace about_flags
}  // namespace chromeos
