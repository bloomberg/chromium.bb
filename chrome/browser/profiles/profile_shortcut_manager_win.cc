// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_shortcut_manager_win.h"

#include <shlobj.h>  // For SHChangeNotify().

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_enumerator.h"
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/shortcut.h"
#include "chrome/browser/app_icon_win.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_info_cache_observer.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/product.h"
#include "chrome/installer/util/shell_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "grit/chrome_unscaled_resources.h"
#include "grit/chromium_strings.h"
#include "skia/ext/image_operations.h"
#include "skia/ext/platform_canvas.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/icon_util.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_family.h"


using content::BrowserThread;

namespace {

// Name of the badged icon file generated for a given profile.
const char kProfileIconFileName[] = "Google Profile.ico";

// Characters that are not allowed in Windows filenames. Taken from
// http://msdn.microsoft.com/en-us/library/aa365247.aspx
const base::char16 kReservedCharacters[] = L"<>:\"/\\|?*\x01\x02\x03\x04\x05"
    L"\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F\x10\x11\x12\x13\x14\x15\x16\x17"
    L"\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F";

// The maximum number of characters allowed in profile shortcuts' file names.
// Warning: migration code will be needed if this is changed later, since
// existing shortcuts might no longer be found if the name is generated
// differently than it was when a shortcut was originally created.
const int kMaxProfileShortcutFileNameLength = 64;

// The avatar badge size needs to be half of the shortcut icon size because
// the Windows taskbar icon is 32x32 and the avatar icon overlay is 16x16. So to
// get the shortcut avatar badge and the avatar icon overlay to match up, we
// need to preserve those ratios when creating the shortcut icon.
const int kShortcutIconSize = 48;
const int kProfileAvatarBadgeSize = kShortcutIconSize / 2;

const int kCurrentProfileIconVersion = 3;

// 2x sized profile avatar icons. Mirrors |kDefaultAvatarIconResources| in
// profile_info_cache.cc.
const int kProfileAvatarIconResources2x[] = {
  IDR_PROFILE_AVATAR_2X_0,
  IDR_PROFILE_AVATAR_2X_1,
  IDR_PROFILE_AVATAR_2X_2,
  IDR_PROFILE_AVATAR_2X_3,
  IDR_PROFILE_AVATAR_2X_4,
  IDR_PROFILE_AVATAR_2X_5,
  IDR_PROFILE_AVATAR_2X_6,
  IDR_PROFILE_AVATAR_2X_7,
  IDR_PROFILE_AVATAR_2X_8,
  IDR_PROFILE_AVATAR_2X_9,
  IDR_PROFILE_AVATAR_2X_10,
  IDR_PROFILE_AVATAR_2X_11,
  IDR_PROFILE_AVATAR_2X_12,
  IDR_PROFILE_AVATAR_2X_13,
  IDR_PROFILE_AVATAR_2X_14,
  IDR_PROFILE_AVATAR_2X_15,
  IDR_PROFILE_AVATAR_2X_16,
  IDR_PROFILE_AVATAR_2X_17,
  IDR_PROFILE_AVATAR_2X_18,
  IDR_PROFILE_AVATAR_2X_19,
  IDR_PROFILE_AVATAR_2X_20,
  IDR_PROFILE_AVATAR_2X_21,
  IDR_PROFILE_AVATAR_2X_22,
  IDR_PROFILE_AVATAR_2X_23,
  IDR_PROFILE_AVATAR_2X_24,
  IDR_PROFILE_AVATAR_2X_25,
  IDR_PROFILE_AVATAR_2X_26,
};

// Badges |app_icon_bitmap| with |avatar_bitmap| at the bottom right corner and
// returns the resulting SkBitmap.
SkBitmap BadgeIcon(const SkBitmap& app_icon_bitmap,
                   const SkBitmap& avatar_bitmap,
                   int scale_factor) {
  SkBitmap source_bitmap =
      profiles::GetAvatarIconAsSquare(avatar_bitmap, scale_factor);
  int avatar_badge_size = kProfileAvatarBadgeSize;
  if (app_icon_bitmap.width() != kShortcutIconSize) {
    avatar_badge_size =
        app_icon_bitmap.width() * kProfileAvatarBadgeSize / kShortcutIconSize;
  }
  SkBitmap sk_icon = skia::ImageOperations::Resize(
      source_bitmap, skia::ImageOperations::RESIZE_LANCZOS3, avatar_badge_size,
      source_bitmap.height() * avatar_badge_size / source_bitmap.width());

  // Overlay the avatar on the icon, anchoring it to the bottom-right of the
  // icon.
  SkBitmap badged_bitmap;
  badged_bitmap.allocN32Pixels(app_icon_bitmap.width(),
                               app_icon_bitmap.height());
  SkCanvas offscreen_canvas(badged_bitmap);
  offscreen_canvas.clear(SK_ColorTRANSPARENT);

  offscreen_canvas.drawBitmap(app_icon_bitmap, 0, 0);
  offscreen_canvas.drawBitmap(sk_icon,
                              app_icon_bitmap.width() - sk_icon.width(),
                              app_icon_bitmap.height() - sk_icon.height());
  return badged_bitmap;
}

// Updates the preferences with the current icon version on icon creation
// success.
void OnProfileIconCreateSuccess(base::FilePath profile_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!g_browser_process->profile_manager())
    return;
  Profile* profile =
      g_browser_process->profile_manager()->GetProfileByPath(profile_path);
  if (profile) {
    profile->GetPrefs()->SetInteger(prefs::kProfileIconVersion,
                                    kCurrentProfileIconVersion);
  }
}

