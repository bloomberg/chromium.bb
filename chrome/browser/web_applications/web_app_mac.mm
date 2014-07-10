// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/web_applications/web_app_mac.h"

#import <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_enumerator.h"
#include "base/files/scoped_temp_dir.h"
#include "base/mac/foundation_util.h"
#include "base/mac/launch_services_util.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#include "base/metrics/sparse_histogram.h"
#include "base/path_service.h"
#include "base/process/process_handle.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#import "chrome/browser/mac/dock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#import "chrome/common/mac/app_mode_common.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "grit/chrome_unscaled_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#import "skia/ext/skia_utils_mac.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_family.h"

bool g_app_shims_allow_update_and_launch_in_tests = false;

namespace {

// Launch Services Key to run as an agent app, which doesn't launch in the dock.
NSString* const kLSUIElement = @"LSUIElement";

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
  // array in web_app.cc.
  SkBitmap bitmap = image.AsBitmap();
  if (bitmap.colorType() != kN32_SkColorType ||
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

bool AppShimsDisabledForTest() {
  // Disable app shims in tests because shims created in ~/Applications will not
  // be cleaned up.
  return CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestType);
}

base::FilePath GetWritableApplicationsDirectory() {
  base::FilePath path;
  if (base::mac::GetUserDirectory(NSApplicationDirectory, &path)) {
    if (!base::DirectoryExists(path)) {
      if (!base::CreateDirectory(path))
        return base::FilePath();

      // Create a zero-byte ".localized" file to inherit localizations from OSX
      // for folders that have special meaning.
      base::WriteFile(path.Append(".localized"), NULL, 0);
    }
    return base::PathIsWritable(path) ? path : base::FilePath();
  }
  return base::FilePath();
}

// Given the path to an app bundle, return the resources directory.
base::FilePath GetResourcesPath(const base::FilePath& app_path) {
  return app_path.Append("Contents").Append("Resources");
}

bool HasExistingExtensionShim(const base::FilePath& destination_directory,
                              const std::string& extension_id,
                              const base::FilePath& own_basename) {
  // Check if there any any other shims for the same extension.
  base::FileEnumerator enumerator(destination_directory,
                                  false /* recursive */,
                                  base::FileEnumerator::DIRECTORIES);
  for (base::FilePath shim_path = enumerator.Next();
       !shim_path.empty(); shim_path = enumerator.Next()) {
    if (shim_path.BaseName() != own_basename &&
        EndsWith(shim_path.RemoveExtension().value(),
                 extension_id,
                 true /* case_sensitive */)) {
      return true;
    }
  }

  return false;
}

// Given the path to an app bundle, return the path to the Info.plist file.
NSString* GetPlistPath(const base::FilePath& bundle_path) {
  return base::mac::FilePathToNSString(
      bundle_path.Append("Contents").Append("Info.plist"));
}

NSMutableDictionary* ReadPlist(NSString* plist_path) {
  return [NSMutableDictionary dictionaryWithContentsOfFile:plist_path];
}

// Takes the path to an app bundle and checks that the CrAppModeUserDataDir in
// the Info.plist starts with the current user_data_dir. This uses starts with
// instead of equals because the CrAppModeUserDataDir could be the user_data_dir
// or the |app_data_dir_|.
bool HasSameUserDataDir(const base::FilePath& bundle_path) {
  NSDictionary* plist = ReadPlist(GetPlistPath(bundle_path));
  base::FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  DCHECK(!user_data_dir.empty());
  return StartsWithASCII(
      base::SysNSStringToUTF8(
          [plist valueForKey:app_mode::kCrAppModeUserDataDirKey]),
      user_data_dir.value(),
      true /* case_sensitive */);
}

void LaunchShimOnFileThread(const web_app::ShortcutInfo& shortcut_info,
                            bool launched_after_rebuild) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  base::FilePath shim_path = web_app::GetAppInstallPath(shortcut_info);

  if (shim_path.empty() ||
      !base::PathExists(shim_path) ||
      !HasSameUserDataDir(shim_path)) {
    // The user may have deleted the copy in the Applications folder, use the
    // one in the web app's |app_data_dir_|.
    base::FilePath app_data_dir = web_app::GetWebAppDataDirectory(
        shortcut_info.profile_path, shortcut_info.extension_id, GURL());
    shim_path = app_data_dir.Append(shim_path.BaseName());
  }

  if (!base::PathExists(shim_path))
    return;

