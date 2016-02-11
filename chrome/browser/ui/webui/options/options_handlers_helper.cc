// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/browser_finder.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"

namespace options {
namespace helper {

void OpenNewWindowForProfile(Profile* profile, Profile::CreateStatus status) {
  if (status != Profile::CREATE_STATUS_INITIALIZED)
    return;

  profiles::FindOrCreateNewWindowForProfile(
      profile, chrome::startup::IS_PROCESS_STARTUP,
      chrome::startup::IS_FIRST_RUN, false);
}

void DeleteProfileAtPath(base::FilePath file_path, content::WebUI* web_ui) {
  DCHECK(web_ui);

  if (!profiles::IsMultipleProfilesEnabled())
    return;

  ProfileMetrics::LogProfileDeleteUser(ProfileMetrics::DELETE_PROFILE_SETTINGS);

  g_browser_process->profile_manager()->ScheduleProfileForDeletion(
      file_path, base::Bind(&OpenNewWindowForProfile));
}

}  // namespace helper
}  // namespace options
