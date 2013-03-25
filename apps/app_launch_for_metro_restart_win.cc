// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/app_launch_for_metro_restart_win.h"

#include "apps/pref_names.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/app_runtime/app_runtime_api.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/platform_app_launcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "win8/util/win8_util.h"

using extensions::Extension;
using extensions::ExtensionSystem;

namespace apps {

namespace {

void LaunchAppWithId(Profile* profile,
                     const std::string& extension_id) {
  ExtensionService* extension_service =
      ExtensionSystem::Get(profile)->extension_service();
  if (!extension_service)
    return;

  const Extension* extension =
      extension_service->GetExtensionById(extension_id, false);
  if (!extension)
    return;

  extensions::AppEventRouter::DispatchOnLaunchedEvent(profile, extension);
}

}  // namespace

void HandleAppLaunchForMetroRestart(Profile* profile) {
  PrefService* prefs = g_browser_process->local_state();
  if (!prefs->HasPrefPath(prefs::kAppLaunchForMetroRestartProfile))
    return;

  // This will be called for each profile that had a browser window open before
  // relaunch.  After checking that the preference is set, check that the
  // profile that is starting up matches the profile that initially wanted to
  // launch the app.
  base::FilePath profile_dir = base::FilePath::FromUTF8Unsafe(
      prefs->GetString(prefs::kAppLaunchForMetroRestartProfile));
  if (profile_dir.empty() || profile->GetPath().BaseName() != profile_dir)
    return;

  prefs->ClearPref(prefs::kAppLaunchForMetroRestartProfile);

  if (!prefs->HasPrefPath(prefs::kAppLaunchForMetroRestart))
    return;

  std::string extension_id = prefs->GetString(prefs::kAppLaunchForMetroRestart);
  if (extension_id.empty())
    return;

  prefs->ClearPref(prefs::kAppLaunchForMetroRestart);

  if (win8::IsSingleWindowMetroMode()) {
    // In this case we have relaunched with the correct profile, but we are not
    // in Desktop mode, so can not launch apps. Leave the preferences cleared so
    // there are no surprises later.
    return;
  }

  const int kRestartAppLaunchDelayMs = 1000;
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&LaunchAppWithId,
                 profile,
                 extension_id),
      base::TimeDelta::FromMilliseconds(kRestartAppLaunchDelayMs));
}

void SetAppLaunchForMetroRestart(Profile* profile,
                                 const std::string& extension_id) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetString(prefs::kAppLaunchForMetroRestartProfile,
                   profile->GetPath().BaseName().MaybeAsASCII());
  prefs->SetString(prefs::kAppLaunchForMetroRestart, extension_id);
}

}  // namespace apps
