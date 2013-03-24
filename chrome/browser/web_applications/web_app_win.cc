// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app.h"

#include <shlobj.h>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/md5.h"
#include "base/path_service.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/win/shortcut.h"
#include "base/win/windows_version.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/launcher_support/chrome_launcher_support.h"
#include "chrome/installer/util/browser_distribution.h"
#include "content/public/browser/browser_thread.h"
#include "ui/gfx/icon_util.h"

namespace {

const base::FilePath::CharType kIconChecksumFileExt[] =
    FILE_PATH_LITERAL(".ico.md5");

// Calculates image checksum using MD5.
void GetImageCheckSum(const SkBitmap& image, base::MD5Digest* digest) {
  DCHECK(digest);

  SkAutoLockPixels image_lock(image);
  MD5Sum(image.getPixels(), image.getSize(), digest);
}

// Saves |image| as an |icon_file| with the checksum.
bool SaveIconWithCheckSum(const base::FilePath& icon_file,
                          const SkBitmap& image) {
  if (!IconUtil::CreateIconFileFromSkBitmap(image, SkBitmap(), icon_file))
    return false;

  base::MD5Digest digest;
  GetImageCheckSum(image, &digest);

  base::FilePath cheksum_file(icon_file.ReplaceExtension(kIconChecksumFileExt));
  return file_util::WriteFile(cheksum_file,
                              reinterpret_cast<const char*>(&digest),
                              sizeof(digest)) == sizeof(digest);
}

// Returns true if |icon_file| is missing or different from |image|.
bool ShouldUpdateIcon(const base::FilePath& icon_file, const SkBitmap& image) {
  base::FilePath checksum_file(
      icon_file.ReplaceExtension(kIconChecksumFileExt));

  // Returns true if icon_file or checksum file is missing.
  if (!file_util::PathExists(icon_file) ||
      !file_util::PathExists(checksum_file))
    return true;

  base::MD5Digest persisted_image_checksum;
  if (sizeof(persisted_image_checksum) != file_util::ReadFile(checksum_file,
                      reinterpret_cast<char*>(&persisted_image_checksum),
                      sizeof(persisted_image_checksum)))
    return true;

  base::MD5Digest downloaded_image_checksum;
  GetImageCheckSum(image, &downloaded_image_checksum);

  // Update icon if checksums are not equal.
  return memcmp(&persisted_image_checksum, &downloaded_image_checksum,
                sizeof(base::MD5Digest)) != 0;
}

std::vector<base::FilePath> GetShortcutPaths(
    const ShellIntegration::ShortcutLocations& creation_locations) {
  // Shortcut paths under which to create shortcuts.
  std::vector<base::FilePath> shortcut_paths;

  // Locations to add to shortcut_paths.
  struct {
    bool use_this_location;
    int location_id;
    const wchar_t* sub_dir;
  } locations[] = {
    {
      creation_locations.on_desktop,
      base::DIR_USER_DESKTOP,
      NULL
    }, {
      creation_locations.in_applications_menu,
      base::DIR_START_MENU,
      NULL
    }, {
      creation_locations.in_quick_launch_bar,
      // For Win7, create_in_quick_launch_bar means pinning to taskbar. Use
      // base::PATH_START as a flag for this case.
      (base::win::GetVersion() >= base::win::VERSION_WIN7) ?
          base::PATH_START : base::DIR_APP_DATA,
      (base::win::GetVersion() >= base::win::VERSION_WIN7) ?
          NULL : L"Microsoft\\Internet Explorer\\Quick Launch"
    }
  };

  // Populate shortcut_paths.
  for (int i = 0; i < arraysize(locations); ++i) {
    if (locations[i].use_this_location) {
      base::FilePath path;

      // Skip the Win7 case.
      if (locations[i].location_id == base::PATH_START)
        continue;

      if (!PathService::Get(locations[i].location_id, &path)) {
        continue;
      }

      if (locations[i].sub_dir != NULL)
        path = path.Append(locations[i].sub_dir);

      shortcut_paths.push_back(path);
    }
  }

  return shortcut_paths;
}

bool ShortcutIsForProfile(const base::FilePath& shortcut_file_name,
                          const base::FilePath& profile_path) {
  string16 cmd_line_string;
  if (base::win::ResolveShortcut(shortcut_file_name, NULL, &cmd_line_string)) {
    cmd_line_string = L"program " + cmd_line_string;
    CommandLine shortcut_cmd_line = CommandLine::FromString(cmd_line_string);
    return shortcut_cmd_line.HasSwitch(switches::kProfileDirectory) &&
           shortcut_cmd_line.GetSwitchValuePath(switches::kProfileDirectory) ==
               profile_path.BaseName();
  }

  return false;
}

std::vector<base::FilePath> MatchingShortcutsForProfileAndExtension(
    const base::FilePath& shortcut_path,
    const base::FilePath& profile_path,
    const string16& shortcut_name) {
  std::vector<base::FilePath> shortcut_paths;
  base::FilePath base_path = shortcut_path.
      Append(web_app::internals::GetSanitizedFileName(shortcut_name)).
      ReplaceExtension(FILE_PATH_LITERAL(".lnk"));

  const int fileNamesToCheck = 10;
  for (int i = 0; i < fileNamesToCheck; ++i) {
    base::FilePath shortcut_file = base_path;
    if (i) {
      shortcut_file = shortcut_file.InsertBeforeExtensionASCII(
          base::StringPrintf(" (%d)", i));
    }
    if (file_util::PathExists(shortcut_file) &&
        ShortcutIsForProfile(shortcut_file, profile_path)) {
      shortcut_paths.push_back(shortcut_file);
    }
  }
  return shortcut_paths;
}

}  // namespace

