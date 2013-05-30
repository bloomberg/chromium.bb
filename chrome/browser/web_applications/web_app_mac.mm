// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/web_applications/web_app_mac.h"

#import <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>

#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/mac/app_mode_common.h"
#include "content/public/browser/browser_thread.h"
#include "grit/chromium_strings.h"
#include "skia/ext/skia_utils_mac.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/gfx/image/image_family.h"

namespace {

class ScopedCarbonHandle {
 public:
  ScopedCarbonHandle(size_t initial_size) : handle_(NewHandle(initial_size)) {
    DCHECK(handle_);
    DCHECK_EQ(noErr, MemError());
  }
  ~ScopedCarbonHandle() { DisposeHandle(handle_); }

  Handle Get() { return handle_; }
  char* Data() { return *handle_; }
  size_t HandleSize() const { return GetHandleSize(handle_); }

  IconFamilyHandle GetAsIconFamilyHandle() {
    return reinterpret_cast<IconFamilyHandle>(handle_);
  }

  bool WriteDataToFile(const base::FilePath& path) {
    NSData* data = [NSData dataWithBytes:Data()
                                  length:HandleSize()];
    return [data writeToFile:base::mac::FilePathToNSString(path)
                  atomically:NO];
  }

 private:
  Handle handle_;
};

void ConvertSkiaToARGB(const SkBitmap& bitmap, ScopedCarbonHandle* handle) {
  CHECK_EQ(4u * bitmap.width() * bitmap.height(), handle->HandleSize());

  char* argb = handle->Data();
  SkAutoLockPixels lock(bitmap);
  for (int y = 0; y < bitmap.height(); ++y) {
    for (int x = 0; x < bitmap.width(); ++x) {
      SkColor pixel = bitmap.getColor(x, y);
      argb[0] = SkColorGetA(pixel);
      argb[1] = SkColorGetR(pixel);
      argb[2] = SkColorGetG(pixel);
      argb[3] = SkColorGetB(pixel);
      argb += 4;
    }
  }
}

// Adds |image| to |icon_family|. Returns true on success, false on failure.
bool AddGfxImageToIconFamily(IconFamilyHandle icon_family,
                             const gfx::Image& image) {
  // When called via ShowCreateChromeAppShortcutsDialog the ImageFamily will
  // have all the representations desired here for mac, from the kDesiredSizes
  // array in web_app_ui.cc.
  SkBitmap bitmap = image.AsBitmap();
  if (bitmap.config() != SkBitmap::kARGB_8888_Config ||
      bitmap.width() != bitmap.height()) {
    return false;
  }

  OSType icon_type;
  switch (bitmap.width()) {
    case 512:
      icon_type = kIconServices512PixelDataARGB;
      break;
    case 256:
      icon_type = kIconServices256PixelDataARGB;
      break;
    case 128:
      icon_type = kIconServices128PixelDataARGB;
      break;
    case 48:
      icon_type = kIconServices48PixelDataARGB;
      break;
    case 32:
      icon_type = kIconServices32PixelDataARGB;
      break;
    case 16:
      icon_type = kIconServices16PixelDataARGB;
      break;
    default:
      return false;
  }

  ScopedCarbonHandle raw_data(bitmap.getSize());
  ConvertSkiaToARGB(bitmap, &raw_data);
  OSErr result = SetIconFamilyData(icon_family, icon_type, raw_data.Get());
  DCHECK_EQ(noErr, result);
  return result == noErr;
}

base::FilePath GetWritableApplicationsDirectory() {
  base::FilePath path;
  if (base::mac::GetLocalDirectory(NSApplicationDirectory, &path) &&
      file_util::PathIsWritable(path)) {
    return path;
  }
  if (base::mac::GetUserDirectory(NSApplicationDirectory, &path))
    return path;
  return base::FilePath();
}

}  // namespace

