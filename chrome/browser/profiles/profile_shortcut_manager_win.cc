// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_shortcut_manager_win.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string16.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/win/shortcut.h"
#include "chrome/browser/app_icon_win.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_info_cache_observer.h"
#include "chrome/browser/profiles/profile_info_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/product.h"
#include "chrome/installer/util/shell_util.h"
#include "content/public/browser/browser_thread.h"
#include "skia/ext/image_operations.h"
#include "skia/ext/platform_canvas.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/icon_util.h"
#include "ui/gfx/image/image.h"

using content::BrowserThread;

namespace {

const int kProfileAvatarShortcutBadgeWidth = 28;
const int kProfileAvatarShortcutBadgeHeight = 28;
const int kShortcutIconSize = 48;

// Creates a desktop shortcut icon file (.ico) on the disk for a given profile,
// badging the browser distribution icon with the profile avatar.
// Returns a path to the shortcut icon file on disk, which is empty if this
// fails. Use index 0 when assigning the resulting file as the icon.
FilePath CreateChromeDesktopShortcutIconForProfile(
    const FilePath& profile_path,
    const SkBitmap& avatar_bitmap) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  HICON app_icon_handle = GetAppIconForSize(kShortcutIconSize);
  scoped_ptr<SkBitmap> app_icon_bitmap(
      IconUtil::CreateSkBitmapFromHICON(app_icon_handle));
  DestroyIcon(app_icon_handle);
  if (!app_icon_bitmap.get())
    return FilePath();

  // TODO(rlp): Share this chunk of code with
  // avatar_menu_button::DrawTaskBarDecoration.
  const SkBitmap* source_bitmap = NULL;
  SkBitmap squarer_bitmap;
  if ((avatar_bitmap.width() == profiles::kAvatarIconWidth) &&
      (avatar_bitmap.height() == profiles::kAvatarIconHeight)) {
    // Shave a couple of columns so the bitmap is more square. So when
    // resized to a square aspect ratio it looks pretty.
    int x = 2;
    avatar_bitmap.extractSubset(&squarer_bitmap, SkIRect::MakeXYWH(x, 0,
        profiles::kAvatarIconWidth - x * 2, profiles::kAvatarIconHeight));
    source_bitmap = &squarer_bitmap;
  } else {
    source_bitmap = &avatar_bitmap;
  }
  SkBitmap sk_icon = skia::ImageOperations::Resize(
      *source_bitmap,
      skia::ImageOperations::RESIZE_LANCZOS3,
      kProfileAvatarShortcutBadgeWidth,
      kProfileAvatarShortcutBadgeHeight);

  // Overlay the avatar on the icon, anchoring it to the bottom-right of the
  // icon.
  scoped_ptr<SkCanvas> offscreen_canvas(
      skia::CreateBitmapCanvas(app_icon_bitmap->width(),
                               app_icon_bitmap->height(),
                               false));
  DCHECK(offscreen_canvas.get());
  offscreen_canvas->drawBitmap(*app_icon_bitmap, 0, 0);
  offscreen_canvas->drawBitmap(
      sk_icon,
      app_icon_bitmap->width() - kProfileAvatarShortcutBadgeWidth,
      app_icon_bitmap->height() - kProfileAvatarShortcutBadgeHeight);
  const SkBitmap& final_bitmap =
      offscreen_canvas->getDevice()->accessBitmap(false);

  // Finally, write the .ico file containing this new bitmap.
  const FilePath icon_path =
      profile_path.AppendASCII(profiles::internal::kProfileIconFileName);
  if (!IconUtil::CreateIconFileFromSkBitmap(final_bitmap, icon_path))
    return FilePath();

  return icon_path;
}

// Gets the directory where to create desktop shortcuts.
bool GetDesktopShortcutsDirectory(FilePath* directory) {
  const bool result =
      ShellUtil::GetShortcutPath(ShellUtil::SHORTCUT_LOCATION_DESKTOP,
                                 BrowserDistribution::GetDistribution(),
                                 ShellUtil::CURRENT_USER, directory);
  DCHECK(result);
  return result;
}

// Returns the long form of |path|, which will expand any shortened components
// like "foo~2" to their full names.
FilePath ConvertToLongPath(const FilePath& path) {
  const size_t length = GetLongPathName(path.value().c_str(), NULL, 0);
  if (length != 0 && length != path.value().length()) {
    std::vector<wchar_t> long_path(length);
    if (GetLongPathName(path.value().c_str(), &long_path[0], length) != 0)
      return FilePath(&long_path[0]);
  }
  return path;
}