// Creates a desktop shortcut icon file (.ico) on the disk for a given profile,
// badging the browser distribution icon with the profile avatar.
// Returns a path to the shortcut icon file on disk, which is empty if this
// fails. Use index 0 when assigning the resulting file as the icon. If both
// given bitmaps are empty, an unbadged icon is created.
// Returns the path to the created icon on success and an empty base::FilePath
// on failure.
// TODO(calamity): Ideally we'd just copy the app icon verbatim from the exe's
// resources in the case of an unbadged icon.
base::FilePath CreateOrUpdateShortcutIconForProfile(
    const base::FilePath& profile_path,
    const SkBitmap& avatar_bitmap_1x,
    const SkBitmap& avatar_bitmap_2x) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  if (!base::PathExists(profile_path)) {
    LOG(ERROR) << "Profile directory " << profile_path.value()
               << " did not exist when trying to create profile icon";
    return base::FilePath();
  }

  scoped_ptr<SkBitmap> app_icon_bitmap(GetAppIconForSize(kShortcutIconSize));
  if (!app_icon_bitmap)
    return base::FilePath();

  gfx::ImageFamily badged_bitmaps;
  if (!avatar_bitmap_1x.empty()) {
    badged_bitmaps.Add(gfx::Image::CreateFrom1xBitmap(
        BadgeIcon(*app_icon_bitmap, avatar_bitmap_1x, 1)));
  }

  scoped_ptr<SkBitmap> large_app_icon_bitmap(
      GetAppIconForSize(IconUtil::kLargeIconSize));
  if (large_app_icon_bitmap && !avatar_bitmap_2x.empty()) {
    badged_bitmaps.Add(gfx::Image::CreateFrom1xBitmap(
        BadgeIcon(*large_app_icon_bitmap, avatar_bitmap_2x, 2)));
  }

  // If we have no badged bitmaps, we should just use the default chrome icon.
  if (badged_bitmaps.empty()) {
    badged_bitmaps.Add(gfx::Image::CreateFrom1xBitmap(*app_icon_bitmap));
    if (large_app_icon_bitmap) {
      badged_bitmaps.Add(
          gfx::Image::CreateFrom1xBitmap(*large_app_icon_bitmap));
    }
  }
  // Finally, write the .ico file containing this new bitmap.
  const base::FilePath icon_path =
      profiles::internal::GetProfileIconPath(profile_path);
  const bool had_icon = base::PathExists(icon_path);

  if (!IconUtil::CreateIconFileFromImageFamily(badged_bitmaps, icon_path)) {
    // This can happen in theory if the profile directory is deleted between the
    // beginning of this function and here; however this is extremely unlikely
    // and this check will help catch any regression where this call would start
    // failing constantly.
    NOTREACHED();
    return base::FilePath();
  }

  if (had_icon) {
    // This invalidates the Windows icon cache and causes the icon changes to
    // register with the taskbar and desktop. SHCNE_ASSOCCHANGED will cause a
    // desktop flash and we would like to avoid that if possible.
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
  } else {
    SHChangeNotify(SHCNE_CREATE, SHCNF_PATH, icon_path.value().c_str(), NULL);
  }
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&OnProfileIconCreateSuccess, profile_path));
  return icon_path;
}

