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
    const gfx::Image& avatar_image) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  const SkBitmap* avatar_bitmap = avatar_image.ToSkBitmap();
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

string16 CreateProfileShortcutFlags(const FilePath& profile_path) {
    return base::StringPrintf(L"--%ls=\"%ls\"",
      ASCIIToUTF16(switches::kProfileDirectory).c_str(),
      profile_path.BaseName().value().c_str());
}

// Wrap a ShellUtil function that returns a bool so it can be posted in a
// task to the FILE thread.
void CallShellUtilBoolFunction(
    const base::Callback<bool(void)>& bool_function) {
  bool_function.Run();
}

}  // namespace

class ProfileShortcutManagerWin : public ProfileShortcutManager {
 public:
  ProfileShortcutManagerWin();
  virtual ~ProfileShortcutManagerWin();

  virtual void CreateChromeDesktopShortcut(
      const FilePath& profile_path, const string16& profile_name,
      const gfx::Image& avatar_image) OVERRIDE;
  virtual void DeleteChromeDesktopShortcut(const FilePath& profile_path)
      OVERRIDE;

 private:
  struct ProfileShortcutInfo {
    string16 flags;
    string16 profile_name;
    gfx::Image avatar_image;

    ProfileShortcutInfo()
        : flags(string16()),
          profile_name(string16()),
          avatar_image(gfx::Image()) {
    }

    ProfileShortcutInfo(
        string16 p_flags,
        string16 p_profile_name,
        gfx::Image p_avatar_image)
        : flags(p_flags),
          profile_name(p_profile_name),
          avatar_image(p_avatar_image) {
    }
  };

  // TODO(hallielaine): Repopulate this map on chrome session startup
  typedef std::map<FilePath, ProfileShortcutInfo> ProfileShortcutsMap;
  ProfileShortcutsMap profile_shortcuts_;
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
ProfileShortcutManager* ProfileShortcutManager::Create() {
  return new ProfileShortcutManagerWin();
}

ProfileShortcutManagerWin::ProfileShortcutManagerWin() {
}

ProfileShortcutManagerWin::~ProfileShortcutManagerWin() {
}

void ProfileShortcutManagerWin::CreateChromeDesktopShortcut(
    const FilePath& profile_path, const string16& profile_name,
    const gfx::Image& avatar_image) {
  FilePath shortcut_icon = CreateChromeDesktopShortcutIconForProfile(
      profile_path, avatar_image);
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  FilePath chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe))
    return;
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  string16 description;
  if (!dist)
    return;
  description = WideToUTF16(dist->GetAppDescription());

  // Add the profile to the map if it doesn't exist already
  if (!profile_shortcuts_.count(profile_path)) {
    string16 flags = CreateProfileShortcutFlags(profile_path);
    profile_shortcuts_.insert(std::make_pair(profile_path,
                              ProfileShortcutInfo(flags, profile_name,
                                  avatar_image)));
  }

  ShellUtil::CreateChromeDesktopShortcut(
      dist,
      chrome_exe.value(),
      description,
      profile_shortcuts_[profile_path].profile_name,
      profile_shortcuts_[profile_path].flags,
      shortcut_icon.empty() ? chrome_exe.value() : shortcut_icon.value(),
      shortcut_icon.empty() ? dist->GetIconIndex() : 0,
      ShellUtil::CURRENT_USER,
      ShellUtil::SHORTCUT_CREATE_ALWAYS);
}

void ProfileShortcutManagerWin::DeleteChromeDesktopShortcut(
    const FilePath& profile_path) {
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  string16 shortcut;
  // If we can find the shortcut, delete it
  if (ShellUtil::GetChromeShortcutName(dist, false,
      profile_shortcuts_[profile_path].profile_name, &shortcut)) {
    std::vector<string16> appended_names(1, shortcut);
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
            base::Bind(&CallShellUtilBoolFunction, base::Bind(
                &ShellUtil::RemoveChromeDesktopShortcutsWithAppendedNames,
                appended_names)));
    profile_shortcuts_.erase(profile_path);
  }
}

