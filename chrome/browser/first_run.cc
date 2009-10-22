// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
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
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"

namespace {

// The kSentinelFile file absence will tell us it is a first run.
const char kSentinelFile[] = "First Run";

// Gives the full path to the sentinel file. The file might not exist.
bool GetFirstRunSentinelFilePath(FilePath* path) {
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

}  // namespace

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

bool FirstRun::RemoveSentinel() {
  FilePath first_run_sentinel;
  if (!GetFirstRunSentinelFilePath(&first_run_sentinel))
    return false;
  return file_util::Delete(first_run_sentinel, false);
}

bool FirstRun::CreateSentinel() {
  FilePath first_run_sentinel;
  if (!GetFirstRunSentinelFilePath(&first_run_sentinel))
    return false;
  return file_util::WriteFile(first_run_sentinel, "", 0) != -1;
}

bool FirstRun::SetShowFirstRunBubblePref() {
  PrefService* local_state = g_browser_process->local_state();
  if (!local_state)
    return false;
  if (!local_state->IsPrefRegistered(prefs::kShouldShowFirstRunBubble)) {
    local_state->RegisterBooleanPref(prefs::kShouldShowFirstRunBubble, false);
    local_state->SetBoolean(prefs::kShouldShowFirstRunBubble, true);
  }
  return true;
}

bool FirstRun::SetShowWelcomePagePref() {
  PrefService* local_state = g_browser_process->local_state();
  if (!local_state)
    return false;
  if (!local_state->IsPrefRegistered(prefs::kShouldShowWelcomePage)) {
    local_state->RegisterBooleanPref(prefs::kShouldShowWelcomePage, false);
    local_state->SetBoolean(prefs::kShouldShowWelcomePage, true);
  }
  return true;
}

