// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/note_taking_helper.h"

#include <utility>

#include "apps/launcher.h"
#include "ash/common/system/chromeos/palette/palette_utils.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_split.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/api/app_runtime.h"
#include "extensions/common/extension.h"
#include "url/gurl.h"

namespace app_runtime = extensions::api::app_runtime;

namespace chromeos {
namespace {

// Pointer to singleton instance.
NoteTakingHelper* g_helper = nullptr;

// Whitelisted Chrome note-taking apps.
const char* const kExtensionIds[] = {
    // TODO(jdufault): Remove dev version? See crbug.com/640828.
    NoteTakingHelper::kDevKeepExtensionId,
    NoteTakingHelper::kProdKeepExtensionId,
};

// Returns all installed and enabled whitelisted Chrome note-taking apps.
std::vector<const extensions::Extension*> GetChromeApps(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // TODO(derat): Query for additional Chrome apps once http://crbug.com/657139
  // is resolved.
  std::vector<extensions::ExtensionId> ids;
  const std::string switch_value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kNoteTakingAppIds);
  if (!switch_value.empty()) {
    ids = base::SplitString(switch_value, ",", base::TRIM_WHITESPACE,
                            base::SPLIT_WANT_NONEMPTY);
  }
  ids.insert(ids.end(), kExtensionIds,
             kExtensionIds + arraysize(kExtensionIds));

  const extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(profile);
  const extensions::ExtensionSet& enabled_extensions =
      extension_registry->enabled_extensions();

  std::vector<const extensions::Extension*> extensions;
  for (const auto& id : ids) {
    if (enabled_extensions.Contains(id)) {
      extensions.push_back(extension_registry->GetExtensionById(
          id, extensions::ExtensionRegistry::ENABLED));
    }
  }
  return extensions;
}

}  // namespace

const char NoteTakingHelper::kDevKeepExtensionId[] =
    "ogfjaccbdfhecploibfbhighmebiffla";
const char NoteTakingHelper::kProdKeepExtensionId[] =
    "hmjkmjkepdijhoojdojkdfohbdgmmhki";

// static
void NoteTakingHelper::Initialize() {
  DCHECK(!g_helper);
  g_helper = new NoteTakingHelper();
}

// static
void NoteTakingHelper::Shutdown() {
  DCHECK(g_helper);
  delete g_helper;
  g_helper = nullptr;
}

// static
NoteTakingHelper* NoteTakingHelper::Get() {
  DCHECK(g_helper);
  return g_helper;
}

NoteTakingAppInfos NoteTakingHelper::GetAvailableApps(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  NoteTakingAppInfos infos;

  const std::vector<const extensions::Extension*> chrome_apps =
      GetChromeApps(profile);
  for (const auto& app : chrome_apps)
    infos.push_back(NoteTakingAppInfo{app->name(), app->id(), false});

  // Determine which app, if any, is preferred.
  const std::string pref_app_id =
      profile->GetPrefs()->GetString(prefs::kNoteTakingAppId);
  for (auto& info : infos) {
    if (info.app_id == pref_app_id) {
      info.preferred = true;
      break;
    }
  }

  return infos;
}

void NoteTakingHelper::SetPreferredApp(Profile* profile,
                                       const std::string& app_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile);
  profile->GetPrefs()->SetString(prefs::kNoteTakingAppId, app_id);
}

bool NoteTakingHelper::IsAppAvailable(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile);
  return ash::IsPaletteFeatureEnabled() && !GetAvailableApps(profile).empty();
}

void NoteTakingHelper::LaunchAppForNewNote(Profile* profile,
                                           const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile);
  std::string app_id = profile->GetPrefs()->GetString(prefs::kNoteTakingAppId);
  if (!app_id.empty() && LaunchAppInternal(profile, app_id, path))
    return;

  // If the user hasn't chosen an app or we were unable to launch the one that
  // they've chosen, just launch the first one we see.
  NoteTakingAppInfos infos = GetAvailableApps(profile);
  if (infos.empty())
    LOG(WARNING) << "Unable to launch note-taking app; none available";
  else
    LaunchAppInternal(profile, infos[0].app_id, path);
}

NoteTakingHelper::NoteTakingHelper()
    : launch_chrome_app_callback_(
          base::Bind(&apps::LaunchPlatformAppWithAction)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

NoteTakingHelper::~NoteTakingHelper() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

bool NoteTakingHelper::LaunchAppInternal(Profile* profile,
                                         const std::string& app_id,
                                         const base::FilePath& path) {
  DCHECK(profile);

  const extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(profile);
  const extensions::Extension* app = extension_registry->GetExtensionById(
      app_id, extensions::ExtensionRegistry::ENABLED);
  if (!app) {
    LOG(WARNING) << "Failed to find Chrome note-taking app " << app_id;
    return false;
  }
  auto action_data = base::MakeUnique<app_runtime::ActionData>();
  action_data->action_type = app_runtime::ActionType::ACTION_TYPE_NEW_NOTE;
  launch_chrome_app_callback_.Run(profile, app, std::move(action_data), path);

  return true;
}

}  // namespace chromeos