  CommandLine command_line(CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(
      app_mode::kLaunchedByChromeProcessId,
      base::IntToString(base::GetCurrentProcId()));
  if (launched_after_rebuild)
    command_line.AppendSwitch(app_mode::kLaunchedAfterRebuild);
  // Launch without activating (kLSLaunchDontSwitch).
  base::mac::OpenApplicationWithPath(
      shim_path, command_line, kLSLaunchDefaults | kLSLaunchDontSwitch, NULL);
}

base::FilePath GetAppLoaderPath() {
  return base::mac::PathForFrameworkBundleResource(
      base::mac::NSToCFCast(@"app_mode_loader.app"));
}

void UpdateAndLaunchShimOnFileThread(
    const web_app::ShortcutInfo& shortcut_info,
    const extensions::FileHandlersInfo& file_handlers_info) {
  base::FilePath shortcut_data_dir = web_app::GetWebAppDataDirectory(
      shortcut_info.profile_path, shortcut_info.extension_id, GURL());
  web_app::internals::UpdatePlatformShortcuts(
      shortcut_data_dir, base::string16(), shortcut_info, file_handlers_info);
  LaunchShimOnFileThread(shortcut_info, true);
}

void UpdateAndLaunchShim(
    const web_app::ShortcutInfo& shortcut_info,
    const extensions::FileHandlersInfo& file_handlers_info) {
  content::BrowserThread::PostTask(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(
          &UpdateAndLaunchShimOnFileThread, shortcut_info, file_handlers_info));
}

void RebuildAppAndLaunch(const web_app::ShortcutInfo& shortcut_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (shortcut_info.extension_id == app_mode::kAppListModeId) {
    AppListService* app_list_service =
        AppListService::Get(chrome::HOST_DESKTOP_TYPE_NATIVE);
    app_list_service->CreateShortcut();
    app_list_service->Show();
    return;
  }

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* profile =
      profile_manager->GetProfileByPath(shortcut_info.profile_path);
  if (!profile || !profile_manager->IsValidProfile(profile))
    return;

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile);
  const extensions::Extension* extension = registry->GetExtensionById(
      shortcut_info.extension_id, extensions::ExtensionRegistry::ENABLED);
  if (!extension || !extension->is_platform_app())
    return;

  web_app::GetInfoForApp(extension, profile, base::Bind(&UpdateAndLaunchShim));
}

base::FilePath GetLocalizableAppShortcutsSubdirName() {
  static const char kChromiumAppDirName[] = "Chromium Apps.localized";
  static const char kChromeAppDirName[] = "Chrome Apps.localized";
  static const char kChromeCanaryAppDirName[] = "Chrome Canary Apps.localized";

  switch (chrome::VersionInfo::GetChannel()) {
    case chrome::VersionInfo::CHANNEL_UNKNOWN:
      return base::FilePath(kChromiumAppDirName);

    case chrome::VersionInfo::CHANNEL_CANARY:
      return base::FilePath(kChromeCanaryAppDirName);

    default:
      return base::FilePath(kChromeAppDirName);
  }
}

// Creates a canvas the same size as |overlay|, copies the appropriate
// representation from |backgound| into it (according to Cocoa), then draws
// |overlay| over it using NSCompositeSourceOver.
NSImageRep* OverlayImageRep(NSImage* background, NSImageRep* overlay) {
  DCHECK(background);
  NSInteger dimension = [overlay pixelsWide];
  DCHECK_EQ(dimension, [overlay pixelsHigh]);
  base::scoped_nsobject<NSBitmapImageRep> canvas([[NSBitmapImageRep alloc]
      initWithBitmapDataPlanes:NULL
                    pixelsWide:dimension
                    pixelsHigh:dimension
                 bitsPerSample:8
               samplesPerPixel:4
                      hasAlpha:YES
                      isPlanar:NO
                colorSpaceName:NSCalibratedRGBColorSpace
                   bytesPerRow:0
                  bitsPerPixel:0]);

  // There isn't a colorspace name constant for sRGB, so retag.
  NSBitmapImageRep* srgb_canvas = [canvas
      bitmapImageRepByRetaggingWithColorSpace:[NSColorSpace sRGBColorSpace]];
  canvas.reset([srgb_canvas retain]);

  // Communicate the DIP scale (1.0). TODO(tapted): Investigate HiDPI.
  [canvas setSize:NSMakeSize(dimension, dimension)];

  NSGraphicsContext* drawing_context =
      [NSGraphicsContext graphicsContextWithBitmapImageRep:canvas];
  [NSGraphicsContext saveGraphicsState];
  [NSGraphicsContext setCurrentContext:drawing_context];
  [background drawInRect:NSMakeRect(0, 0, dimension, dimension)
                fromRect:NSZeroRect
               operation:NSCompositeCopy
                fraction:1.0];
  [overlay drawInRect:NSMakeRect(0, 0, dimension, dimension)
             fromRect:NSZeroRect
            operation:NSCompositeSourceOver
             fraction:1.0
       respectFlipped:NO
                hints:0];
  [NSGraphicsContext restoreGraphicsState];
  return canvas.autorelease();
}