// Returns true if the file at |path| is a Chrome shortcut and returns its
// command line in output parameter |command_line|.
bool IsChromeShortcut(const FilePath& path,
                      const FilePath& chrome_exe,
                      string16* command_line) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  if (path.Extension() != installer::kLnkExt)
    return false;

  FilePath target_path;
  if (!base::win::ResolveShortcut(path, &target_path, command_line))
    return false;
  // One of the paths may be in short (elided) form. Compare long paths to
  // ensure these are still properly matched.
  return ConvertToLongPath(target_path) == ConvertToLongPath(chrome_exe);
}

// Populates |paths| with the file paths of Chrome desktop shortcuts that have
// the specified |command_line|. If |include_empty_command_lines| is true,
// Chrome desktop shortcuts with empty command lines will also be included.
void ListDesktopShortcutsWithCommandLine(const FilePath& chrome_exe,
                                         const string16& command_line,
                                         bool include_empty_command_lines,
                                         std::vector<FilePath>* paths) {
  FilePath shortcuts_directory;
  if (!GetDesktopShortcutsDirectory(&shortcuts_directory))
    return;

  file_util::FileEnumerator enumerator(shortcuts_directory, false,
      file_util::FileEnumerator::FILES);
  for (FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    string16 shortcut_command_line;
    if (!IsChromeShortcut(path, chrome_exe, &shortcut_command_line))
      continue;

    // TODO(asvitkine): Change this to build a CommandLine object and ensure all
    // args from |command_line| are present in the shortcut's CommandLine. This
    // will be more robust when |command_line| contains multiple args.
    if ((shortcut_command_line.empty() && include_empty_command_lines) ||
        (shortcut_command_line.find(command_line) != string16::npos)) {
      paths->push_back(path);
    }
  }
}

// Renames an existing Chrome desktop profile shortcut. Must be called on the
// FILE thread.
void RenameChromeDesktopShortcutForProfile(
    const string16& old_shortcut_file,
    const string16& new_shortcut_file) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  FilePath shortcuts_directory;
  if (!GetDesktopShortcutsDirectory(&shortcuts_directory))
    return;

  FilePath old_shortcut_path = shortcuts_directory.Append(old_shortcut_file);
  // If the shortcut does not exist, it may have been renamed by the user. In
  // that case, its name should not be changed.
  if (!file_util::PathExists(old_shortcut_path))
    return;

  FilePath new_shortcut_path = shortcuts_directory.Append(new_shortcut_file);
  if (!file_util::Move(old_shortcut_path, new_shortcut_path))
    LOG(ERROR) << "Could not rename Windows profile desktop shortcut.";
}

// Updates all desktop shortcuts for the given profile to have the specified
// parameters. If |create_mode| is CREATE_WHEN_NONE_FOUND, a new shortcut is
// created if no existing ones were found. Whether non-profile shortcuts should
// be updated is specified by |action|. Must be called on the FILE thread.
void CreateOrUpdateDesktopShortcutsForProfile(
    const FilePath& profile_path,
    const string16& profile_name,
    const SkBitmap& avatar_image,
    ProfileShortcutManagerWin::CreateOrUpdateMode create_mode,
    ProfileShortcutManagerWin::NonProfileShortcutAction action) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  FilePath chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe)) {
    NOTREACHED();
    return;
  }

  BrowserDistribution* distribution = BrowserDistribution::GetDistribution();
  installer::Product product(distribution);

  ShellUtil::ShortcutProperties properties(ShellUtil::CURRENT_USER);
  product.AddDefaultShortcutProperties(chrome_exe, &properties);
  const FilePath shortcut_icon =
      CreateChromeDesktopShortcutIconForProfile(profile_path, avatar_image);
  if (!shortcut_icon.empty())
    properties.set_icon(shortcut_icon, 0);

  const string16 command_line =
      profiles::internal::CreateProfileShortcutFlags(profile_path);
  properties.set_arguments(command_line);

  ShellUtil::ShortcutOperation operation =
      ShellUtil::SHELL_SHORTCUT_REPLACE_EXISTING;

  std::vector<FilePath> shortcuts;
  ListDesktopShortcutsWithCommandLine(chrome_exe, command_line,
      action == ProfileShortcutManagerWin::UPDATE_NON_PROFILE_SHORTCUTS,
      &shortcuts);
  if (create_mode == ProfileShortcutManagerWin::CREATE_WHEN_NONE_FOUND &&
      shortcuts.empty()) {
    const string16 shortcut_name =
        profiles::internal::GetShortcutFilenameForProfile(profile_name,
                                                          distribution);
    shortcuts.push_back(FilePath(shortcut_name));
    operation = ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS;
  }

  for (size_t i = 0; i < shortcuts.size(); ++i) {
    const FilePath shortcut_name = shortcuts[i].BaseName().RemoveExtension();
    properties.set_shortcut_name(shortcut_name.value());
    ShellUtil::CreateOrUpdateShortcut(ShellUtil::SHORTCUT_LOCATION_DESKTOP,
        distribution, properties, operation);
  }
}

