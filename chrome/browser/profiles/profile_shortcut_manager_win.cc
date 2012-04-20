// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_shortcut_manager_win.h"

#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/app_icon_win.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_info_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/installer/util/auto_launch_util.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/shell_util.h"
#include "content/public/browser/browser_thread.h"
#include "grit/generated_resources.h"
#include "skia/ext/image_operations.h"
#include "skia/ext/platform_canvas.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/icon_util.h"
#include "ui/gfx/image/image.h"

using content::BrowserThread;

namespace {

const char kProfileIconFileName[] = "Google Profile.ico";
const int kProfileAvatarShortcutBadgeWidth = 28;
const int kProfileAvatarShortcutBadgeHeight = 28;
const int kShortcutIconSize = 48;

// Creates the argument to pass to the Windows executable that launches Chrome
// with the profile in |profile_base_dir|.
// For example: --profile-directory="Profile 2"
string16 CreateProfileShortcutSwitch(const string16& profile_base_dir) {
  return base::StringPrintf(L"--%ls=\"%ls\"",
      ASCIIToUTF16(switches::kProfileDirectory).c_str(),
      profile_base_dir.c_str());
}

// Wrap a ShellUtil function that returns a bool so it can be posted in a
// task to the FILE thread.
void CallShellUtilBoolFunction(
    const base::Callback<bool(void)>& bool_function) {
  bool_function.Run();
}

// Creates a desktop shortcut icon file (.ico) on the disk for a given profile,
// badging the browser distribution icon with the profile avatar.
// |profile_base_dir| is the base directory (and key) of the profile. Returns
// a path to the shortcut icon file on disk, which is empty if this fails.
// Use index 0 when assigning the resulting file as the icon.
FilePath CreateChromeDesktopShortcutIconForProfile(
    const FilePath& profile_path,
    const gfx::Image& avatar_image) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  const SkBitmap* avatar_bitmap = avatar_image.ToSkBitmap();
  HICON app_icon_handle = GetAppIconForSize(kShortcutIconSize);
  scoped_ptr<SkBitmap> app_icon_bitmap(
      IconUtil::CreateSkBitmapFromHICON(app_icon_handle));
  DestroyIcon(app_icon_handle);
  if (!app_icon_bitmap.get())
    return FilePath();