// Helper function to extract the single NSImageRep held in a resource bundle
// image.
NSImageRep* ImageRepForResource(int resource_id) {
  gfx::Image& image =
      ResourceBundle::GetSharedInstance().GetNativeImageNamed(resource_id);
  NSArray* image_reps = [image.AsNSImage() representations];
  DCHECK_EQ(1u, [image_reps count]);
  return [image_reps objectAtIndex:0];
}

// Adds a localized strings file for the Chrome Apps directory using the current
// locale. OSX will use this for the display name.
// + Chrome Apps.localized (|apps_directory|)
// | + .localized
// | | en.strings
// | | de.strings
void UpdateAppShortcutsSubdirLocalizedName(
    const base::FilePath& apps_directory) {
  base::FilePath localized = apps_directory.Append(".localized");
  if (!base::CreateDirectory(localized))
    return;

  base::FilePath directory_name = apps_directory.BaseName().RemoveExtension();
  base::string16 localized_name = ShellIntegration::GetAppShortcutsSubdirName();
  NSDictionary* strings_dict = @{
      base::mac::FilePathToNSString(directory_name) :
          base::SysUTF16ToNSString(localized_name)
  };

  std::string locale = l10n_util::NormalizeLocale(
      l10n_util::GetApplicationLocale(std::string()));

  NSString* strings_path = base::mac::FilePathToNSString(
      localized.Append(locale + ".strings"));
  [strings_dict writeToFile:strings_path
                 atomically:YES];

  base::scoped_nsobject<NSImage> folder_icon_image([[NSImage alloc] init]);

  // Use complete assets for the small icon sizes. -[NSWorkspace setIcon:] has a
  // bug when dealing with named NSImages where it incorrectly handles alpha
  // premultiplication. This is most noticable with small assets since the 1px
  // border is a much larger component of the small icons.
  // See http://crbug.com/305373 for details.
  [folder_icon_image addRepresentation:ImageRepForResource(IDR_APPS_FOLDER_16)];
  [folder_icon_image addRepresentation:ImageRepForResource(IDR_APPS_FOLDER_32)];

  // Brand larger folder assets with an embossed app launcher logo to conserve
  // distro size and for better consistency with changing hue across OSX
  // versions. The folder is textured, so compresses poorly without this.
  const int kBrandResourceIds[] = {
    IDR_APPS_FOLDER_OVERLAY_128,
    IDR_APPS_FOLDER_OVERLAY_512,
  };
  NSImage* base_image = [NSImage imageNamed:NSImageNameFolder];
  for (size_t i = 0; i < arraysize(kBrandResourceIds); ++i) {
    NSImageRep* with_overlay =
        OverlayImageRep(base_image, ImageRepForResource(kBrandResourceIds[i]));
    DCHECK(with_overlay);
    if (with_overlay)
      [folder_icon_image addRepresentation:with_overlay];
  }
  [[NSWorkspace sharedWorkspace]
      setIcon:folder_icon_image
      forFile:base::mac::FilePathToNSString(apps_directory)
      options:0];
}

void DeletePathAndParentIfEmpty(const base::FilePath& app_path) {
  DCHECK(!app_path.empty());
  base::DeleteFile(app_path, true);
  base::FilePath apps_folder = app_path.DirName();
  if (base::IsDirectoryEmpty(apps_folder))
    base::DeleteFile(apps_folder, false);
}

bool IsShimForProfile(const base::FilePath& base_name,
                      const std::string& profile_base_name) {
  if (!StartsWithASCII(base_name.value(), profile_base_name, true))
    return false;

  if (base_name.Extension() != ".app")
    return false;

  std::string app_id = base_name.RemoveExtension().value();
  // Strip (profile_base_name + " ") from the start.
  app_id = app_id.substr(profile_base_name.size() + 1);
  return extensions::Extension::IdIsValid(app_id);
}

std::vector<base::FilePath> GetAllAppBundlesInPath(
    const base::FilePath& internal_shortcut_path,
    const std::string& profile_base_name) {
  std::vector<base::FilePath> bundle_paths;

  base::FileEnumerator enumerator(internal_shortcut_path,
                                  true /* recursive */,
                                  base::FileEnumerator::DIRECTORIES);
  for (base::FilePath bundle_path = enumerator.Next();
       !bundle_path.empty(); bundle_path = enumerator.Next()) {
    if (IsShimForProfile(bundle_path.BaseName(), profile_base_name))
      bundle_paths.push_back(bundle_path);
  }

  return bundle_paths;
}