// Deletes all desktop shortcuts for the specified profile and also removes the
// corresponding icon file. Must be called on the FILE thread.
void DeleteDesktopShortcutsAndIconFile(const FilePath& profile_path,
                                       const FilePath& icon_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  FilePath chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe)) {
    NOTREACHED();
    return;
  }

  const string16 command_line =
      profiles::internal::CreateProfileShortcutFlags(profile_path);
  std::vector<FilePath> shortcuts;
  ListDesktopShortcutsWithCommandLine(chrome_exe, command_line, false,
                                      &shortcuts);

  BrowserDistribution* distribution = BrowserDistribution::GetDistribution();
  for (size_t i = 0; i < shortcuts.size(); ++i) {
    const string16 shortcut_name =
        shortcuts[i].BaseName().RemoveExtension().value();
    ShellUtil::RemoveShortcut(ShellUtil::SHORTCUT_LOCATION_DESKTOP,
                              distribution, chrome_exe.value(),
                              ShellUtil::CURRENT_USER,
                              &shortcut_name);
  }

  file_util::Delete(icon_path, false);
}

}  // namespace

namespace profiles {
namespace internal {

const char kProfileIconFileName[] = "Google Profile.ico";

string16 GetShortcutFilenameForProfile(const string16& profile_name,
                                       BrowserDistribution* distribution) {
  string16 shortcut_name;
  if (!profile_name.empty()) {
    shortcut_name.append(profile_name);
    shortcut_name.append(L" - ");
  }
  shortcut_name.append(distribution->GetAppShortCutName());
  return shortcut_name + installer::kLnkExt;
}

string16 CreateProfileShortcutFlags(const FilePath& profile_path) {
  return base::StringPrintf(L"--%ls=\"%ls\"",
                            ASCIIToUTF16(switches::kProfileDirectory).c_str(),
                            profile_path.BaseName().value().c_str());
}

}  // namespace internal
}  // namespace profiles

// static
bool ProfileShortcutManager::IsFeatureEnabled() {
  return false;
}

// static
ProfileShortcutManager* ProfileShortcutManager::Create(
    ProfileManager* manager) {
  return new ProfileShortcutManagerWin(manager);
}

ProfileShortcutManagerWin::ProfileShortcutManagerWin(ProfileManager* manager)
    : profile_manager_(manager) {
  profile_manager_->GetProfileInfoCache().AddObserver(this);
}

ProfileShortcutManagerWin::~ProfileShortcutManagerWin() {
  profile_manager_->GetProfileInfoCache().RemoveObserver(this);
}

void ProfileShortcutManagerWin::CreateProfileShortcut(
    const FilePath& profile_path) {
  CreateOrUpdateShortcutsForProfileAtPath(profile_path, CREATE_WHEN_NONE_FOUND,
                                          IGNORE_NON_PROFILE_SHORTCUTS);
}

void ProfileShortcutManagerWin::OnProfileAdded(const FilePath& profile_path) {
  const size_t profile_count =
      profile_manager_->GetProfileInfoCache().GetNumberOfProfiles();
  if (profile_count == 1) {
    CreateOrUpdateShortcutsForProfileAtPath(profile_path,
                                            CREATE_WHEN_NONE_FOUND,
                                            UPDATE_NON_PROFILE_SHORTCUTS);
  } else if (profile_count == 2) {
    CreateOrUpdateShortcutsForProfileAtPath(GetOtherProfilePath(profile_path),
                                            UPDATE_EXISTING_ONLY,
                                            UPDATE_NON_PROFILE_SHORTCUTS);
  }
}

void ProfileShortcutManagerWin::OnProfileWillBeRemoved(
    const FilePath& profile_path) {
}

void ProfileShortcutManagerWin::OnProfileWasRemoved(
    const FilePath& profile_path,
    const string16& profile_name) {
  const ProfileInfoCache& cache = profile_manager_->GetProfileInfoCache();
  // If there is only one profile remaining, remove the badging information
  // from an existing shortcut.
  if (cache.GetNumberOfProfiles() == 1) {
    CreateOrUpdateShortcutsForProfileAtPath(cache.GetPathOfProfileAtIndex(0),
                                            UPDATE_EXISTING_ONLY,
                                            IGNORE_NON_PROFILE_SHORTCUTS);
  }

  const FilePath icon_path =
      profile_path.AppendASCII(profiles::internal::kProfileIconFileName);
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                          base::Bind(&DeleteDesktopShortcutsAndIconFile,
                                     profile_path, icon_path));
}