namespace web_app {

const char kChromeAppDirName[] = "Chrome Apps.localized";

WebAppShortcutCreator::WebAppShortcutCreator(
    const base::FilePath& user_data_dir,
    const ShellIntegration::ShortcutInfo& shortcut_info,
    const string16& chrome_bundle_id)
    : user_data_dir_(user_data_dir),
      info_(shortcut_info),
      chrome_bundle_id_(chrome_bundle_id) {
}

WebAppShortcutCreator::~WebAppShortcutCreator() {
}

base::FilePath WebAppShortcutCreator::GetShortcutPath() const {
  base::FilePath dst_path = GetDestinationPath();
  if (dst_path.empty())
    return dst_path;

  base::FilePath app_name = internals::GetSanitizedFileName(info_.title);
  return dst_path.Append(app_name.ReplaceExtension("app"));
}

bool WebAppShortcutCreator::CreateShortcut() {
  base::FilePath app_name = internals::GetSanitizedFileName(info_.title);
  base::FilePath app_file_name = app_name.ReplaceExtension("app");
  base::FilePath dst_path = GetDestinationPath();
  if (dst_path.empty() || !file_util::DirectoryExists(dst_path.DirName())) {
    LOG(ERROR) << "Couldn't find an Applications directory to copy app to.";
    return false;
  }
  if (!file_util::CreateDirectory(dst_path)) {
    LOG(ERROR) << "Creating directory " << dst_path.value() << " failed.";
    return false;
  }

  base::FilePath app_path = dst_path.Append(app_file_name);
  app_path = file_util::MakeUniqueDirectory(app_path);
  if (app_path.empty()) {
    LOG(ERROR) << "Couldn't create a unique directory for app path: "
               << app_path.value();
    return false;
  }
  app_file_name = app_path.BaseName();

  base::ScopedTempDir scoped_temp_dir;
  if (!scoped_temp_dir.CreateUniqueTempDir())
    return false;
  base::FilePath staging_path = scoped_temp_dir.path().Append(app_file_name);

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

  if (!file_util::CopyDirectory(staging_path, dst_path, true)) {
    LOG(ERROR) << "Copying app to dst path: " << dst_path.value() << " failed";
    return false;
  }

  base::mac::RemoveQuarantineAttribute(app_path);
  RevealGeneratedBundleInFinder(app_path);

  return true;
}

base::FilePath WebAppShortcutCreator::GetAppLoaderPath() const {
  return base::mac::PathForFrameworkBundleResource(
      base::mac::NSToCFCast(@"app_mode_loader.app"));
}

base::FilePath WebAppShortcutCreator::GetDestinationPath() const {
  base::FilePath path = GetWritableApplicationsDirectory();
  if (path.empty())
    return path;
  return path.Append(kChromeAppDirName);
}

bool WebAppShortcutCreator::UpdatePlist(const base::FilePath& app_path) const {
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

bool WebAppShortcutCreator::UpdateIcon(const base::FilePath& app_path) const {
  if (info_.favicon.empty())
    return true;

  ScopedCarbonHandle icon_family(0);
  bool image_added = false;
  for (gfx::ImageFamily::const_iterator it = info_.favicon.begin();
       it != info_.favicon.end(); ++it) {
    if (it->IsEmpty())
      continue;

    // Missing an icon size is not fatal so don't fail if adding the bitmap
    // doesn't work.
    if (!AddGfxImageToIconFamily(icon_family.GetAsIconFamilyHandle(), *it))
      continue;

    image_added = true;
  }

  if (!image_added)
    return false;

  base::FilePath resources_path =
      app_path.Append("Contents").Append("Resources");
  if (!file_util::CreateDirectory(resources_path))
    return false;

  return icon_family.WriteDataToFile(resources_path.Append("app.icns"));
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
    const base::FilePath& generated_bundle) const {
  [[NSWorkspace sharedWorkspace]
                    selectFile:base::mac::FilePathToNSString(generated_bundle)
      inFileViewerRootedAtPath:nil];
}

}  // namespace

namespace web_app {

base::FilePath GetAppInstallPath(
    const ShellIntegration::ShortcutInfo& shortcut_info) {
  WebAppShortcutCreator shortcut_creator(base::FilePath(),
                                         shortcut_info,
                                         string16());
  return shortcut_creator.GetShortcutPath();
}

namespace internals {

base::FilePath GetAppBundleByExtensionId(std::string extension_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  // This matches APP_MODE_APP_BUNDLE_ID in chrome/chrome.gyp.
  std::string bundle_id =
      base::mac::BaseBundleID() + std::string(".app.") + extension_id;
  base::mac::ScopedCFTypeRef<CFStringRef> bundle_id_cf(
      base::SysUTF8ToCFStringRef(bundle_id));
  CFURLRef url_ref = NULL;
  OSStatus status = LSFindApplicationForInfo(
      kLSUnknownCreator, bundle_id_cf.get(), NULL, NULL, &url_ref);
  base::mac::ScopedCFTypeRef<CFURLRef> url(url_ref);

  if (status != noErr)
    return base::FilePath();

  NSString* path_string = [base::mac::CFToNSCast(url.get()) path];
  return base::FilePath([path_string fileSystemRepresentation]);
}

bool CreatePlatformShortcuts(
    const base::FilePath& web_app_path,
    const ShellIntegration::ShortcutInfo& shortcut_info,
    const ShellIntegration::ShortcutLocations& /*creation_locations*/) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  string16 bundle_id = UTF8ToUTF16(base::mac::BaseBundleID());
  WebAppShortcutCreator shortcut_creator(web_app_path, shortcut_info,
                            bundle_id);
  return shortcut_creator.CreateShortcut();
}

void DeletePlatformShortcuts(
    const base::FilePath& web_app_path,
    const ShellIntegration::ShortcutInfo& info) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));

  base::FilePath bundle_path = GetAppBundleByExtensionId(info.extension_id);
  file_util::Delete(bundle_path, true);
}

void UpdatePlatformShortcuts(
    const base::FilePath& web_app_path,
    const string16& old_app_title,
    const ShellIntegration::ShortcutInfo& shortcut_info) {
  // TODO(benwells): Implement this when shortcuts / weblings are enabled on
  // mac.
}

}  // namespace internals

}  // namespace web_app