web_app::ShortcutInfo BuildShortcutInfoFromBundle(
    const base::FilePath& bundle_path) {
  NSDictionary* plist = ReadPlist(GetPlistPath(bundle_path));

  web_app::ShortcutInfo shortcut_info;
  shortcut_info.extension_id = base::SysNSStringToUTF8(
      [plist valueForKey:app_mode::kCrAppModeShortcutIDKey]);
  shortcut_info.is_platform_app = true;
  shortcut_info.url = GURL(base::SysNSStringToUTF8(
      [plist valueForKey:app_mode::kCrAppModeShortcutURLKey]));
  shortcut_info.title = base::SysNSStringToUTF16(
      [plist valueForKey:app_mode::kCrAppModeShortcutNameKey]);
  shortcut_info.profile_name = base::SysNSStringToUTF8(
      [plist valueForKey:app_mode::kCrAppModeProfileNameKey]);

  // Figure out the profile_path. Since the user_data_dir could contain the
  // path to the web app data dir.
  base::FilePath user_data_dir = base::mac::NSStringToFilePath(
      [plist valueForKey:app_mode::kCrAppModeUserDataDirKey]);
  base::FilePath profile_base_name = base::mac::NSStringToFilePath(
      [plist valueForKey:app_mode::kCrAppModeProfileDirKey]);
  if (user_data_dir.DirName().DirName().BaseName() == profile_base_name)
    shortcut_info.profile_path = user_data_dir.DirName().DirName();
  else
    shortcut_info.profile_path = user_data_dir.Append(profile_base_name);

  return shortcut_info;
}

web_app::ShortcutInfo RecordAppShimErrorAndBuildShortcutInfo(
    const base::FilePath& bundle_path) {
  NSDictionary* plist = ReadPlist(GetPlistPath(bundle_path));
  base::Version full_version(base::SysNSStringToUTF8(
      [plist valueForKey:app_mode::kCFBundleShortVersionStringKey]));
  int major_version = 0;
  if (full_version.IsValid())
    major_version = full_version.components()[0];
  UMA_HISTOGRAM_SPARSE_SLOWLY("Apps.AppShimErrorVersion", major_version);

  return BuildShortcutInfoFromBundle(bundle_path);
}

void UpdateFileTypes(NSMutableDictionary* plist,
                     const extensions::FileHandlersInfo& file_handlers_info) {
  NSMutableArray* document_types =
      [NSMutableArray arrayWithCapacity:file_handlers_info.size()];

  for (extensions::FileHandlersInfo::const_iterator info_it =
           file_handlers_info.begin();
       info_it != file_handlers_info.end();
       ++info_it) {
    const extensions::FileHandlerInfo& info = *info_it;

    NSMutableArray* file_extensions =
        [NSMutableArray arrayWithCapacity:info.extensions.size()];
    for (std::set<std::string>::iterator it = info.extensions.begin();
         it != info.extensions.end();
         ++it) {
      [file_extensions addObject:base::SysUTF8ToNSString(*it)];
    }

    NSMutableArray* mime_types =
        [NSMutableArray arrayWithCapacity:info.types.size()];
    for (std::set<std::string>::iterator it = info.types.begin();
         it != info.types.end();
         ++it) {
      [mime_types addObject:base::SysUTF8ToNSString(*it)];
    }

    NSDictionary* type_dictionary = @{
      // TODO(jackhou): Add the type name and and icon file once the manifest
      // supports these.
      // app_mode::kCFBundleTypeNameKey : ,
      // app_mode::kCFBundleTypeIconFileKey : ,
      app_mode::kCFBundleTypeExtensionsKey : file_extensions,
      app_mode::kCFBundleTypeMIMETypesKey : mime_types,
      app_mode::kCFBundleTypeRoleKey : app_mode::kBundleTypeRoleViewer
    };
    [document_types addObject:type_dictionary];
  }

  [plist setObject:document_types
            forKey:app_mode::kCFBundleDocumentTypesKey];
}

}  // namespace