namespace web_app {

namespace internals {

// Saves |image| to |icon_file| if the file is outdated and refresh shell's
// icon cache to ensure correct icon is displayed. Returns true if icon_file
// is up to date or successfully updated.
bool CheckAndSaveIcon(const base::FilePath& icon_file, const SkBitmap& image) {
  if (ShouldUpdateIcon(icon_file, image)) {
    if (SaveIconWithCheckSum(icon_file, image)) {
      // Refresh shell's icon cache. This call is quite disruptive as user would
      // see explorer rebuilding the icon cache. It would be great that we find
      // a better way to achieve this.
      SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST | SHCNF_FLUSHNOWAIT,
                     NULL, NULL);
    } else {
      return false;
    }
  }

  return true;
}

base::FilePath GetShortcutExecutablePath(
    const ShellIntegration::ShortcutInfo& shortcut_info) {
  if (shortcut_info.is_platform_app &&
      BrowserDistribution::GetDistribution()->AppHostIsSupported() &&
      chrome_launcher_support::IsAppHostPresent()) {
    return chrome_launcher_support::GetAnyAppHostPath();
  }

  return chrome_launcher_support::GetAnyChromePath();
}

bool CreatePlatformShortcuts(
    const base::FilePath& web_app_path,
    const ShellIntegration::ShortcutInfo& shortcut_info,
    const ShellIntegration::ShortcutLocations& creation_locations) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));

  // Shortcut paths under which to create shortcuts.
  std::vector<base::FilePath> shortcut_paths =
      GetShortcutPaths(creation_locations);

  bool pin_to_taskbar = creation_locations.in_quick_launch_bar &&
                        (base::win::GetVersion() >= base::win::VERSION_WIN7);

  // Create/update the shortcut in the web app path for the "Pin To Taskbar"
  // option in Win7. We use the web app path shortcut because we will overwrite
  // it rather than appending unique numbers if the shortcut already exists.
  // This prevents pinned apps from having unique numbers in their names.
  if (pin_to_taskbar)
    shortcut_paths.push_back(web_app_path);

  if (shortcut_paths.empty())
    return false;

  // Ensure web_app_path exists.
  if (!file_util::PathExists(web_app_path) &&
      !file_util::CreateDirectory(web_app_path)) {
    return false;
  }

  // Generates file name to use with persisted ico and shortcut file.
  base::FilePath file_name =
      web_app::internals::GetSanitizedFileName(shortcut_info.title);

  // Creates an ico file to use with shortcut.
  base::FilePath icon_file = web_app_path.Append(file_name).ReplaceExtension(
      FILE_PATH_LITERAL(".ico"));
  if (!web_app::internals::CheckAndSaveIcon(icon_file,
        *shortcut_info.favicon.ToSkBitmap())) {
    return false;
  }

  base::FilePath target_exe = GetShortcutExecutablePath(shortcut_info);
  DCHECK(!target_exe.empty());

  // Working directory.
  base::FilePath working_dir(target_exe.DirName());

  CommandLine cmd_line(CommandLine::NO_PROGRAM);
  cmd_line = ShellIntegration::CommandLineArgsForLauncher(shortcut_info.url,
      shortcut_info.extension_id, shortcut_info.profile_path);

  // TODO(evan): we rely on the fact that command_line_string() is
  // properly quoted for a Windows command line.  The method on
  // CommandLine should probably be renamed to better reflect that
  // fact.
  string16 wide_switches(cmd_line.GetCommandLineString());

  // Sanitize description
  string16 description = shortcut_info.description;
  if (description.length() >= MAX_PATH)
    description.resize(MAX_PATH - 1);

  // Generates app id from web app url and profile path.
  std::string app_name(web_app::GenerateApplicationNameFromInfo(shortcut_info));
  string16 app_id(ShellIntegration::GetAppModelIdForProfile(
      UTF8ToUTF16(app_name), shortcut_info.profile_path));

  bool success = true;
  for (size_t i = 0; i < shortcut_paths.size(); ++i) {
    base::FilePath shortcut_file = shortcut_paths[i].Append(file_name).
        ReplaceExtension(FILE_PATH_LITERAL(".lnk"));
    if (shortcut_paths[i] != web_app_path) {
      int unique_number =
          file_util::GetUniquePathNumber(shortcut_file, FILE_PATH_LITERAL(""));
      if (unique_number == -1) {
        success = false;
        continue;
      } else if (unique_number > 0) {
        shortcut_file = shortcut_file.InsertBeforeExtensionASCII(
            base::StringPrintf(" (%d)", unique_number));
      }
    }
    base::win::ShortcutProperties shortcut_properties;
    shortcut_properties.set_target(target_exe);
    shortcut_properties.set_working_dir(working_dir);
    shortcut_properties.set_arguments(wide_switches);
    shortcut_properties.set_description(description);
    shortcut_properties.set_icon(icon_file, 0);
    shortcut_properties.set_app_id(app_id);
    shortcut_properties.set_dual_mode(false);
    success = base::win::CreateOrUpdateShortcutLink(
        shortcut_file, shortcut_properties,
        base::win::SHORTCUT_CREATE_ALWAYS) && success;
  }

  if (success && pin_to_taskbar) {
    // Use the web app path shortcut for pinning to avoid having unique numbers
    // in the application name.
    base::FilePath shortcut_to_pin = web_app_path.Append(file_name).
        ReplaceExtension(FILE_PATH_LITERAL(".lnk"));
    success = base::win::TaskbarPinShortcutLink(
        shortcut_to_pin.value().c_str()) && success;
  }

  return success;
}

