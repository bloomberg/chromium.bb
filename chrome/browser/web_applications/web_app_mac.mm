// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_mac.h"

#import <Cocoa/Cocoa.h>

#include "base/file_util.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/memory/scoped_nsobject.h"
#include "base/scoped_temp_dir.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/mac/app_mode_common.h"
#include "content/public/browser/browser_thread.h"
#include "grit/chromium_strings.h"
#include "skia/ext/skia_utils_mac.h"
#include "third_party/icon_family/IconFamily.h"
#include "ui/base/l10n/l10n_util_mac.h"

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

}  // namespace


namespace web_app {

WebAppShortcutCreator::WebAppShortcutCreator(
    const FilePath& user_data_dir,
    const ShellIntegration::ShortcutInfo& shortcut_info)
    : user_data_dir_(user_data_dir),
      info_(shortcut_info) {
}

WebAppShortcutCreator::~WebAppShortcutCreator() {
}

bool WebAppShortcutCreator::CreateShortcut() {
  FilePath app_name = internals::GetSanitizedFileName(info_.title);
  FilePath app_file_name = app_name.ReplaceExtension("app");
  ScopedTempDir scoped_temp_dir;
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

  [[NSWorkspace sharedWorkspace]
      selectFile:base::mac::FilePathToNSString(dst_path)
      inFileViewerRootedAtPath:nil];
  return true;
}

FilePath WebAppShortcutCreator::GetAppLoaderPath() const {
  NSString* app_loader = [l10n_util::GetNSString(IDS_PRODUCT_NAME)
      stringByAppendingString:@" App Mode Loader.app"];
  return base::mac::PathForFrameworkBundleResource(
      base::mac::NSToCFCast(app_loader));
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
  NSMutableDictionary* dict =
      [NSMutableDictionary dictionaryWithContentsOfFile:plist_path];

  [dict setObject:base::SysUTF8ToNSString(info_.extension_id)
           forKey:app_mode::kCrAppModeShortcutIDKey];
  [dict setObject:base::SysUTF16ToNSString(info_.title)
           forKey:app_mode::kCrAppModeShortcutNameKey];
  [dict setObject:base::SysUTF8ToNSString(info_.url.spec())
           forKey:app_mode::kCrAppModeShortcutURLKey];
  [dict setObject:base::mac::FilePathToNSString(user_data_dir_)
           forKey:app_mode::kCrAppModeUserDataDirKey];
  [dict setObject:base::mac::FilePathToNSString(info_.extension_path)
           forKey:app_mode::kCrAppModeExtensionPathKey];
  return [dict writeToFile:plist_path atomically:YES];
}

bool WebAppShortcutCreator::UpdateIcon(const FilePath& app_path) const {
  // TODO(sail): Add support for multiple icon sizes.
  if (info_.favicon.empty() || info_.favicon.width() != 32 ||
      info_.favicon.height() != 32) {
    return true;
  }

  NSBitmapImageRep* image_rep = SkBitmapToImageRep(info_.favicon);
  if (!image_rep)
    return false;

  scoped_nsobject<IconFamily> icon_family([[IconFamily alloc] init]);
  bool success = [icon_family setIconFamilyElement:kLarge32BitData
                                fromBitmapImageRep:image_rep] &&
                 [icon_family setIconFamilyElement:kLarge8BitData
                                fromBitmapImageRep:image_rep] &&
                 [icon_family setIconFamilyElement:kLarge8BitMask
                                fromBitmapImageRep:image_rep] &&
                 [icon_family setIconFamilyElement:kLarge1BitMask
                                fromBitmapImageRep:image_rep];
  if (!success)
    return false;

  FilePath resources_path = app_path.Append("Contents").Append("Resources");
  if (!file_util::CreateDirectory(resources_path))
    return false;
  FilePath icon_path = resources_path.Append("app.icns");
  return [icon_family writeToFile:base::mac::FilePathToNSString(icon_path)];
}

}  // namespace

namespace web_app {
namespace internals {

void CreateShortcutTask(const FilePath& web_app_path,
                        const FilePath& profile_path,
                        const ShellIntegration::ShortcutInfo& shortcut_info) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  WebAppShortcutCreator shortcut_creator(web_app_path, shortcut_info);
  shortcut_creator.CreateShortcut();
}

}  // namespace internals
}  // namespace web_app