namespace web_app {

WebAppShortcutCreator::WebAppShortcutCreator(
    const base::FilePath& app_data_dir,
    const ShortcutInfo& shortcut_info,
    const extensions::FileHandlersInfo& file_handlers_info)
    : app_data_dir_(app_data_dir),
      info_(shortcut_info),
      file_handlers_info_(file_handlers_info) {}

WebAppShortcutCreator::~WebAppShortcutCreator() {}

base::FilePath WebAppShortcutCreator::GetApplicationsShortcutPath() const {
  base::FilePath applications_dir = GetApplicationsDirname();
  return applications_dir.empty() ?
      base::FilePath() : applications_dir.Append(GetShortcutBasename());
}

base::FilePath WebAppShortcutCreator::GetInternalShortcutPath() const {
  return app_data_dir_.Append(GetShortcutBasename());
}

base::FilePath WebAppShortcutCreator::GetShortcutBasename() const {
  std::string app_name;
  // Check if there should be a separate shortcut made for different profiles.
  // Such shortcuts will have a |profile_name| set on the ShortcutInfo,
  // otherwise it will be empty.
  if (!info_.profile_name.empty()) {
    app_name += info_.profile_path.BaseName().value();
    app_name += ' ';
  }
  app_name += info_.extension_id;
  return base::FilePath(app_name).ReplaceExtension("app");
}

bool WebAppShortcutCreator::BuildShortcut(
    const base::FilePath& staging_path) const {
  // Update the app's plist and icon in a temp directory. This works around
  // a Finder bug where the app's icon doesn't properly update.
  if (!base::CopyDirectory(GetAppLoaderPath(), staging_path, true)) {
    LOG(ERROR) << "Copying app to staging path: " << staging_path.value()
               << " failed.";
    return false;
  }

  return UpdatePlist(staging_path) &&
      UpdateDisplayName(staging_path) &&
      UpdateIcon(staging_path);
}

size_t WebAppShortcutCreator::CreateShortcutsIn(
    const std::vector<base::FilePath>& folders) const {
  size_t succeeded = 0;

  base::ScopedTempDir scoped_temp_dir;
  if (!scoped_temp_dir.CreateUniqueTempDir())
    return 0;

  base::FilePath app_name = GetShortcutBasename();
  base::FilePath staging_path = scoped_temp_dir.path().Append(app_name);
  if (!BuildShortcut(staging_path))
    return 0;

  for (std::vector<base::FilePath>::const_iterator it = folders.begin();
       it != folders.end(); ++it) {
    const base::FilePath& dst_path = *it;
    if (!base::CreateDirectory(dst_path)) {
      LOG(ERROR) << "Creating directory " << dst_path.value() << " failed.";
      return succeeded;
    }

    if (!base::CopyDirectory(staging_path, dst_path, true)) {
      LOG(ERROR) << "Copying app to dst path: " << dst_path.value()
                 << " failed";
      return succeeded;
    }

    // Remove the quarantine attribute from both the bundle and the executable.
    base::mac::RemoveQuarantineAttribute(dst_path.Append(app_name));
    base::mac::RemoveQuarantineAttribute(
        dst_path.Append(app_name)
            .Append("Contents").Append("MacOS").Append("app_mode_loader"));
    ++succeeded;
  }

  return succeeded;
}

bool WebAppShortcutCreator::CreateShortcuts(
    ShortcutCreationReason creation_reason,
    ShortcutLocations creation_locations) {
  const base::FilePath applications_dir = GetApplicationsDirname();
  if (applications_dir.empty() ||
      !base::DirectoryExists(applications_dir.DirName())) {
    LOG(ERROR) << "Couldn't find an Applications directory to copy app to.";
    return false;
  }

  UpdateAppShortcutsSubdirLocalizedName(applications_dir);

  // If non-nil, this path is added to the OSX Dock after creating shortcuts.
  NSString* path_to_add_to_dock = nil;

  std::vector<base::FilePath> paths;

  // The app list shim is not tied to a particular profile, so omit the copy
  // placed under the profile path. For shims, this copy is used when the
  // version under Applications is removed, and not needed for app list because
  // setting LSUIElement means there is no Dock "running" status to show.
  const bool is_app_list = info_.extension_id == app_mode::kAppListModeId;
  if (is_app_list) {
    path_to_add_to_dock = base::SysUTF8ToNSString(
        applications_dir.Append(GetShortcutBasename()).AsUTF8Unsafe());
  } else {
    paths.push_back(app_data_dir_);
  }

  bool shortcut_visible =
      creation_locations.applications_menu_location != APP_MENU_LOCATION_HIDDEN;
  if (shortcut_visible)
    paths.push_back(applications_dir);

  DCHECK(!paths.empty());
  size_t success_count = CreateShortcutsIn(paths);
  if (success_count == 0)
    return false;

  if (!is_app_list)
    UpdateInternalBundleIdentifier();

  if (success_count != paths.size())
    return false;

  if (creation_locations.in_quick_launch_bar && path_to_add_to_dock &&
      shortcut_visible) {
    switch (dock::AddIcon(path_to_add_to_dock, nil)) {
      case dock::IconAddFailure:
        // If adding the icon failed, instead reveal the Finder window.
        RevealAppShimInFinder();
        break;
      case dock::IconAddSuccess:
      case dock::IconAlreadyPresent:
        break;
    }
    return true;
  }

  if (creation_reason == SHORTCUT_CREATION_BY_USER)
    RevealAppShimInFinder();

  return true;
}

void WebAppShortcutCreator::DeleteShortcuts() {
  base::FilePath app_path = GetApplicationsShortcutPath();
  if (!app_path.empty() && HasSameUserDataDir(app_path))
    DeletePathAndParentIfEmpty(app_path);

  // In case the user has moved/renamed/copied the app bundle.
  base::FilePath bundle_path = GetAppBundleById(GetBundleIdentifier());
  if (!bundle_path.empty() && HasSameUserDataDir(bundle_path))
    base::DeleteFile(bundle_path, true);

  // Delete the internal one.
  DeletePathAndParentIfEmpty(GetInternalShortcutPath());
}

bool WebAppShortcutCreator::UpdateShortcuts() {
  std::vector<base::FilePath> paths;
  base::DeleteFile(GetInternalShortcutPath(), true);
  paths.push_back(app_data_dir_);

  // Try to update the copy under /Applications. If that does not exist, check
  // if a matching bundle can be found elsewhere.
  base::FilePath app_path = GetApplicationsShortcutPath();
  if (app_path.empty() || !base::PathExists(app_path))
    app_path = GetAppBundleById(GetBundleIdentifier());

  if (!app_path.empty()) {
    base::DeleteFile(app_path, true);
    paths.push_back(app_path.DirName());
  }

  size_t success_count = CreateShortcutsIn(paths);
  if (success_count == 0)
    return false;

  UpdateInternalBundleIdentifier();
  return success_count == paths.size() && !app_path.empty();
}

base::FilePath WebAppShortcutCreator::GetApplicationsDirname() const {
  base::FilePath path = GetWritableApplicationsDirectory();
  if (path.empty())
    return path;

  return path.Append(GetLocalizableAppShortcutsSubdirName());
}

bool WebAppShortcutCreator::UpdatePlist(const base::FilePath& app_path) const {
  NSString* extension_id = base::SysUTF8ToNSString(info_.extension_id);
  NSString* extension_title = base::SysUTF16ToNSString(info_.title);
  NSString* extension_url = base::SysUTF8ToNSString(info_.url.spec());
  NSString* chrome_bundle_id =
      base::SysUTF8ToNSString(base::mac::BaseBundleID());
  NSDictionary* replacement_dict =
      [NSDictionary dictionaryWithObjectsAndKeys:
          extension_id, app_mode::kShortcutIdPlaceholder,
          extension_title, app_mode::kShortcutNamePlaceholder,
          extension_url, app_mode::kShortcutURLPlaceholder,
          chrome_bundle_id, app_mode::kShortcutBrowserBundleIDPlaceholder,
          nil];

  NSString* plist_path = GetPlistPath(app_path);
  NSMutableDictionary* plist = ReadPlist(plist_path);
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
  [plist setObject:base::SysUTF8ToNSString(GetBundleIdentifier())
            forKey:base::mac::CFToNSCast(kCFBundleIdentifierKey)];
  [plist setObject:base::mac::FilePathToNSString(app_data_dir_)
            forKey:app_mode::kCrAppModeUserDataDirKey];
  [plist setObject:base::mac::FilePathToNSString(info_.profile_path.BaseName())
            forKey:app_mode::kCrAppModeProfileDirKey];
  [plist setObject:base::SysUTF8ToNSString(info_.profile_name)
            forKey:app_mode::kCrAppModeProfileNameKey];
  [plist setObject:[NSNumber numberWithBool:YES]
            forKey:app_mode::kLSHasLocalizedDisplayNameKey];
  if (info_.extension_id == app_mode::kAppListModeId) {
    // Prevent the app list from bouncing in the dock, and getting a run light.
    [plist setObject:[NSNumber numberWithBool:YES]
              forKey:kLSUIElement];
  }

  base::FilePath app_name = app_path.BaseName().RemoveExtension();
  [plist setObject:base::mac::FilePathToNSString(app_name)
            forKey:base::mac::CFToNSCast(kCFBundleNameKey)];

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableAppsFileAssociations)) {
    UpdateFileTypes(plist, file_handlers_info_);
  }

  return [plist writeToFile:plist_path
                 atomically:YES];
}

