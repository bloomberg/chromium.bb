// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/owner_flags_storage.h"

#include "base/prefs/pref_service.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/cros_settings_names.h"
#include "chrome/common/pref_names.h"

namespace chromeos {
namespace about_flags {

OwnerFlagsStorage::OwnerFlagsStorage(PrefService *prefs,
                                     CrosSettings *cros_settings)
    : ::about_flags::PrefServiceFlagsStorage(prefs),
      cros_settings_(cros_settings) {
  // Make this code more unit test friendly.
  if (g_browser_process->local_state()) {
    const ListValue* legacy_experiments =
        g_browser_process->local_state()->GetList(
            prefs::kEnabledLabsExperiments);
    if (!legacy_experiments->empty()) {
      // If there are any flags set in local state migrate them to the owner's
      // prefs and device settings.
      std::set<std::string> flags;
      for (ListValue::const_iterator it = legacy_experiments->begin();
           it != legacy_experiments->end(); ++it) {
        std::string experiment_name;
        if (!(*it)->GetAsString(&experiment_name)) {
          LOG(WARNING) << "Invalid entry in " << prefs::kEnabledLabsExperiments;
          continue;
        }
        flags.insert(experiment_name);
      }
      SetFlags(flags);
      g_browser_process->local_state()->ClearPref(
          prefs::kEnabledLabsExperiments);
    }
  }
}

OwnerFlagsStorage::~OwnerFlagsStorage() {}

bool OwnerFlagsStorage::SetFlags(std::set<std::string> flags) {
  PrefServiceFlagsStorage::SetFlags(flags);

  base::ListValue experiments_list;

  for (std::set<std::string>::const_iterator it = flags.begin();
       it != flags.end(); ++it) {
    experiments_list.Append(new base::StringValue(*it));
  }
  cros_settings_->Set(kStartUpFlags, experiments_list);

  return true;
}

}  // namespace about_flags
}  // namespace chromeos
