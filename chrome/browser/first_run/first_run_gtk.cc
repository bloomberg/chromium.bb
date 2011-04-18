// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run/first_run.h"

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/util/google_update_settings.h"
#include "content/common/result_codes.h"
#include "googleurl/src/gurl.h"
#include "ui/base/ui_base_switches.h"

// TODO(port): This is just a piece of the silent import functionality from
// ImportSettings for Windows.  It would be nice to get the rest of it ported.
bool FirstRun::ImportBookmarks(const FilePath& import_bookmarks_path) {
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
  // Time to launch the process that is going to do the import. We'll wait
  // for the process to return.
  return base::LaunchApp(import_cmd, true, false, NULL);
}

// static
bool FirstRun::IsOrganicFirstRun() {
  // We treat all installs as organic.
  return true;
}

// static
void FirstRun::PlatformSetup() {
  // Things that Windows does here (creating a desktop icon, for example) are
  // handled at install time on Linux.
}