bool WebAppShortcutCreator::UpdateDisplayName(
    const base::FilePath& app_path) const {
  // OSX searches for the best language in the order of preferred languages.
  // Since we only have one localization directory, it will choose this one.
  base::FilePath localized_dir = GetResourcesPath(app_path).Append("en.lproj");
  if (!base::CreateDirectory(localized_dir))
    return false;

  NSString* bundle_name = base::SysUTF16ToNSString(info_.title);
  NSString* display_name = base::SysUTF16ToNSString(info_.title);
  if (HasExistingExtensionShim(GetApplicationsDirname(),
                               info_.extension_id,
                               app_path.BaseName())) {
    display_name = [bundle_name
        stringByAppendingString:base::SysUTF8ToNSString(
            " (" + info_.profile_name + ")")];
  }

  NSDictionary* strings_plist = @{
    base::mac::CFToNSCast(kCFBundleNameKey) : bundle_name,
    app_mode::kCFBundleDisplayNameKey : display_name
  };

  NSString* localized_path = base::mac::FilePathToNSString(
      localized_dir.Append("InfoPlist.strings"));
  return [strings_plist writeToFile:localized_path
                         atomically:YES];
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

  base::FilePath resources_path = GetResourcesPath(app_path);
  if (!base::CreateDirectory(resources_path))
    return false;

  return icon_family.WriteDataToFile(resources_path.Append("app.icns"));
}