  // TODO(stevet): Share this chunk of code with
  // avatar_menu_button::DrawTaskBarDecoration.
  const SkBitmap* source_bitmap = NULL;
  SkBitmap squarer_bitmap;
  if ((avatar_bitmap->width() == profiles::kAvatarIconWidth) &&
      (avatar_bitmap->height() == profiles::kAvatarIconHeight)) {
    // Shave a couple of columns so the bitmap is more square. So when
    // resized to a square aspect ratio it looks pretty.
    int x = 2;
    avatar_bitmap->extractSubset(&squarer_bitmap, SkIRect::MakeXYWH(x, 0,
        profiles::kAvatarIconWidth - x * 2, profiles::kAvatarIconHeight));
    source_bitmap = &squarer_bitmap;
  } else {
    source_bitmap = avatar_bitmap;
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
  FilePath icon_path = profile_path.AppendASCII(kProfileIconFileName);
  if (!IconUtil::CreateIconFileFromSkBitmap(final_bitmap, icon_path))
    return FilePath();

  return icon_path;
}

// Creates a desktop shortcut to open Chrome with the given profile name and
// base directory. Iff |create|, create shortcut if it doesn't already exist.
// Must be called on the FILE thread.
void CreateChromeDesktopShortcutForProfile(
    const string16& profile_name,
    const string16& profile_base_dir,
    const FilePath& profile_path,
    const gfx::Image* avatar_image,
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
  const string16& directory = CreateProfileShortcutSwitch(profile_base_dir);
  FilePath icon_path = avatar_image ?
      CreateChromeDesktopShortcutIconForProfile(profile_path, *avatar_image) :
      FilePath();

  ShellUtil::CreateChromeDesktopShortcut(
      dist,
      chrome_exe.value(),
      description,
      profile_name,
      directory,
      icon_path.empty() ? chrome_exe.value() : icon_path.value(),
      icon_path.empty() ? dist->GetIconIndex() : 0,
      ShellUtil::CURRENT_USER,
      create ? ShellUtil::SHORTCUT_CREATE_ALWAYS :
               ShellUtil::SHORTCUT_NO_OPTIONS);
}

// Renames an existing Chrome desktop profile shortcut. Must be called on the
// FILE thread.
void RenameChromeDesktopShortcutForProfile(
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

// Updates the arguments to a Chrome desktop shortcut for a profile. Must be
// called on the FILE thread.
void UpdateChromeDesktopShortcutForProfile(
    const string16& shortcut,
    const string16& arguments,
    const FilePath& profile_path,
    const gfx::Image* avatar_image) {
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
  FilePath icon_path = avatar_image ?
      CreateChromeDesktopShortcutIconForProfile(profile_path, *avatar_image) :
      FilePath();

  ShellUtil::UpdateChromeShortcut(
      dist,
      chrome_exe.value(),
      shortcut_path.value(),
      arguments,
      description,
      icon_path.empty() ? chrome_exe.value() : icon_path.value(),
      icon_path.empty() ? dist->GetIconIndex() : 0,
      ShellUtil::SHORTCUT_NO_OPTIONS);
}

void DeleteAutoLaunchValueForProfile(
    const FilePath& profile_path) {
  if (auto_launch_util::AutoStartRequested(profile_path.BaseName().value(),
                                           true,  // Window requested.
                                           FilePath())) {
    auto_launch_util::DisableForegroundStartAtLogin(
        profile_path.BaseName().value());
  }
}

}  // namespace

ProfileShortcutManagerWin::ProfileShortcutManagerWin() {
}

ProfileShortcutManagerWin::~ProfileShortcutManagerWin() {
}

void ProfileShortcutManagerWin::AddProfileShortcut(
    const FilePath& profile_path) {
  ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  size_t index = cache.GetIndexOfProfileWithPath(profile_path);
  if (index == std::string::npos)
    return;

  // Launch task to add shortcut to desktop on Windows. If this is the very
  // first profile created, don't add the user name to the shortcut.
  // TODO(mirandac): respect master_preferences choice to create no shortcuts
  // (see http://crbug.com/104463)
  if (g_browser_process->profile_manager()->GetNumberOfProfiles() > 1) {
    {
      // We make a copy of the Image to ensure that the underlying image data is
      // AddRef'd, in case the original copy gets deleted.
      gfx::Image* avatar_copy =
          new gfx::Image(cache.GetAvatarIconOfProfileAtIndex(index));
      string16 profile_name = cache.GetNameOfProfileAtIndex(index);
      string16 profile_base_dir = profile_path.BaseName().value();
      BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
          base::Bind(&CreateChromeDesktopShortcutForProfile,
                     profile_name, profile_base_dir, profile_path,
                     base::Owned(avatar_copy), true));
    }

    // If this is the second existing multi-user account created, change the
    // original shortcut use the first profile's details (name, badge,
    // argument).
    if (cache.GetNumberOfProfiles() == 2) {
      // Get the index of the first profile, based on the index of the second
      // profile. It's either 0 or 1, whichever the second profile isn't.
      size_t first_index = 0;
      if (cache.GetIndexOfProfileWithPath(profile_path) == 0)
        first_index = 1;
      string16 first_name = cache.GetNameOfProfileAtIndex(first_index);
      BrowserDistribution* dist = BrowserDistribution::GetDistribution();

      string16 old_shortcut;
      string16 new_shortcut;
      if (ShellUtil::GetChromeShortcutName(dist, false, L"", &old_shortcut) &&
          ShellUtil::GetChromeShortcutName(dist, false, first_name,
                                           &new_shortcut)) {
        // Update doesn't allow changing the target, so rename first.
        BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
            base::Bind(&RenameChromeDesktopShortcutForProfile,
                       old_shortcut, new_shortcut));

        // Fetch the avatar for the first profile and make a copy of the Image
        // to ensure that the underlying image data is AddRef'd, in case the
        // original copy is deleted.
        gfx::Image& first_avatar =
            ResourceBundle::GetSharedInstance().GetNativeImageNamed(
                ProfileInfoCache::GetDefaultAvatarIconResourceIDAtIndex(
                    cache.GetAvatarIconIndexOfProfileAtIndex(first_index)));
        gfx::Image* first_avatar_copy = new gfx::Image(first_avatar);
        FilePath first_path = cache.GetPathOfProfileAtIndex(first_index);

        BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
            base::Bind(&UpdateChromeDesktopShortcutForProfile,
                       new_shortcut,
                       CreateProfileShortcutSwitch(UTF8ToUTF16(
                           first_path.BaseName().MaybeAsASCII())),
                       first_path,
                       base::Owned(first_avatar_copy)));
      }
    }
  } else {  // Only one profile, so create original shortcut, with no avatar.
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
        base::Bind(&CreateChromeDesktopShortcutForProfile,
                   string16(), string16(), FilePath(),
                   static_cast<gfx::Image*>(NULL), true));
  }
}

