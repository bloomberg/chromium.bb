// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/web_applications/web_app_mac.h"

#import <Cocoa/Cocoa.h>

#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/memory/scoped_nsobject.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/mac/app_mode_common.h"
#include "content/public/browser/browser_thread.h"
#include "grit/chromium_strings.h"
#include "skia/ext/skia_utils_mac.h"
#include "third_party/icon_family/IconFamily.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/gfx/image/image_skia.h"

namespace {

// Creates a NSBitmapImageRep from |bitmap|.
NSBitmapImageRep* SkBitmapToImageRep(const SkBitmap& bitmap) {
  base::mac::ScopedCFTypeRef<CGColorSpaceRef> color_space(
      CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB));
  NSImage* image = gfx::SkBitmapToNSImageWithColorSpace(
    bitmap, color_space.get());
  return base::mac::ObjCCast<NSBitmapImageRep>(
      [[image representations] lastObject]);
}

// Adds |image_rep| to |icon_family|. Returns true on success, false on failure.
bool AddBitmapImageRepToIconFamily(IconFamily* icon_family,
                                   NSBitmapImageRep* image_rep) {
  NSSize size = [image_rep size];
  if (size.width != size.height)
    return false;

  switch (static_cast<int>(size.width)) {
    case 512:
      return [icon_family setIconFamilyElement:kIconServices512PixelDataARGB
                            fromBitmapImageRep:image_rep];
    case 256:
      return [icon_family setIconFamilyElement:kIconServices256PixelDataARGB
                            fromBitmapImageRep:image_rep];
    case 128:
      return [icon_family setIconFamilyElement:kThumbnail32BitData
                            fromBitmapImageRep:image_rep] &&
             [icon_family setIconFamilyElement:kThumbnail8BitMask
                            fromBitmapImageRep:image_rep];
    case 32:
      return [icon_family setIconFamilyElement:kLarge32BitData
                            fromBitmapImageRep:image_rep] &&
             [icon_family setIconFamilyElement:kLarge8BitData
                            fromBitmapImageRep:image_rep] &&
             [icon_family setIconFamilyElement:kLarge8BitMask
                            fromBitmapImageRep:image_rep] &&
             [icon_family setIconFamilyElement:kLarge1BitMask
                            fromBitmapImageRep:image_rep];
    case 16:
      return [icon_family setIconFamilyElement:kSmall32BitData
                            fromBitmapImageRep:image_rep] &&
             [icon_family setIconFamilyElement:kSmall8BitData
                            fromBitmapImageRep:image_rep] &&
             [icon_family setIconFamilyElement:kSmall8BitMask
                            fromBitmapImageRep:image_rep] &&
             [icon_family setIconFamilyElement:kSmall1BitMask
                            fromBitmapImageRep:image_rep];
    default:
      return false;
  }
}

}  // namespace