bool WebAppShortcutCreator::UpdateInternalBundleIdentifier() const {
  NSString* plist_path = GetPlistPath(GetInternalShortcutPath());
  NSMutableDictionary* plist = ReadPlist(plist_path);

  [plist setObject:base::SysUTF8ToNSString(GetInternalBundleIdentifier())
            forKey:base::mac::CFToNSCast(kCFBundleIdentifierKey)];
  return [plist writeToFile:plist_path
                 atomically:YES];
}

base::FilePath WebAppShortcutCreator::GetAppBundleById(
    const std::string& bundle_id) const {
  base::ScopedCFTypeRef<CFStringRef> bundle_id_cf(
      base::SysUTF8ToCFStringRef(bundle_id));
  CFURLRef url_ref = NULL;
  OSStatus status = LSFindApplicationForInfo(
      kLSUnknownCreator, bundle_id_cf.get(), NULL, NULL, &url_ref);
  if (status != noErr)
    return base::FilePath();

  base::ScopedCFTypeRef<CFURLRef> url(url_ref);
  NSString* path_string = [base::mac::CFToNSCast(url.get()) path];
  return base::FilePath([path_string fileSystemRepresentation]);
}

std::string WebAppShortcutCreator::GetBundleIdentifier() const {
  // Replace spaces in the profile path with hyphen.
  std::string normalized_profile_path;
  base::ReplaceChars(info_.profile_path.BaseName().value(),
                     " ", "-", &normalized_profile_path);

  // This matches APP_MODE_APP_BUNDLE_ID in chrome/chrome.gyp.
  std::string bundle_id =
      base::mac::BaseBundleID() + std::string(".app.") +
      normalized_profile_path + "-" + info_.extension_id;

  return bundle_id;
}

std::string WebAppShortcutCreator::GetInternalBundleIdentifier() const {
  return GetBundleIdentifier() + "-internal";
}

void WebAppShortcutCreator::RevealAppShimInFinder() const {
  base::FilePath app_path = GetApplicationsShortcutPath();
  if (app_path.empty())
    return;

  [[NSWorkspace sharedWorkspace]
                    selectFile:base::mac::FilePathToNSString(app_path)
      inFileViewerRootedAtPath:nil];
}

base::FilePath GetAppInstallPath(const ShortcutInfo& shortcut_info) {
  WebAppShortcutCreator shortcut_creator(
      base::FilePath(), shortcut_info, extensions::FileHandlersInfo());
  return shortcut_creator.GetApplicationsShortcutPath();
}

void MaybeLaunchShortcut(const ShortcutInfo& shortcut_info) {
  if (AppShimsDisabledForTest() &&
      !g_app_shims_allow_update_and_launch_in_tests) {
    return;
  }

  content::BrowserThread::PostTask(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&LaunchShimOnFileThread, shortcut_info, false));
}

bool MaybeRebuildShortcut(const CommandLine& command_line) {
  if (!command_line.HasSwitch(app_mode::kAppShimError))
    return false;

  base::PostTaskAndReplyWithResult(
      content::BrowserThread::GetBlockingPool(),
      FROM_HERE,
      base::Bind(&RecordAppShimErrorAndBuildShortcutInfo,
                 command_line.GetSwitchValuePath(app_mode::kAppShimError)),
      base::Bind(&RebuildAppAndLaunch));
  return true;
}