// Gets the user and system directories for desktop shortcuts. Parameters may
// be NULL if a directory type is not needed. Returns true on success.
bool GetDesktopShortcutsDirectories(
    base::FilePath* user_shortcuts_directory,
    base::FilePath* system_shortcuts_directory) {
  BrowserDistribution* distribution = BrowserDistribution::GetDistribution();
  if (user_shortcuts_directory &&
      !ShellUtil::GetShortcutPath(ShellUtil::SHORTCUT_LOCATION_DESKTOP,
                                  distribution, ShellUtil::CURRENT_USER,
                                  user_shortcuts_directory)) {
    NOTREACHED();
    return false;
  }
  if (system_shortcuts_directory &&
      !ShellUtil::GetShortcutPath(ShellUtil::SHORTCUT_LOCATION_DESKTOP,
                                  distribution, ShellUtil::SYSTEM_LEVEL,
                                  system_shortcuts_directory)) {
    NOTREACHED();
    return false;
  }
  return true;
}

// Returns the long form of |path|, which will expand any shortened components
// like "foo~2" to their full names.
base::FilePath ConvertToLongPath(const base::FilePath& path) {
  const size_t length = GetLongPathName(path.value().c_str(), NULL, 0);
  if (length != 0 && length != path.value().length()) {
    std::vector<wchar_t> long_path(length);
    if (GetLongPathName(path.value().c_str(), &long_path[0], length) != 0)
      return base::FilePath(&long_path[0]);
  }
  return path;
}

// Returns true if the file at |path| is a Chrome shortcut and returns its
// command line in output parameter |command_line|.
bool IsChromeShortcut(const base::FilePath& path,
                      const base::FilePath& chrome_exe,
                      base::string16* command_line) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  if (path.Extension() != installer::kLnkExt)
    return false;

  base::FilePath target_path;
  if (!base::win::ResolveShortcut(path, &target_path, command_line))
    return false;
  // One of the paths may be in short (elided) form. Compare long paths to
  // ensure these are still properly matched.
  return ConvertToLongPath(target_path) == ConvertToLongPath(chrome_exe);
}

// Populates |paths| with the file paths of Chrome desktop shortcuts that have
// the specified |command_line|. If |include_empty_command_lines| is true,
// Chrome desktop shortcuts with empty command lines will also be included.
void ListDesktopShortcutsWithCommandLine(const base::FilePath& chrome_exe,
                                         const base::string16& command_line,
                                         bool include_empty_command_lines,
                                         std::vector<base::FilePath>* paths) {
  base::FilePath user_shortcuts_directory;
  if (!GetDesktopShortcutsDirectories(&user_shortcuts_directory, NULL))
    return;

  base::FileEnumerator enumerator(user_shortcuts_directory, false,
                                  base::FileEnumerator::FILES);
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    base::string16 shortcut_command_line;
    if (!IsChromeShortcut(path, chrome_exe, &shortcut_command_line))
      continue;

    // TODO(asvitkine): Change this to build a CommandLine object and ensure all
    // args from |command_line| are present in the shortcut's CommandLine. This
    // will be more robust when |command_line| contains multiple args.
    if ((shortcut_command_line.empty() && include_empty_command_lines) ||
        (shortcut_command_line.find(command_line) != base::string16::npos)) {
      paths->push_back(path);
    }
  }
}

// Renames the given desktop shortcut and informs the shell of this change.
bool RenameDesktopShortcut(const base::FilePath& old_shortcut_path,
                           const base::FilePath& new_shortcut_path) {
  if (!base::Move(old_shortcut_path, new_shortcut_path))
    return false;

  // Notify the shell of the rename, which allows the icon to keep its position
  // on the desktop when renamed. Note: This only works if either SHCNF_FLUSH or
  // SHCNF_FLUSHNOWAIT is specified as a flag.
  SHChangeNotify(SHCNE_RENAMEITEM, SHCNF_PATH | SHCNF_FLUSHNOWAIT,
                 old_shortcut_path.value().c_str(),
                 new_shortcut_path.value().c_str());
  return true;
}