namespace web_app {

WebAppShortcutCreator::WebAppShortcutCreator(
    const FilePath& user_data_dir,
    const ShellIntegration::ShortcutInfo& shortcut_info,
    const string16& chrome_bundle_id)
    : user_data_dir_(user_data_dir),
      info_(shortcut_info),
      chrome_bundle_id_(chrome_bundle_id) {
}

WebAppShortcutCreator::~WebAppShortcutCreator() {
}

bool WebAppShortcutCreator::CreateShortcut() {
  FilePath app_name = internals::GetSanitizedFileName(info_.title);
  FilePath app_file_name = app_name.ReplaceExtension("app");
  base::ScopedTempDir scoped_temp_dir;
  if (!scoped_temp_dir.CreateUniqueTempDir())
    return false;
  FilePath staging_path = scoped_temp_dir.path().Append(app_file_name);

  // Update the app's plist and icon in a temp directory. This works around
  // a Finder bug where the app's icon doesn't properly update.
  if (!file_util::CopyDirectory(GetAppLoaderPath(), staging_path, true)) {
    LOG(ERROR) << "Copying app to staging path: " << staging_path.value()
               << " failed";
    return false;
  }

  if (!UpdatePlist(staging_path))
    return false;

  if (!UpdateIcon(staging_path))
    return false;

  FilePath dst_path = GetDestinationPath(app_file_name);
  if (!file_util::CopyDirectory(staging_path, dst_path, true)) {
    LOG(ERROR) << "Copying app to dst path: " << dst_path.value() << " failed";
    return false;
  }

  dst_path = dst_path.Append(app_file_name);
  base::mac::RemoveQuarantineAttribute(dst_path);
  RevealGeneratedBundleInFinder(dst_path);

  return true;
}

FilePath WebAppShortcutCreator::GetAppLoaderPath() const {
  return base::mac::PathForFrameworkBundleResource(
      base::mac::NSToCFCast(@"app_mode_loader.app"));
}

FilePath WebAppShortcutCreator::GetDestinationPath(
    const FilePath& app_file_name) const {
  FilePath path;
  if (base::mac::GetLocalDirectory(NSApplicationDirectory, &path) &&
      file_util::PathIsWritable(path)) {
    return path;
  }

  if (base::mac::GetUserDirectory(NSApplicationDirectory, &path))
    return path;

  return FilePath();
}

bool WebAppShortcutCreator::UpdatePlist(const FilePath& app_path) const {
  NSString* plist_path = base::mac::FilePathToNSString(
      app_path.Append("Contents").Append("Info.plist"));

  NSString* extension_id = base::SysUTF8ToNSString(info_.extension_id);
  NSString* extension_title = base::SysUTF16ToNSString(info_.title);
  NSString* extension_url = base::SysUTF8ToNSString(info_.url.spec());
  NSString* chrome_bundle_id = base::SysUTF16ToNSString(chrome_bundle_id_);
  NSDictionary* replacement_dict =
      [NSDictionary dictionaryWithObjectsAndKeys:
          extension_id, app_mode::kShortcutIdPlaceholder,
          extension_title, app_mode::kShortcutNamePlaceholder,
          extension_url, app_mode::kShortcutURLPlaceholder,
          chrome_bundle_id, app_mode::kShortcutBrowserBundleIDPlaceholder,
          nil];

  NSMutableDictionary* plist =
      [NSMutableDictionary dictionaryWithContentsOfFile:plist_path];
  NSArray* keys = [plist allKeys];

  // 1. Fill in variables.
  for (id key in keys) {
    NSString* value = [plist valueForKey:key];
    if (![value isKindOfClass:[NSString class]] || [value length] < 2)
      continue;

    // Remove leading and trailing '@'s.
    NSString* variable =
        [value substringWithRange:NSMakeRange(1, [value length] - 2)];

    NSString* substitution = [replacement_dict valueForKey:variable];
    if (substitution)
      [plist setObject:substitution forKey:key];
  }

  // 2. Fill in other values.
  [plist setObject:GetBundleIdentifier(plist)
            forKey:base::mac::CFToNSCast(kCFBundleIdentifierKey)];
  [plist setObject:base::mac::FilePathToNSString(user_data_dir_)
            forKey:app_mode::kCrAppModeUserDataDirKey];
  [plist setObject:base::mac::FilePathToNSString(info_.profile_path.BaseName())
            forKey:app_mode::kCrAppModeProfileDirKey];
  return [plist writeToFile:plist_path atomically:YES];
}

bool WebAppShortcutCreator::UpdateIcon(const FilePath& app_path) const {
  if (info_.favicon.IsEmpty())
    return true;

  scoped_nsobject<IconFamily> icon_family([[IconFamily alloc] init]);
  bool image_added = false;
  info_.favicon.ToImageSkia()->EnsureRepsForSupportedScaleFactors();
  std::vector<gfx::ImageSkiaRep> image_reps =
      info_.favicon.ToImageSkia()->image_reps();
  for (size_t i = 0; i < image_reps.size(); ++i) {
    NSBitmapImageRep* image_rep = SkBitmapToImageRep(
        image_reps[i].sk_bitmap());
    if (!image_rep)
      continue;

    // Missing an icon size is not fatal so don't fail if adding the bitmap
    // doesn't work.
    if (!AddBitmapImageRepToIconFamily(icon_family, image_rep))
      continue;

    image_added = true;
  }

  if (!image_added)
    return false;

  FilePath resources_path = app_path.Append("Contents").Append("Resources");
  if (!file_util::CreateDirectory(resources_path))
    return false;
  FilePath icon_path = resources_path.Append("app.icns");
  return [icon_family writeToFile:base::mac::FilePathToNSString(icon_path)];
}

NSString* WebAppShortcutCreator::GetBundleIdentifier(NSDictionary* plist) const
{
  NSString* bundle_id_template =
    base::mac::ObjCCast<NSString>(
        [plist objectForKey:base::mac::CFToNSCast(kCFBundleIdentifierKey)]);
  NSString* extension_id = base::SysUTF8ToNSString(info_.extension_id);
  NSString* placeholder =
      [NSString stringWithFormat:@"@%@@", app_mode::kShortcutIdPlaceholder];
  NSString* bundle_id =
      [bundle_id_template
          stringByReplacingOccurrencesOfString:placeholder
                                    withString:extension_id];
  return bundle_id;
}

void WebAppShortcutCreator::RevealGeneratedBundleInFinder(
    const FilePath& generated_bundle) const {
  [[NSWorkspace sharedWorkspace]
                    selectFile:base::mac::FilePathToNSString(generated_bundle)
      inFileViewerRootedAtPath:nil];
}

}  // namespace

namespace web_app {

namespace internals {

bool CreatePlatformShortcuts(
    const FilePath& web_app_path,
    const ShellIntegration::ShortcutInfo& shortcut_info) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  string16 bundle_id = UTF8ToUTF16(base::mac::BaseBundleID());
  WebAppShortcutCreator shortcut_creator(web_app_path, shortcut_info,
                            bundle_id);
  return shortcut_creator.CreateShortcut();
}

void DeletePlatformShortcuts(
    const FilePath& web_app_path,
    const ShellIntegration::ShortcutInfo& shortcut_info) {
  // TODO(benwells): Implement this when shortcuts / weblings are enabled on
  // mac.
}

void UpdatePlatformShortcuts(
    const FilePath& web_app_path,
    const ShellIntegration::ShortcutInfo& shortcut_info) {
  // TODO(benwells): Implement this when shortcuts / weblings are enabled on
  // mac.
}

}  // namespace internals

}  // namespace web_app
