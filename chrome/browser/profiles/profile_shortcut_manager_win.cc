// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_shortcut_manager_win.h"

#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/shell_util.h"
#include "content/public/browser/browser_thread.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;

namespace {

// Creates the argument to pass to the Windows executable that launches Chrome
// with the profile in |profile_base_dir|.
// For example: --profile-directory="Profile 2"
string16 CreateProfileShortcutSwitch(string16 profile_base_dir) {
  string16 profile_directory = base::StringPrintf(L"--%ls=\"%ls\"",
      ASCIIToUTF16(switches::kProfileDirectory).c_str(),
      profile_base_dir.c_str());
  return profile_directory;
}

// Wrap a ShellUtil function that returns a bool so it can be posted in a
// task to the FILE thread.
void CallShellUtilBoolFunction(
    const base::Callback<bool(void)>& bool_function) {
  bool_function.Run();
}

}  // namespace

ProfileShortcutManagerWin::ProfileShortcutManagerWin() {
}

ProfileShortcutManagerWin::~ProfileShortcutManagerWin() {
}

void ProfileShortcutManagerWin::OnProfileAdded(
    const string16& profile_name,
    const string16& profile_base_dir) {
  // Launch task to add shortcut to desktop on Windows. If this is the very
  // first profile created, don't add the user name to the shortcut.
  // TODO(mirandac): respect master_preferences choice to create no shortcuts
  // (see http://crbug.com/104463)
  if (g_browser_process->profile_manager()->GetNumberOfProfiles() > 1) {
    string16 profile_directory =
        CreateProfileShortcutSwitch(profile_base_dir);
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
        base::Bind(&CreateChromeDesktopShortcutForProfile,
                   profile_name, profile_directory, true));

    // If this is the very first multi-user account created, change the
    // original shortcut to launch with the First User profile.
    PrefService* local_state = g_browser_process->local_state();
    if (local_state->GetInteger(prefs::kProfilesNumCreated) == 2) {
      string16 default_name = l10n_util::GetStringUTF16(
          IDS_DEFAULT_PROFILE_NAME);
      string16 default_directory =
          CreateProfileShortcutSwitch(UTF8ToUTF16(chrome::kInitialProfile));
      BrowserDistribution* dist = BrowserDistribution::GetDistribution();

      string16 old_shortcut;
      string16 new_shortcut;
      if (ShellUtil::GetChromeShortcutName(dist, false, L"", &old_shortcut) &&
          ShellUtil::GetChromeShortcutName(dist, false, default_name,
                                           &new_shortcut)) {
        // Update doesn't allow changing the target, so rename first.
        BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
            base::Bind(&RenameChromeDesktopShortcutForProfile,
                       old_shortcut, new_shortcut));
        BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
            base::Bind(&UpdateChromeDesktopShortcutForProfile,
                       new_shortcut, default_directory));
      }
    }
  } else {  // Only one profile, so create original shortcut.
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
        base::Bind(&CreateChromeDesktopShortcutForProfile,
                   L"", L"", true));
  }
}

void ProfileShortcutManagerWin::OnProfileRemoved(
    const string16& profile_name) {
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  string16 shortcut;
  if (ShellUtil::GetChromeShortcutName(dist, false, profile_name, &shortcut)) {
    std::vector<string16> shortcuts(1, shortcut);
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
        base::Bind(&CallShellUtilBoolFunction,
            base::Bind(
                &ShellUtil::RemoveChromeDesktopShortcutsWithAppendedNames,
                shortcuts)));
  }
}

void ProfileShortcutManagerWin::OnProfileNameChanged(
    const string16& old_profile_name,
    const string16& new_profile_name) {
  // Launch task to change name of desktop shortcut on Windows.
  // TODO(mirandac): respect master_preferences choice to create no shortcuts
  // (see http://crbug.com/104463)
  string16 old_shortcut;
  string16 new_shortcut;
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  if (ShellUtil::GetChromeShortcutName(
          dist, false, old_profile_name, &old_shortcut) &&
      ShellUtil::GetChromeShortcutName(
          dist, false, new_profile_name, &new_shortcut)) {
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
        base::Bind(&RenameChromeDesktopShortcutForProfile,
                   old_shortcut,
                   new_shortcut));
  }
}

// static
std::vector<string16> ProfileShortcutManagerWin::GenerateShortcutsFromProfiles(
    const std::vector<string16>& profile_names) {
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  std::vector<string16> shortcuts;
  shortcuts.reserve(profile_names.size());
  for (std::vector<string16>::const_iterator it = profile_names.begin();
       it != profile_names.end();
       ++it) {
    string16 shortcut;
    if (ShellUtil::GetChromeShortcutName(dist, false, *it, &shortcut))
      shortcuts.push_back(shortcut);
  }
  return shortcuts;
}

// static
void ProfileShortcutManagerWin::CreateChromeDesktopShortcutForProfile(
    const string16& profile_name,
    const string16& directory,
    bool create) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  FilePath chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe))
    return;
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  string16 description;
  if (!dist)
    return;
  else
    description = WideToUTF16(dist->GetAppDescription());
  ShellUtil::CreateChromeDesktopShortcut(
      dist,
      chrome_exe.value(),
      description,
      profile_name,
      directory,
      ShellUtil::CURRENT_USER,
      false,  // Use alternate text.
      create);  // Create if it doesn't already exist.
}

// static
void ProfileShortcutManagerWin::RenameChromeDesktopShortcutForProfile(
    const string16& old_shortcut,
    const string16& new_shortcut) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  FilePath shortcut_path;
  if (ShellUtil::GetDesktopPath(false,  // User's directory instead of system.
                                &shortcut_path)) {
    FilePath old_profile = shortcut_path.Append(old_shortcut);
    FilePath new_profile = shortcut_path.Append(new_shortcut);
    file_util::Move(old_profile, new_profile);
  }
}

// static
void ProfileShortcutManagerWin::UpdateChromeDesktopShortcutForProfile(
    const string16& shortcut,
    const string16& arguments) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  FilePath shortcut_path;
  if (!ShellUtil::GetDesktopPath(false, &shortcut_path))
    return;

  shortcut_path = shortcut_path.Append(shortcut);
  FilePath chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe))
    return;
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  string16 description;
  if (!dist)
    return;
  else
    description = WideToUTF16(dist->GetAppDescription());
  ShellUtil::UpdateChromeShortcut(
      dist,
      chrome_exe.value(),
      shortcut_path.value(),
      arguments,
      description,
      false);
}