// Renames an existing Chrome desktop profile shortcut. Must be called on the
// FILE thread.
void RenameChromeDesktopShortcutForProfile(
    const base::string16& old_shortcut_filename,
    const base::string16& new_shortcut_filename) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  base::FilePath user_shortcuts_directory;
  base::FilePath system_shortcuts_directory;
  if (!GetDesktopShortcutsDirectories(&user_shortcuts_directory,
                                      &system_shortcuts_directory)) {
    return;
  }

  const base::FilePath old_shortcut_path =
      user_shortcuts_directory.Append(old_shortcut_filename);
  const base::FilePath new_shortcut_path =
      user_shortcuts_directory.Append(new_shortcut_filename);

  if (base::PathExists(old_shortcut_path)) {
    // Rename the old shortcut unless a system-level shortcut exists at the
    // destination, in which case the old shortcut is simply deleted.
    const base::FilePath possible_new_system_shortcut =
        system_shortcuts_directory.Append(new_shortcut_filename);
    if (base::PathExists(possible_new_system_shortcut))
      base::DeleteFile(old_shortcut_path, false);
    else if (!RenameDesktopShortcut(old_shortcut_path, new_shortcut_path))
      DLOG(ERROR) << "Could not rename Windows profile desktop shortcut.";
  } else {
    // If the shortcut does not exist, it may have been renamed by the user. In
    // that case, its name should not be changed.
    // It's also possible that a system-level shortcut exists instead - this
    // should only be the case for the original Chrome shortcut from an
    // installation. If that's the case, copy that one over - it will get its
    // properties updated by
    // |CreateOrUpdateDesktopShortcutsAndIconForProfile()|.
    const base::FilePath possible_old_system_shortcut =
        system_shortcuts_directory.Append(old_shortcut_filename);
    if (base::PathExists(possible_old_system_shortcut))
      base::CopyFile(possible_old_system_shortcut, new_shortcut_path);
  }
}

struct CreateOrUpdateShortcutsParams {
  CreateOrUpdateShortcutsParams(
      base::FilePath profile_path,
      ProfileShortcutManagerWin::CreateOrUpdateMode create_mode,
      ProfileShortcutManagerWin::NonProfileShortcutAction action)
      : profile_path(profile_path), create_mode(create_mode), action(action) {}
  ~CreateOrUpdateShortcutsParams() {}

  ProfileShortcutManagerWin::CreateOrUpdateMode create_mode;
  ProfileShortcutManagerWin::NonProfileShortcutAction action;

  // The path for this profile.
  base::FilePath profile_path;
  // The profile name before this update. Empty on create.
  base::string16 old_profile_name;
  // The new profile name.
  base::string16 profile_name;
  // Avatar images for this profile.
  SkBitmap avatar_image_1x;
  SkBitmap avatar_image_2x;
};

// Updates all desktop shortcuts for the given profile to have the specified
// parameters. If |params.create_mode| is CREATE_WHEN_NONE_FOUND, a new shortcut
// is created if no existing ones were found. Whether non-profile shortcuts
// should be updated is specified by |params.action|. Must be called on the FILE
// thread.
void CreateOrUpdateDesktopShortcutsAndIconForProfile(
    const CreateOrUpdateShortcutsParams& params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  const base::FilePath shortcut_icon =
      CreateOrUpdateShortcutIconForProfile(params.profile_path,
                                           params.avatar_image_1x,
                                           params.avatar_image_2x);
  if (shortcut_icon.empty() ||
      params.create_mode ==
          ProfileShortcutManagerWin::CREATE_OR_UPDATE_ICON_ONLY) {
    return;
  }

  base::FilePath chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe)) {
    NOTREACHED();
    return;
  }

  BrowserDistribution* distribution = BrowserDistribution::GetDistribution();
  // Ensure that the distribution supports creating shortcuts. If it doesn't,
  // the following code may result in NOTREACHED() being hit.
  DCHECK(distribution->CanCreateDesktopShortcuts());

  if (params.old_profile_name != params.profile_name) {
    const base::string16 old_shortcut_filename =
        profiles::internal::GetShortcutFilenameForProfile(
            params.old_profile_name,
            distribution);
    const base::string16 new_shortcut_filename =
        profiles::internal::GetShortcutFilenameForProfile(params.profile_name,
                                                          distribution);
    RenameChromeDesktopShortcutForProfile(old_shortcut_filename,
                                          new_shortcut_filename);
  }

  ShellUtil::ShortcutProperties properties(ShellUtil::CURRENT_USER);
  installer::Product product(distribution);
  product.AddDefaultShortcutProperties(chrome_exe, &properties);

  const base::string16 command_line =
      profiles::internal::CreateProfileShortcutFlags(params.profile_path);

  // Only set the profile-specific properties when |profile_name| is non empty.
  // If it is empty, it means the shortcut being created should be a regular,
  // non-profile Chrome shortcut.
  if (!params.profile_name.empty()) {
    properties.set_arguments(command_line);
    properties.set_icon(shortcut_icon, 0);
  } else {
    // Set the arguments explicitly to the empty string to ensure that
    // |ShellUtil::CreateOrUpdateShortcut| updates that part of the shortcut.
    properties.set_arguments(base::string16());
  }

  properties.set_app_id(
      ShellIntegration::GetChromiumModelIdForProfile(params.profile_path));

  ShellUtil::ShortcutOperation operation =
      ShellUtil::SHELL_SHORTCUT_REPLACE_EXISTING;

  std::vector<base::FilePath> shortcuts;
  ListDesktopShortcutsWithCommandLine(chrome_exe, command_line,
      params.action == ProfileShortcutManagerWin::UPDATE_NON_PROFILE_SHORTCUTS,
      &shortcuts);
  if (params.create_mode == ProfileShortcutManagerWin::CREATE_WHEN_NONE_FOUND &&
      shortcuts.empty()) {
    const base::string16 shortcut_name =
        profiles::internal::GetShortcutFilenameForProfile(params.profile_name,
                                                          distribution);
    shortcuts.push_back(base::FilePath(shortcut_name));
    operation = ShellUtil::SHELL_SHORTCUT_CREATE_IF_NO_SYSTEM_LEVEL;
  }

  for (size_t i = 0; i < shortcuts.size(); ++i) {
    const base::FilePath shortcut_name =
        shortcuts[i].BaseName().RemoveExtension();
    properties.set_shortcut_name(shortcut_name.value());
    ShellUtil::CreateOrUpdateShortcut(ShellUtil::SHORTCUT_LOCATION_DESKTOP,
        distribution, properties, operation);
  }
}

