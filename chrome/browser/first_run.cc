// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run.h"

#include "build/build_config.h"

// TODO(port): move more code in back from the first_run_win.cc module.

#if defined(OS_WIN)
#include "chrome/installer/util/install_util.h"
#endif

#include "base/file_util.h"
#include "base/path_service.h"
#include "chrome/browser/importer/importer.h"
#include "chrome/browser/pref_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"

namespace {

// The kSentinelFile file absence will tell us it is a first run.
const char kSentinelFile[] = "First Run";

}  // namespace

// TODO(port): Import switches need to be ported to both Mac and Linux. Not all
// import switches here are implemented for Linux. None are implemented for Mac
// (as this function will not be called on Mac).
int FirstRun::ImportNow(Profile* profile, const CommandLine& cmdline) {
  int return_code = true;
  if (cmdline.HasSwitch(switches::kImportFromFile)) {
    // Silently import preset bookmarks from file.
    // This is an OEM scenario.
    return_code = ImportFromFile(profile, cmdline);
  }
  if (cmdline.HasSwitch(switches::kImport)) {
#if defined(OS_WIN)
    return_code = ImportFromBrowser(profile, cmdline);
#else
    NOTIMPLEMENTED();
#endif
  }
  return return_code;
}

// static
bool FirstRun::IsChromeFirstRun() {
  // A troolean, 0 means not yet set, 1 means set to true, 2 set to false.
  static int first_run = 0;
  if (first_run != 0)
    return first_run == 1;

  FilePath first_run_sentinel;
  if (!GetFirstRunSentinelFilePath(&first_run_sentinel) ||
      file_util::PathExists(first_run_sentinel)) {
    first_run = 2;
    return false;
  }
  first_run = 1;
  return true;
}

// static
bool FirstRun::RemoveSentinel() {
  FilePath first_run_sentinel;
  if (!GetFirstRunSentinelFilePath(&first_run_sentinel))
    return false;
  return file_util::Delete(first_run_sentinel, false);
}

// static
bool FirstRun::CreateSentinel() {
  FilePath first_run_sentinel;
  if (!GetFirstRunSentinelFilePath(&first_run_sentinel))
    return false;
  return file_util::WriteFile(first_run_sentinel, "", 0) != -1;
}

// static
bool FirstRun::SetShowFirstRunBubblePref(bool show_bubble) {
  PrefService* local_state = g_browser_process->local_state();
  if (!local_state)
    return false;
  if (!local_state->FindPreference(prefs::kShouldShowFirstRunBubble)) {
    local_state->RegisterBooleanPref(prefs::kShouldShowFirstRunBubble, false);
    local_state->SetBoolean(prefs::kShouldShowFirstRunBubble, show_bubble);
  }
  return true;
}

// static
bool FirstRun::SetShowWelcomePagePref() {
  PrefService* local_state = g_browser_process->local_state();
  if (!local_state)
    return false;
  if (!local_state->FindPreference(prefs::kShouldShowWelcomePage)) {
    local_state->RegisterBooleanPref(prefs::kShouldShowWelcomePage, false);
    local_state->SetBoolean(prefs::kShouldShowWelcomePage, true);
  }
  return true;
}

// static
bool FirstRun::SetOEMFirstRunBubblePref() {
  PrefService* local_state = g_browser_process->local_state();
  if (!local_state)
    return false;
  if (!local_state->FindPreference(prefs::kShouldUseOEMFirstRunBubble)) {
    local_state->RegisterBooleanPref(prefs::kShouldUseOEMFirstRunBubble,
                                     false);
    local_state->SetBoolean(prefs::kShouldUseOEMFirstRunBubble, true);
  }
  return true;
}

// static
bool FirstRun::SetMinimalFirstRunBubblePref() {
  PrefService* local_state = g_browser_process->local_state();
  if (!local_state)
    return false;
  if (!local_state->FindPreference(prefs::kShouldUseMinimalFirstRunBubble)) {
    local_state->RegisterBooleanPref(prefs::kShouldUseMinimalFirstRunBubble,
                                     false);
    local_state->SetBoolean(prefs::kShouldUseMinimalFirstRunBubble, true);
  }
  return true;
}

// static
int FirstRun::ImportFromFile(Profile* profile, const CommandLine& cmdline) {
  FilePath file_path = cmdline.GetSwitchValuePath(switches::kImportFromFile);
  if (file_path.empty()) {
    NOTREACHED();
    return false;
  }
  scoped_refptr<ImporterHost> importer_host = new ImporterHost();
  FirstRunImportObserver observer;

  importer_host->set_headless();

  ProfileInfo profile_info;
  profile_info.browser_type = importer::BOOKMARKS_HTML;
  profile_info.source_path = file_path;

  StartImportingWithUI(
      NULL,
      importer::FAVORITES,
      importer_host,
      profile_info,
      profile,
      &observer,
      true);

  observer.RunLoop();
  return observer.import_result();
}

// static
bool FirstRun::GetFirstRunSentinelFilePath(FilePath* path) {
  FilePath first_run_sentinel;

#if defined(OS_WIN)
  FilePath exe_path;
  if (!PathService::Get(base::DIR_EXE, &exe_path))
    return false;
  if (InstallUtil::IsPerUserInstall(exe_path.value().c_str())) {
    first_run_sentinel = exe_path;
  } else {
    if (!PathService::Get(chrome::DIR_USER_DATA, &first_run_sentinel))
      return false;
  }
#else
  if (!PathService::Get(chrome::DIR_USER_DATA, &first_run_sentinel))
    return false;
#endif

  *path = first_run_sentinel.AppendASCII(kSentinelFile);
  return true;
}

#if (defined(OS_WIN) || defined(OS_LINUX)) && !defined(OS_CHROMEOS)
// static
void Upgrade::RelaunchChromeBrowserWithNewCommandLineIfNeeded() {
  if (new_command_line_) {
    if (!RelaunchChromeBrowser(*new_command_line_)) {
      DLOG(ERROR) << "Launching a new instance of the browser failed.";
    } else {
      DLOG(WARNING) << "Launched a new instance of the browser.";
    }
    delete new_command_line_;
    new_command_line_ = NULL;
  }
}
#endif  // (defined(OS_WIN) || defined(OS_LINUX)) && !defined(OS_CHROMEOS)

int FirstRunImportObserver::import_result() const {
  return import_result_;
}

void FirstRunImportObserver::ImportCanceled() {
  import_result_ = ResultCodes::IMPORTER_CANCEL;
  Finish();
}
void FirstRunImportObserver::ImportComplete() {
  import_result_ = ResultCodes::NORMAL_EXIT;
  Finish();
}

void FirstRunImportObserver::RunLoop() {
  loop_running_ = true;
  MessageLoop::current()->Run();
}

void FirstRunImportObserver::Finish() {
  if (loop_running_)
    MessageLoop::current()->Quit();
}

