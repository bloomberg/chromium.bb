// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_util.h"

#include "base/file_util.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"

namespace chrome {

#if defined(OS_CHROMEOS)
// Default implementation for ports which do not have this implemented.
void InitAppList(Profile* profile) {}
#endif

#if defined(ENABLE_APP_LIST)
base::FilePath GetAppListProfilePath(const base::FilePath& user_data_dir) {
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);

  std::string app_list_profile;
  if (local_state->HasPrefPath(prefs::kAppListProfile))
    app_list_profile = local_state->GetString(prefs::kAppListProfile);

  // If the user has no profile preference for the app launcher, default to the
  // last browser profile used.
  if (app_list_profile.empty() &&
      local_state->HasPrefPath(prefs::kProfileLastUsed))
    app_list_profile = local_state->GetString(prefs::kProfileLastUsed);

  std::string profile_path = app_list_profile.empty() ?
      chrome::kInitialProfile :
      app_list_profile;

  return user_data_dir.AppendASCII(profile_path);
}

void RegisterAppListPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kAppListProfile, "");
}
#endif  // defined(ENABLE_APP_LIST)


}  // namespace chrome