// Called when the app's ShortcutInfo (with icon) is loaded when creating app
// shortcuts.
void CreateAppShortcutInfoLoaded(
    Profile* profile,
    const extensions::Extension* app,
    const base::Callback<void(bool)>& close_callback,
    const ShortcutInfo& shortcut_info) {
  base::scoped_nsobject<NSAlert> alert([[NSAlert alloc] init]);

  NSButton* continue_button = [alert
      addButtonWithTitle:l10n_util::GetNSString(IDS_CREATE_SHORTCUTS_COMMIT)];
  [continue_button setKeyEquivalent:@""];

  NSButton* cancel_button =
      [alert addButtonWithTitle:l10n_util::GetNSString(IDS_CANCEL)];
  [cancel_button setKeyEquivalent:@"\r"];

  [alert setMessageText:l10n_util::GetNSString(IDS_CREATE_SHORTCUTS_LABEL)];
  [alert setAlertStyle:NSInformationalAlertStyle];

  base::scoped_nsobject<NSButton> application_folder_checkbox(
      [[NSButton alloc] initWithFrame:NSZeroRect]);
  [application_folder_checkbox setButtonType:NSSwitchButton];
  [application_folder_checkbox
      setTitle:l10n_util::GetNSString(IDS_CREATE_SHORTCUTS_APP_FOLDER_CHKBOX)];
  [application_folder_checkbox setState:NSOnState];
  [application_folder_checkbox sizeToFit];
  [alert setAccessoryView:application_folder_checkbox];

  const int kIconPreviewSizePixels = 128;
  const int kIconPreviewTargetSize = 64;
  const gfx::Image* icon = shortcut_info.favicon.GetBest(
      kIconPreviewSizePixels, kIconPreviewSizePixels);

  if (icon && !icon->IsEmpty()) {
    NSImage* icon_image = icon->ToNSImage();
    [icon_image
        setSize:NSMakeSize(kIconPreviewTargetSize, kIconPreviewTargetSize)];
    [alert setIcon:icon_image];
  }

  bool dialog_accepted = false;
  if ([alert runModal] == NSAlertFirstButtonReturn &&
      [application_folder_checkbox state] == NSOnState) {
    dialog_accepted = true;
    CreateShortcuts(
        SHORTCUT_CREATION_BY_USER, ShortcutLocations(), profile, app);
  }

  if (!close_callback.is_null())
    close_callback.Run(dialog_accepted);
}

void UpdateShortcutsForAllApps(Profile* profile,
                               const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile);
  if (!registry)
    return;

  // Update all apps.
  scoped_ptr<extensions::ExtensionSet> everything =
      registry->GenerateInstalledExtensionsSet();
  for (extensions::ExtensionSet::const_iterator it = everything->begin();
       it != everything->end(); ++it) {
    if (web_app::ShouldCreateShortcutFor(profile, it->get()))
      web_app::UpdateAllShortcuts(base::string16(), profile, it->get());
  }

  callback.Run();
}

namespace internals {

bool CreatePlatformShortcuts(
    const base::FilePath& app_data_path,
    const ShortcutInfo& shortcut_info,
    const extensions::FileHandlersInfo& file_handlers_info,
    const ShortcutLocations& creation_locations,
    ShortcutCreationReason creation_reason) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  if (AppShimsDisabledForTest())
    return true;

  WebAppShortcutCreator shortcut_creator(
      app_data_path, shortcut_info, file_handlers_info);
  return shortcut_creator.CreateShortcuts(creation_reason, creation_locations);
}

void DeletePlatformShortcuts(const base::FilePath& app_data_path,
                             const ShortcutInfo& shortcut_info) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  WebAppShortcutCreator shortcut_creator(
      app_data_path, shortcut_info, extensions::FileHandlersInfo());
  shortcut_creator.DeleteShortcuts();
}

void UpdatePlatformShortcuts(
    const base::FilePath& app_data_path,
    const base::string16& old_app_title,
    const ShortcutInfo& shortcut_info,
    const extensions::FileHandlersInfo& file_handlers_info) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  if (AppShimsDisabledForTest() &&
      !g_app_shims_allow_update_and_launch_in_tests) {
    return;
  }

  WebAppShortcutCreator shortcut_creator(
      app_data_path, shortcut_info, file_handlers_info);
  shortcut_creator.UpdateShortcuts();
}

void DeleteAllShortcutsForProfile(const base::FilePath& profile_path) {
  const std::string profile_base_name = profile_path.BaseName().value();
  std::vector<base::FilePath> bundles = GetAllAppBundlesInPath(
      profile_path.Append(chrome::kWebAppDirname), profile_base_name);

  for (std::vector<base::FilePath>::const_iterator it = bundles.begin();
       it != bundles.end(); ++it) {
    web_app::ShortcutInfo shortcut_info =
        BuildShortcutInfoFromBundle(*it);
    WebAppShortcutCreator shortcut_creator(
        it->DirName(), shortcut_info, extensions::FileHandlersInfo());
    shortcut_creator.DeleteShortcuts();
  }
}

}  // namespace internals

}  // namespace web_app

namespace chrome {

void ShowCreateChromeAppShortcutsDialog(
    gfx::NativeWindow /*parent_window*/,
    Profile* profile,
    const extensions::Extension* app,
    const base::Callback<void(bool)>& close_callback) {
  web_app::GetShortcutInfoForApp(
      app,
      profile,
      base::Bind(&web_app::CreateAppShortcutInfoLoaded,
                 profile,
                 app,
                 close_callback));
}

}  // namespace chrome