// Returns true if any desktop shortcuts exist with target |chrome_exe|,
// regardless of their command line arguments.
bool ChromeDesktopShortcutsExist(const base::FilePath& chrome_exe) {
  base::FilePath user_shortcuts_directory;
  if (!GetDesktopShortcutsDirectories(&user_shortcuts_directory, NULL))
    return false;

  base::FileEnumerator enumerator(user_shortcuts_directory, false,
                                  base::FileEnumerator::FILES);
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    if (IsChromeShortcut(path, chrome_exe, NULL))
      return true;
  }

  return false;
}

// Deletes all desktop shortcuts for the specified profile. If
// |ensure_shortcuts_remain| is true, then a regular non-profile shortcut will
// be created if this function would otherwise delete the last Chrome desktop
// shortcut(s). Must be called on the FILE thread.
void DeleteDesktopShortcuts(const base::FilePath& profile_path,
                            bool ensure_shortcuts_remain) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  base::FilePath chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe)) {
    NOTREACHED();
    return;
  }

  const base::string16 command_line =
      profiles::internal::CreateProfileShortcutFlags(profile_path);
  std::vector<base::FilePath> shortcuts;
  ListDesktopShortcutsWithCommandLine(chrome_exe, command_line, false,
                                      &shortcuts);

  for (size_t i = 0; i < shortcuts.size(); ++i) {
    // Use base::DeleteFile() instead of ShellUtil::RemoveShortcuts(), as the
    // latter causes non-profile taskbar shortcuts to be removed since it
    // doesn't consider the command-line of the shortcuts it deletes.
    // TODO(huangs): Refactor with ShellUtil::RemoveShortcuts().
    base::win::TaskbarUnpinShortcutLink(shortcuts[i].value().c_str());
    base::DeleteFile(shortcuts[i], false);
    // Notify the shell that the shortcut was deleted to ensure desktop refresh.
    SHChangeNotify(SHCNE_DELETE, SHCNF_PATH, shortcuts[i].value().c_str(),
                   NULL);
  }

  // If |ensure_shortcuts_remain| is true and deleting this profile caused the
  // last shortcuts to be removed, re-create a regular non-profile shortcut.
  const bool had_shortcuts = !shortcuts.empty();
  if (ensure_shortcuts_remain && had_shortcuts &&
      !ChromeDesktopShortcutsExist(chrome_exe)) {
    BrowserDistribution* distribution = BrowserDistribution::GetDistribution();
    // Ensure that the distribution supports creating shortcuts. If it doesn't,
    // the following code may result in NOTREACHED() being hit.
    DCHECK(distribution->CanCreateDesktopShortcuts());
    installer::Product product(distribution);

    ShellUtil::ShortcutProperties properties(ShellUtil::CURRENT_USER);
    product.AddDefaultShortcutProperties(chrome_exe, &properties);
    properties.set_shortcut_name(
        profiles::internal::GetShortcutFilenameForProfile(base::string16(),
                                                          distribution));
    ShellUtil::CreateOrUpdateShortcut(
        ShellUtil::SHORTCUT_LOCATION_DESKTOP, distribution, properties,
        ShellUtil::SHELL_SHORTCUT_CREATE_IF_NO_SYSTEM_LEVEL);
  }
}

