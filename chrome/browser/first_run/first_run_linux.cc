// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run/first_run.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/strings/string_piece.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/first_run/first_run_internal.h"
#include "chrome/browser/importer/importer_host.h"
#include "chrome/browser/process_singleton.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/master_preferences.h"
#include "content/public/common/result_codes.h"
#include "googleurl/src/gurl.h"
#include "ui/base/ui_base_switches.h"

namespace first_run {
namespace internal {

bool IsOrganicFirstRun() {
  // We treat all installs as organic.
  return true;
}

// TODO(port): This is just a piece of the silent import functionality from
// ImportSettings for Windows.  It would be nice to get the rest of it ported.
bool ImportBookmarks(const base::FilePath& import_bookmarks_path) {
  const CommandLine& cmdline = *CommandLine::ForCurrentProcess();
  CommandLine import_cmd(cmdline.GetProgram());

  // Propagate user data directory switch.
  if (cmdline.HasSwitch(switches::kUserDataDir)) {
    import_cmd.AppendSwitchPath(switches::kUserDataDir,
        cmdline.GetSwitchValuePath(switches::kUserDataDir));
  }
  // Since ImportSettings is called before the local state is stored on disk
  // we pass the language as an argument. GetApplicationLocale checks the
  // current command line as fallback.
  import_cmd.AppendSwitchASCII(switches::kLang,
                               g_browser_process->GetApplicationLocale());

  import_cmd.CommandLine::AppendSwitchPath(switches::kImportFromFile,
                                           import_bookmarks_path);

  // The importer doesn't need to do any background networking tasks so disable
  // them.
  import_cmd.CommandLine::AppendSwitch(switches::kDisableBackgroundNetworking);

  // Time to launch the process that is going to do the import. We'll wait
  // for the process to return.
  base::LaunchOptions options;
  options.wait = true;
  return base::LaunchProcess(import_cmd, options, NULL);
}

}  // namespace internal
}  // namespace first_run

namespace first_run {

base::FilePath MasterPrefsPath() {
  // The standard location of the master prefs is next to the chrome binary.
  base::FilePath master_prefs;
  if (!PathService::Get(base::DIR_EXE, &master_prefs))
    return base::FilePath();
  return master_prefs.AppendASCII(installer::kDefaultMasterPrefs);
}

}  // namespace first_run