void ProfileShortcutManagerWin::OnProfileAdded(
    const FilePath& profile_path) {
}

void ProfileShortcutManagerWin::OnProfileWillBeRemoved(
    const FilePath& profile_path) {
  ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  size_t index = cache.GetIndexOfProfileWithPath(profile_path);
  if (index == std::string::npos)
    return;
  string16 profile_name = cache.GetNameOfProfileAtIndex(index);
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

void ProfileShortcutManagerWin::OnProfileWasRemoved(
    const FilePath& profile_path,
    const string16& profile_name) {
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&DeleteAutoLaunchValueForProfile, profile_path));

  // If there is one profile left, we want to remove the badge and name from it.
  ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  if (cache.GetNumberOfProfiles() != 1)
    return;

  FilePath last_profile_path = cache.GetPathOfProfileAtIndex(0);
  string16 old_shortcut;
  string16 new_shortcut;
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  if (ShellUtil::GetChromeShortcutName(
          dist, false, cache.GetNameOfProfileAtIndex(0), &old_shortcut) &&
      ShellUtil::GetChromeShortcutName(
          dist, false, L"", &new_shortcut)) {
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
        base::Bind(&RenameChromeDesktopShortcutForProfile,
                   old_shortcut,
                   new_shortcut));
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
        base::Bind(&UpdateChromeDesktopShortcutForProfile,
                   new_shortcut,
                   CreateProfileShortcutSwitch(UTF8ToUTF16(
                       last_profile_path.BaseName().MaybeAsASCII())),
                   last_profile_path,
                   static_cast<gfx::Image*>(NULL)));
  }
}

void ProfileShortcutManagerWin::OnProfileNameChanged(
    const FilePath& profile_path,
    const string16& old_profile_name) {
  // Launch task to change name of desktop shortcut on Windows.
  // TODO(mirandac): respect master_preferences choice to create no shortcuts
  // (see http://crbug.com/104463)
  ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  size_t index = cache.GetIndexOfProfileWithPath(profile_path);
  if (index == std::string::npos)
    return;
  string16 new_profile_name = cache.GetNameOfProfileAtIndex(index);

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

void ProfileShortcutManagerWin::OnProfileAvatarChanged(
      const FilePath& profile_path) {
  ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  size_t index = cache.GetIndexOfProfileWithPath(profile_path);
  if (index == std::string::npos)
    return;
  string16 profile_name = cache.GetNameOfProfileAtIndex(index);
  string16 profile_base_dir =
      UTF8ToUTF16(profile_path.BaseName().MaybeAsASCII());
  const gfx::Image& avatar_image = cache.GetAvatarIconOfProfileAtIndex(index);

  // Launch task to change the icon of the desktop shortcut on windows.
  string16 new_shortcut;
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  if (ShellUtil::GetChromeShortcutName(dist, false, profile_name,
                                       &new_shortcut)) {
    // We make a copy of the Image to ensure that the underlying image data is
    // AddRef'd, in case the original copy gets deleted.
    gfx::Image* avatar_copy = new gfx::Image(avatar_image);
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
        base::Bind(&UpdateChromeDesktopShortcutForProfile,
                   new_shortcut,
                   CreateProfileShortcutSwitch(profile_base_dir),
                   profile_path,
                   base::Owned(avatar_copy)));
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