// Returns true if profile at |profile_path| has any shortcuts. Does not
// consider non-profile shortcuts. Must be called on the FILE thread.
bool HasAnyProfileShortcuts(const base::FilePath& profile_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  base::FilePath chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe)) {
    NOTREACHED();
    return false;
  }

  const base::string16 command_line =
      profiles::internal::CreateProfileShortcutFlags(profile_path);
  std::vector<base::FilePath> shortcuts;
  ListDesktopShortcutsWithCommandLine(chrome_exe, command_line, false,
                                      &shortcuts);
  return !shortcuts.empty();
}

// Replaces any reserved characters with spaces, and trims the resulting string
// to prevent any leading and trailing spaces. Also makes sure that the
// resulting filename doesn't exceed |kMaxProfileShortcutFileNameLength|.
// TODO(macourteau): find a way to limit the total path's length to MAX_PATH
// instead of limiting the profile's name to |kMaxProfileShortcutFileNameLength|
// characters.
base::string16 SanitizeShortcutProfileNameString(
    const base::string16& profile_name) {
  base::string16 sanitized = profile_name;
  size_t pos = sanitized.find_first_of(kReservedCharacters);
  while (pos != base::string16::npos) {
    sanitized[pos] = L' ';
    pos = sanitized.find_first_of(kReservedCharacters, pos + 1);
  }

  base::TrimWhitespace(sanitized, base::TRIM_LEADING, &sanitized);
  if (sanitized.size() > kMaxProfileShortcutFileNameLength)
    sanitized.erase(kMaxProfileShortcutFileNameLength);
  base::TrimWhitespace(sanitized, base::TRIM_TRAILING, &sanitized);

  return sanitized;
}

// Returns a copied SkBitmap for the given image that can be safely passed to
// another thread.
SkBitmap GetSkBitmapCopy(const gfx::Image& image) {
  DCHECK(!image.IsEmpty());
  const SkBitmap* image_bitmap = image.ToSkBitmap();
  SkBitmap bitmap_copy;
  image_bitmap->deepCopyTo(&bitmap_copy);
  return bitmap_copy;
}

// Returns a copied SkBitmap for the given resource id that can be safely passed
// to another thread.
SkBitmap GetImageResourceSkBitmapCopy(int resource_id) {
  const gfx::Image image =
      ResourceBundle::GetSharedInstance().GetNativeImageNamed(resource_id);
  return GetSkBitmapCopy(image);
}

}  // namespace

namespace profiles {
namespace internal {

base::FilePath GetProfileIconPath(const base::FilePath& profile_path) {
  return profile_path.AppendASCII(kProfileIconFileName);
}

base::string16 GetShortcutFilenameForProfile(
    const base::string16& profile_name,
    BrowserDistribution* distribution) {
  base::string16 shortcut_name;
  if (!profile_name.empty()) {
    shortcut_name.append(SanitizeShortcutProfileNameString(profile_name));
    shortcut_name.append(L" - ");
    shortcut_name.append(l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME));
  } else {
    shortcut_name.append(
        distribution->GetShortcutName(BrowserDistribution::SHORTCUT_CHROME));
  }
  return shortcut_name + installer::kLnkExt;
}

base::string16 CreateProfileShortcutFlags(const base::FilePath& profile_path) {
  return base::StringPrintf(L"--%ls=\"%ls\"",
                            base::ASCIIToUTF16(
                                switches::kProfileDirectory).c_str(),
                            profile_path.BaseName().value().c_str());
}

}  // namespace internal
}  // namespace profiles

// static
bool ProfileShortcutManager::IsFeatureEnabled() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(switches::kEnableProfileShortcutManager) ||
         (BrowserDistribution::GetDistribution()->CanCreateDesktopShortcuts() &&
          !command_line->HasSwitch(switches::kUserDataDir));
}

