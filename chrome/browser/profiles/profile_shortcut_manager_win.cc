// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_shortcut_manager.h"

#include <map>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/app_icon_win.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile_info_cache_observer.h"
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

using content::BrowserThread;

namespace {

const char kProfileIconFileName[] = "Google Profile.ico";
const int kProfileAvatarShortcutBadgeWidth = 28;
const int kProfileAvatarShortcutBadgeHeight = 28;
const int kShortcutIconSize = 48;

// Creates a desktop shortcut icon file (.ico) on the disk for a given profile,
// badging the browser distribution icon with the profile avatar.
// |profile_base_dir| is the base directory (and key) of the profile. Returns
// a path to the shortcut icon file on disk, which is empty if this fails.
// Use index 0 when assigning the resulting file as the icon.
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

  // TODO(hallielaine): Share this chunk of code with
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
  FilePath icon_path = profile_path.AppendASCII(kProfileIconFileName);
  if (!IconUtil::CreateIconFileFromSkBitmap(final_bitmap, icon_path))
    return FilePath();

  return icon_path;
}

string16 CreateProfileShortcutFlags(const FilePath& profile_path) {
    return base::StringPrintf(L"--%ls=\"%ls\"",
      ASCIIToUTF16(switches::kProfileDirectory).c_str(),
      profile_path.BaseName().value().c_str());
}

// Wrap a ShellUtil/FileUtil function that returns a bool so it can be posted
// in a task to the FILE thread.
void CallBoolFunction(
    const base::Callback<bool(void)>& bool_function) {
  bool_function.Run();
}

// Renames an existing Chrome desktop profile shortcut. Must be called on the
// FILE thread.
void RenameChromeDesktopShortcutForProfile(
    const string16& old_shortcut_file,
    const string16& new_shortcut_file) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  FilePath shortcut_path;
  if (ShellUtil::GetDesktopPath(false,  // User's directory instead of system.
                                &shortcut_path)) {
    FilePath old_shortcut_path = shortcut_path.Append(old_shortcut_file);
    FilePath new_shortcut_path = shortcut_path.Append(new_shortcut_file);
    if (!file_util::Move(old_shortcut_path, new_shortcut_path))
       LOG(ERROR) << "Could not rename Windows profile desktop shortcut.";
  }
}

// Create or update a profile desktop shortcut. Must be called on the FILE
// thread.
void CreateOrUpdateProfileDesktopShortcut(
    const FilePath& profile_path,
    const string16& profile_name,
    const SkBitmap& avatar_image,
    bool create) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  FilePath shortcut_icon = CreateChromeDesktopShortcutIconForProfile(
    profile_path, avatar_image);

  FilePath chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe))
    return;
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  if (!dist)
    return;
  string16 description(dist->GetAppDescription());

  uint32 options = create ? ShellUtil::SHORTCUT_CREATE_ALWAYS :
                            ShellUtil::SHORTCUT_NO_OPTIONS;
  ShellUtil::CreateChromeDesktopShortcut(
      dist,
      chrome_exe.value(),
      description,
      profile_name,
      CreateProfileShortcutFlags(profile_path),
      shortcut_icon.empty() ? chrome_exe.value() : shortcut_icon.value(),
      shortcut_icon.empty() ? dist->GetIconIndex() : 0,
      ShellUtil::CURRENT_USER,
      options);
}

}  // namespace

class ProfileShortcutManagerWin : public ProfileShortcutManager,
                                  public ProfileInfoCacheObserver {
 public:
  explicit ProfileShortcutManagerWin(ProfileInfoCache& cache);
  virtual ~ProfileShortcutManagerWin();

  virtual void StartProfileDesktopShortcutCreation(
      const FilePath& profile_path, const string16& profile_name,
      const gfx::Image& avatar_image) OVERRIDE;
  virtual void DeleteProfileDesktopShortcut(const FilePath& profile_path,
      const string16& profile_name) OVERRIDE;

  virtual void OnProfileAdded(const FilePath& profile_path) OVERRIDE;
  virtual void OnProfileWillBeRemoved(const FilePath& profile_path) OVERRIDE;
  virtual void OnProfileWasRemoved(const FilePath& profile_path,
                                   const string16& profile_name) OVERRIDE;
  virtual void OnProfileNameChanged(const FilePath& profile_path,
                                    const string16& old_profile_name) OVERRIDE;
  virtual void OnProfileAvatarChanged(const FilePath& profile_path) OVERRIDE;

 private:
  ProfileInfoCache& profile_info_cache_;
};

// static
bool ProfileShortcutManager::IsFeatureEnabled() {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kProfileDesktopShortcuts)) {
    return true;
  }
  return false;
}

