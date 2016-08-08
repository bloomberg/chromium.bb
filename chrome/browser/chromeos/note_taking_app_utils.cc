// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/note_taking_app_utils.h"

#include <string>
#include <vector>

#include "apps/launcher.h"
#include "ash/common/system/chromeos/palette/palette_utils.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/strings/string_split.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/chromeos_switches.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "url/gurl.h"

namespace chromeos {
namespace {

// TODO(derat): Add more IDs.
const char* const kExtensionIds[] = {
    "hmjkmjkepdijhoojdojkdfohbdgmmhki",  // Google Keep app (Web Store)
};

// Returns the first installed and enabled whitelisted note-taking app, or null
// if none is installed.
const extensions::Extension* GetApp(Profile* profile) {
  // TODO(derat): Check the to-be-added "note-taking app enabled" pref here and
  // return null if it's disabled.

  std::vector<std::string> ids;

  // TODO(derat): Instead of a using hardcoded list of IDs, use an app
  // designated by a to-be-added pref.
  const std::string switch_value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kNoteTakingAppIds);
  if (!switch_value.empty()) {
    ids = base::SplitString(switch_value, ",", base::TRIM_WHITESPACE,
                            base::SPLIT_WANT_NONEMPTY);
  } else {
    ids.assign(kExtensionIds, kExtensionIds + arraysize(kExtensionIds));
  }

  const extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(profile);
  const extensions::ExtensionSet& enabled_extensions =
      extension_registry->enabled_extensions();
  for (const auto& id : ids) {
    if (enabled_extensions.Contains(id)) {
      return extension_registry->GetExtensionById(
          id, extensions::ExtensionRegistry::ENABLED);
    }
  }
  return nullptr;
}

}  // namespace

bool IsNoteTakingAppAvailable(Profile* profile) {
  DCHECK(profile);
  return ash::IsPaletteEnabled() && GetApp(profile);
}

void LaunchNoteTakingAppForNewNote(Profile* profile,
                                   const base::FilePath& path) {
  DCHECK(profile);
  const extensions::Extension* app = GetApp(profile);
  if (!app) {
    LOG(ERROR) << "Failed to find note-taking app";
    return;
  }

  // TODO(derat): Launch with a "create new note" launch action once that's been
  // added to chrome.appRuntime. Also decide what should be passed as the launch
  // source.
  if (path.empty())
    apps::LaunchPlatformApp(profile, app, extensions::SOURCE_UNTRACKED);
  else
    apps::LaunchPlatformAppWithPath(profile, app, path);
}

}  // namespace chromeos