// static
ProfileShortcutManager* ProfileShortcutManager::Create(
    ProfileManager* manager) {
  return new ProfileShortcutManagerWin(manager);
}

ProfileShortcutManagerWin::ProfileShortcutManagerWin(ProfileManager* manager)
    : profile_manager_(manager) {
  DCHECK_EQ(
      arraysize(kProfileAvatarIconResources2x),
      profiles::GetDefaultAvatarIconCount());

  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_CREATED,
                 content::NotificationService::AllSources());

  profile_manager_->GetProfileInfoCache().AddObserver(this);
}

ProfileShortcutManagerWin::~ProfileShortcutManagerWin() {
  profile_manager_->GetProfileInfoCache().RemoveObserver(this);
}

void ProfileShortcutManagerWin::CreateOrUpdateProfileIcon(
    const base::FilePath& profile_path) {
  CreateOrUpdateShortcutsForProfileAtPath(profile_path,
                                          CREATE_OR_UPDATE_ICON_ONLY,
                                          IGNORE_NON_PROFILE_SHORTCUTS);
}

void ProfileShortcutManagerWin::CreateProfileShortcut(
    const base::FilePath& profile_path) {
  CreateOrUpdateShortcutsForProfileAtPath(profile_path, CREATE_WHEN_NONE_FOUND,
                                          IGNORE_NON_PROFILE_SHORTCUTS);
}

void ProfileShortcutManagerWin::RemoveProfileShortcuts(
    const base::FilePath& profile_path) {
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&DeleteDesktopShortcuts, profile_path, false));
}

void ProfileShortcutManagerWin::HasProfileShortcuts(
    const base::FilePath& profile_path,
    const base::Callback<void(bool)>& callback) {
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&HasAnyProfileShortcuts, profile_path), callback);
}

void ProfileShortcutManagerWin::GetShortcutProperties(
    const base::FilePath& profile_path,
    CommandLine* command_line,
    base::string16* name,
    base::FilePath* icon_path) {
  base::FilePath chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe)) {
    NOTREACHED();
    return;
  }

  const ProfileInfoCache& cache = profile_manager_->GetProfileInfoCache();
  size_t profile_index = cache.GetIndexOfProfileWithPath(profile_path);
  DCHECK_LT(profile_index, cache.GetNumberOfProfiles());

  // The used profile name should be empty if there is only 1 profile.
  base::string16 shortcut_profile_name;
  if (cache.GetNumberOfProfiles() > 1)
    shortcut_profile_name = cache.GetNameOfProfileAtIndex(profile_index);

  *name = base::FilePath(profiles::internal::GetShortcutFilenameForProfile(
      shortcut_profile_name,
      BrowserDistribution::GetDistribution())).RemoveExtension().value();

  command_line->ParseFromString(L"\"" + chrome_exe.value() + L"\" " +
      profiles::internal::CreateProfileShortcutFlags(profile_path));

  *icon_path = profiles::internal::GetProfileIconPath(profile_path);
}

void ProfileShortcutManagerWin::OnProfileAdded(
    const base::FilePath& profile_path) {
  CreateOrUpdateProfileIcon(profile_path);
  if (profile_manager_->GetProfileInfoCache().GetNumberOfProfiles() == 2) {
    // When the second profile is added, make existing non-profile shortcuts
    // point to the first profile and be badged/named appropriately.
    CreateOrUpdateShortcutsForProfileAtPath(GetOtherProfilePath(profile_path),
                                            UPDATE_EXISTING_ONLY,
                                            UPDATE_NON_PROFILE_SHORTCUTS);
  }
}

void ProfileShortcutManagerWin::OnProfileWasRemoved(
    const base::FilePath& profile_path,
    const base::string16& profile_name) {
  const ProfileInfoCache& cache = profile_manager_->GetProfileInfoCache();
  // If there is only one profile remaining, remove the badging information
  // from an existing shortcut.
  const bool deleting_down_to_last_profile = (cache.GetNumberOfProfiles() == 1);
  if (deleting_down_to_last_profile) {
    // This is needed to unbadge the icon.
    CreateOrUpdateShortcutsForProfileAtPath(cache.GetPathOfProfileAtIndex(0),
                                            UPDATE_EXISTING_ONLY,
                                            IGNORE_NON_PROFILE_SHORTCUTS);
  }

  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                          base::Bind(&DeleteDesktopShortcuts,
                                     profile_path,
                                     deleting_down_to_last_profile));
}

