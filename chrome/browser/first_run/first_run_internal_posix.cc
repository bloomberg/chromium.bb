// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run/first_run_internal.h"

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/first_run/first_run_dialog.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/master_preferences.h"
#include "components/startup_metric_utils/startup_metric_utils.h"

namespace first_run {
namespace internal {

void DoPostImportPlatformSpecificTasks(Profile* profile) {
#if !defined(OS_CHROMEOS)
  base::FilePath local_state_path;
  PathService::Get(chrome::FILE_LOCAL_STATE, &local_state_path);
  bool local_state_file_exists = base::PathExists(local_state_path);
  // Launch the first run dialog only for certain builds, and only if the user
  // has not already set preferences.
  if (internal::IsOrganicFirstRun() && !local_state_file_exists) {
    if (ShowFirstRunDialog(profile))
      startup_metric_utils::SetNonBrowserUIDisplayed();
  }

  // If stats reporting was turned on by the first run dialog then toggle
  // the pref (on Windows, the download is tagged with enable/disable stats so
  // this is POSIX-specific).
  if (GoogleUpdateSettings::GetCollectStatsConsent()) {
    g_browser_process->local_state()->SetBoolean(
        prefs::kMetricsReportingEnabled, true);
  }
#endif
}

bool IsFirstRunSentinelPresent() {
  base::FilePath sentinel;
  return !GetFirstRunSentinelFilePath(&sentinel) || base::PathExists(sentinel);
}

bool ShowPostInstallEULAIfNeeded(installer::MasterPreferences* install_prefs) {
  // The EULA is only handled on Windows.
  return true;
}

}  // namespace internal
}  // namespace first_run
