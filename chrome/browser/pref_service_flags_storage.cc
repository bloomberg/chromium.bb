// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pref_service_flags_storage.h"

#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"

namespace about_flags {

PrefServiceFlagsStorage::PrefServiceFlagsStorage(
    PrefService *prefs) : prefs_(prefs) {}

PrefServiceFlagsStorage::~PrefServiceFlagsStorage() {}

std::set<std::string> PrefServiceFlagsStorage::GetFlags() {
  const base::ListValue* enabled_experiments = prefs_->GetList(
      prefs::kEnabledLabsExperiments);
  std::set<std::string> flags;
  for (base::ListValue::const_iterator it = enabled_experiments->begin();
       it != enabled_experiments->end();
       ++it) {
    std::string experiment_name;
    if (!(*it)->GetAsString(&experiment_name)) {
      LOG(WARNING) << "Invalid entry in " << prefs::kEnabledLabsExperiments;
      continue;
    }
    flags.insert(experiment_name);
  }
  return flags;
}

bool PrefServiceFlagsStorage::SetFlags(const std::set<std::string>& flags) {
  ListPrefUpdate update(prefs_, prefs::kEnabledLabsExperiments);
  base::ListValue* experiments_list = update.Get();

  experiments_list->Clear();
  for (std::set<std::string>::const_iterator it = flags.begin();
       it != flags.end(); ++it) {
    experiments_list->Append(new base::StringValue(*it));
  }

  return true;
}

}  // namespace about_flags