void ProfileShortcutManagerWin::OnProfileNameChanged(
    const FilePath& profile_path,
    const string16& old_profile_name) {
  CreateOrUpdateShortcutsForProfileAtPath(profile_path, UPDATE_EXISTING_ONLY,
                                          IGNORE_NON_PROFILE_SHORTCUTS);
}

void ProfileShortcutManagerWin::OnProfileAvatarChanged(
    const FilePath& profile_path) {
  CreateOrUpdateShortcutsForProfileAtPath(profile_path, UPDATE_EXISTING_ONLY,
                                          IGNORE_NON_PROFILE_SHORTCUTS);
}

void ProfileShortcutManagerWin::StartProfileShortcutNameChange(
    const FilePath& profile_path,
    const string16& old_profile_name) {
  const ProfileInfoCache& cache = profile_manager_->GetProfileInfoCache();
  size_t profile_index = cache.GetIndexOfProfileWithPath(profile_path);
  if (profile_index == std::string::npos)
    return;
  // If the shortcut will have an appended name, get the profile name.
  string16 new_profile_name;
  if (cache.GetNumberOfProfiles() != 1)
    new_profile_name = cache.GetNameOfProfileAtIndex(profile_index);

  BrowserDistribution* distribution = BrowserDistribution::GetDistribution();
  const string16 old_shortcut_file =
      profiles::internal::GetShortcutFilenameForProfile(old_profile_name,
                                                        distribution);
  const string16 new_shortcut_file =
      profiles::internal::GetShortcutFilenameForProfile(new_profile_name,
                                                        distribution);
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&RenameChromeDesktopShortcutForProfile,
                 old_shortcut_file,
                 new_shortcut_file));
}

FilePath ProfileShortcutManagerWin::GetOtherProfilePath(
    const FilePath& profile_path) {
  const ProfileInfoCache& cache = profile_manager_->GetProfileInfoCache();
  DCHECK_EQ(2U, cache.GetNumberOfProfiles());
  // Get the index of the current profile, in order to find the index of the
  // other profile.
  size_t current_profile_index = cache.GetIndexOfProfileWithPath(profile_path);
  size_t other_profile_index = (current_profile_index == 0) ? 1 : 0;
  return profile_manager_->GetProfileInfoCache().
      GetPathOfProfileAtIndex(other_profile_index);
}

void ProfileShortcutManagerWin::CreateOrUpdateShortcutsForProfileAtPath(
    const FilePath& profile_path,
    CreateOrUpdateMode create_mode,
    NonProfileShortcutAction action) {
  ProfileInfoCache* cache = &profile_manager_->GetProfileInfoCache();
  size_t profile_index = cache->GetIndexOfProfileWithPath(profile_path);
  if (profile_index == std::string::npos)
    return;
  bool remove_badging = cache->GetNumberOfProfiles() == 1;

  string16 old_shortcut_appended_name =
      cache->GetShortcutNameOfProfileAtIndex(profile_index);

  string16 new_shortcut_appended_name;
  if (!remove_badging)
    new_shortcut_appended_name = cache->GetNameOfProfileAtIndex(profile_index);

  if (create_mode == UPDATE_EXISTING_ONLY &&
      new_shortcut_appended_name != old_shortcut_appended_name) {
    // TODO(asvitkine): Fold this into |UpdateDesktopShortcutsForProfile()|.
    StartProfileShortcutNameChange(profile_path, old_shortcut_appended_name);
  }

  SkBitmap profile_avatar_bitmap_copy;
  if (!remove_badging) {
    size_t profile_icon_index =
        cache->GetAvatarIconIndexOfProfileAtIndex(profile_index);
    gfx::Image profile_avatar_image = ResourceBundle::GetSharedInstance().
        GetNativeImageNamed(
            cache->GetDefaultAvatarIconResourceIDAtIndex(profile_icon_index));

    DCHECK(!profile_avatar_image.IsEmpty());
    const SkBitmap* profile_avatar_bitmap = profile_avatar_image.ToSkBitmap();
    // Make a copy of the SkBitmap to ensure that we can safely use the image
    // data on the FILE thread.
    profile_avatar_bitmap->deepCopyTo(&profile_avatar_bitmap_copy,
                                      profile_avatar_bitmap->getConfig());
  }
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&CreateOrUpdateDesktopShortcutsForProfile,
                 profile_path, new_shortcut_appended_name,
                 profile_avatar_bitmap_copy, create_mode, action));

  cache->SetShortcutNameOfProfileAtIndex(profile_index,
                                         new_shortcut_appended_name);
}