void ProfileShortcutManagerWin::OnProfileNameChanged(
    const base::FilePath& profile_path,
    const base::string16& old_profile_name) {
  CreateOrUpdateShortcutsForProfileAtPath(profile_path, UPDATE_EXISTING_ONLY,
                                          IGNORE_NON_PROFILE_SHORTCUTS);
}

void ProfileShortcutManagerWin::OnProfileAvatarChanged(
    const base::FilePath& profile_path) {
  CreateOrUpdateProfileIcon(profile_path);
}

base::FilePath ProfileShortcutManagerWin::GetOtherProfilePath(
    const base::FilePath& profile_path) {
  const ProfileInfoCache& cache = profile_manager_->GetProfileInfoCache();
  DCHECK_EQ(2U, cache.GetNumberOfProfiles());
  // Get the index of the current profile, in order to find the index of the
  // other profile.
  size_t current_profile_index = cache.GetIndexOfProfileWithPath(profile_path);
  size_t other_profile_index = (current_profile_index == 0) ? 1 : 0;
  return cache.GetPathOfProfileAtIndex(other_profile_index);
}

void ProfileShortcutManagerWin::CreateOrUpdateShortcutsForProfileAtPath(
    const base::FilePath& profile_path,
    CreateOrUpdateMode create_mode,
    NonProfileShortcutAction action) {
  DCHECK(!BrowserThread::IsThreadInitialized(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::UI));
  CreateOrUpdateShortcutsParams params(profile_path, create_mode, action);

  ProfileInfoCache* cache = &profile_manager_->GetProfileInfoCache();
  size_t profile_index = cache->GetIndexOfProfileWithPath(profile_path);
  if (profile_index == std::string::npos)
    return;
  bool remove_badging = cache->GetNumberOfProfiles() == 1;

  params.old_profile_name =
      cache->GetShortcutNameOfProfileAtIndex(profile_index);

  // Exit early if the mode is to update existing profile shortcuts only and
  // none were ever created for this profile, per the shortcut name not being
  // set in the profile info cache.
  if (params.old_profile_name.empty() &&
      create_mode == UPDATE_EXISTING_ONLY &&
      action == IGNORE_NON_PROFILE_SHORTCUTS) {
    return;
  }

  if (!remove_badging) {
    params.profile_name = cache->GetNameOfProfileAtIndex(profile_index);

    // The profile might be using the Gaia avatar, which is not in the
    // resources array.
    bool has_gaia_image = false;
    if (cache->IsUsingGAIAPictureOfProfileAtIndex(profile_index)) {
      const gfx::Image* image =
          cache->GetGAIAPictureOfProfileAtIndex(profile_index);
      if (image) {
        params.avatar_image_1x = GetSkBitmapCopy(*image);
        // Gaia images are 256px, which makes them big enough to use in the
        // large icon case as well.
        DCHECK_GE(image->Width(), IconUtil::kLargeIconSize);
        params.avatar_image_2x = params.avatar_image_1x;
        has_gaia_image = true;
      }
    }

    // If the profile isn't using a Gaia image, or if the Gaia image did not
    // exist, revert to the previously used avatar icon.
    if (!has_gaia_image) {
      const size_t icon_index =
          cache->GetAvatarIconIndexOfProfileAtIndex(profile_index);
      const int resource_id_1x =
          profiles::GetDefaultAvatarIconResourceIDAtIndex(icon_index);
      const int resource_id_2x = kProfileAvatarIconResources2x[icon_index];
      // Make a copy of the SkBitmaps to ensure that we can safely use the image
      // data on the FILE thread.
      params.avatar_image_1x = GetImageResourceSkBitmapCopy(resource_id_1x);
      params.avatar_image_2x = GetImageResourceSkBitmapCopy(resource_id_2x);
    }
  }
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&CreateOrUpdateDesktopShortcutsAndIconForProfile, params));

  cache->SetShortcutNameOfProfileAtIndex(profile_index,
                                         params.profile_name);
}

void ProfileShortcutManagerWin::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    // This notification is triggered when a profile is loaded.
    case chrome::NOTIFICATION_PROFILE_CREATED: {
      Profile* profile =
          content::Source<Profile>(source).ptr()->GetOriginalProfile();
      if (profile->GetPrefs()->GetInteger(prefs::kProfileIconVersion) <
          kCurrentProfileIconVersion) {
        // Ensure the profile's icon file has been created.
        CreateOrUpdateProfileIcon(profile->GetPath());
      }
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}