// static
ProfileShortcutManager* ProfileShortcutManager::Create(
    ProfileInfoCache& cache) {
  return new ProfileShortcutManagerWin(cache);
}

ProfileShortcutManagerWin::ProfileShortcutManagerWin(ProfileInfoCache& cache)
    : profile_info_cache_(cache) {
    cache.AddObserver(this);
}

ProfileShortcutManagerWin::~ProfileShortcutManagerWin() {
    profile_info_cache_.RemoveObserver(this);
}

void ProfileShortcutManagerWin::StartProfileDesktopShortcutCreation(
    const FilePath& profile_path, const string16& profile_name,
    const gfx::Image& avatar_image) {
  DCHECK(!avatar_image.IsEmpty());
  const SkBitmap* avatar_bitmap = avatar_image.ToSkBitmap();

  // Make a copy of the SkBitmap to ensure that we can safely use the image
  // data on the FILE thread.
  SkBitmap avatar_bitmap_copy;
  DCHECK(avatar_bitmap->deepCopyTo(&avatar_bitmap_copy,
                                avatar_bitmap->getConfig()));
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&CreateOrUpdateProfileDesktopShortcut,
                 profile_path, profile_name, avatar_bitmap_copy, true));
}

void ProfileShortcutManagerWin::DeleteProfileDesktopShortcut(
    const FilePath& profile_path, const string16& profile_name) {
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  string16 shortcut;
  // If we can find the shortcut, delete it
  if (ShellUtil::GetChromeShortcutName(dist, false,
                                       profile_name, &shortcut)) {
    std::vector<string16> appended_names(1, shortcut);
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
            base::Bind(&CallBoolFunction, base::Bind(
                &ShellUtil::RemoveChromeDesktopShortcutsWithAppendedNames,
                appended_names)));
  }
  FilePath icon_path = profile_path.AppendASCII(kProfileIconFileName);
  BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
            base::Bind(&CallBoolFunction, base::Bind(
                &file_util::Delete, icon_path, false)));
}

void ProfileShortcutManagerWin::OnProfileAdded(const FilePath& profile_path) {
}

void ProfileShortcutManagerWin::OnProfileWillBeRemoved(
    const FilePath& profile_path) {
}

void ProfileShortcutManagerWin::OnProfileWasRemoved(
    const FilePath& profile_path,
    const string16& profile_name) {
}

void ProfileShortcutManagerWin::OnProfileNameChanged(
    const FilePath& profile_path,
    const string16& old_profile_name) {
  size_t profile_index = profile_info_cache_.GetIndexOfProfileWithPath(
      profile_path);
  if (profile_index == std::string::npos)
      return;

  string16 new_profile_name =
      profile_info_cache_.GetNameOfProfileAtIndex(profile_index);

  string16 old_shortcut_file;
  string16 new_shortcut_file;
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  if (ShellUtil::GetChromeShortcutName(
          dist, false, old_profile_name, &old_shortcut_file) &&
      ShellUtil::GetChromeShortcutName(
          dist, false, new_profile_name, &new_shortcut_file)) {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&RenameChromeDesktopShortcutForProfile,
                   old_shortcut_file,
                   new_shortcut_file));
  }
}

void ProfileShortcutManagerWin::OnProfileAvatarChanged(
    const FilePath& profile_path) {
  size_t profile_index = profile_info_cache_.GetIndexOfProfileWithPath(
      profile_path);
  if (profile_index == std::string::npos)
      return;

  string16 profile_name =
      profile_info_cache_.GetNameOfProfileAtIndex(profile_index);
  string16 shortcut;

  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  if (ShellUtil::GetChromeShortcutName(
         dist, false,  profile_name, &shortcut)) {
    size_t new_icon_index = profile_info_cache_.
        GetAvatarIconIndexOfProfileAtIndex(profile_index);
    gfx::Image avatar_image =
         ResourceBundle::GetSharedInstance().GetNativeImageNamed(
             profile_info_cache_.GetDefaultAvatarIconResourceIDAtIndex(
                 new_icon_index));

    DCHECK(!avatar_image.IsEmpty());
    const SkBitmap* avatar_bitmap = avatar_image.ToSkBitmap();

    // Make a copy of the SkBitmap to ensure that we can safely use the image
    // data on the FILE thread.
    SkBitmap avatar_bitmap_copy;
    DCHECK(avatar_bitmap->deepCopyTo(&avatar_bitmap_copy,
                                     avatar_bitmap->getConfig()));
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&CreateOrUpdateProfileDesktopShortcut,
                   profile_path, profile_name, avatar_bitmap_copy, false));
  }
}