void UpdatePlatformShortcuts(
    const base::FilePath& web_app_path,
    const ShellIntegration::ShortcutInfo& shortcut_info) {
  // Generates file name to use with persisted ico and shortcut file.
  base::FilePath file_name =
      web_app::internals::GetSanitizedFileName(shortcut_info.title);

  // If an icon file exists, and is out of date, replace it with the new icon
  // and let the shell know the icon has been modified.
  base::FilePath icon_file = web_app_path.Append(file_name).ReplaceExtension(
      FILE_PATH_LITERAL(".ico"));
  if (file_util::PathExists(icon_file)) {
    web_app::internals::CheckAndSaveIcon(icon_file,
        *shortcut_info.favicon.ToSkBitmap());
  }
}

void DeletePlatformShortcuts(
    const base::FilePath& web_app_path,
    const ShellIntegration::ShortcutInfo& shortcut_info) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));

  // Get all possible locations for shortcuts.
  ShellIntegration::ShortcutLocations all_shortcut_locations;
  all_shortcut_locations.in_applications_menu = true;
  all_shortcut_locations.in_quick_launch_bar = true;
  all_shortcut_locations.on_desktop = true;
  std::vector<base::FilePath> shortcut_paths = GetShortcutPaths(
      all_shortcut_locations);
  if (base::win::GetVersion() >= base::win::VERSION_WIN7)
    shortcut_paths.push_back(web_app_path);

  for (std::vector<base::FilePath>::const_iterator i = shortcut_paths.begin();
       i != shortcut_paths.end(); ++i) {
    std::vector<base::FilePath> shortcut_files =
        MatchingShortcutsForProfileAndExtension(*i, shortcut_info.profile_path,
            shortcut_info.title);
    for (std::vector<base::FilePath>::const_iterator j = shortcut_files.begin();
         j != shortcut_files.end(); ++j) {
      // Any shortcut could have been pinned, either by chrome or the user, so
      // they are all unpinned.
      base::win::TaskbarUnpinShortcutLink(j->value().c_str());
      file_util::Delete(*j, false);
    }
  }
}

}  // namespace internals

}  // namespace web_app
