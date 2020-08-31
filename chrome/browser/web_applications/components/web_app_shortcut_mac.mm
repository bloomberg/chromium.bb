// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/web_applications/components/web_app_shortcut_mac.h"

#import <Cocoa/Cocoa.h>
#include <stdint.h>

#include <algorithm>
#include <list>
#include <map>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#import "base/mac/launch_services_util.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/process/process_handle.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/version.h"
#import "chrome/browser/mac/dock.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#import "chrome/common/mac/app_mode_common.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "components/crx_file/id_util.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#import "skia/ext/skia_utils_mac.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/utils/mac/SkCGUtils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_family.h"

// A TerminationObserver observes a NSRunningApplication for when it
// terminates. On termination, it will run the specified callback on the UI
// thread and release itself.
@interface TerminationObserver : NSObject {
  base::scoped_nsobject<NSRunningApplication> _app;
  base::OnceClosure _callback;
}
- (id)initWithRunningApplication:(NSRunningApplication*)app
                        callback:(base::OnceClosure)callback;
@end

@implementation TerminationObserver
- (id)initWithRunningApplication:(NSRunningApplication*)app
                        callback:(base::OnceClosure)callback {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (self = [super init]) {
    _callback = std::move(callback);
    _app.reset(app, base::scoped_policy::RETAIN);
    // Note that |observeValueForKeyPath| will be called with the initial value
    // within the |addObserver| call.
    [_app addObserver:self
           forKeyPath:@"isTerminated"
              options:NSKeyValueObservingOptionNew |
                      NSKeyValueObservingOptionInitial
              context:nullptr];
  }
  return self;
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  NSNumber* newNumberValue = [change objectForKey:NSKeyValueChangeNewKey];
  BOOL newValue = [newNumberValue boolValue];
  if (newValue) {
    base::scoped_nsobject<TerminationObserver> scoped_self(
        self, base::scoped_policy::RETAIN);
    base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                   base::BindOnce(
                       [](base::scoped_nsobject<TerminationObserver> observer) {
                         [observer onTerminated];
                       },
                       scoped_self));
  }
}

- (void)onTerminated {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // If |onTerminated| is called repeatedly (which in theory it should not),
  // then ensure that we only call removeObserver and release once by doing an
  // early-out if |callback_| has already been made.
  if (!_callback)
    return;
  std::move(_callback).Run();
  DCHECK(!_callback);
  [_app removeObserver:self forKeyPath:@"isTerminated" context:nullptr];
  [self release];
}
@end

// TODO(https://crbug.com/941909): Change all launch functions to take a single
// callback that returns a NSRunningApplication, rather than separate launch and
// termination callbacks.
void RunAppLaunchCallbacks(
    base::scoped_nsobject<NSRunningApplication> app,
    base::OnceCallback<void(base::Process)> launch_callback,
    base::OnceClosure termination_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(app);

  // If the app doesn't have a valid pid, or if the application has been
  // terminated, then indicate failure in |launch_callback|.
  base::Process process([app processIdentifier]);
  if (!process.IsValid() || [app isTerminated]) {
    std::move(launch_callback).Run(base::Process());
    return;
  }

  // Otherwise, indicate successful launch, and watch for termination.
  // TODO(https://crbug.com/941909): This watches for termination indefinitely,
  // but we only need to watch for termination until the app establishes a
  // (whereupon termination will be noticed by the mojo connection closing).
  std::move(launch_callback).Run(std::move(process));
  [[TerminationObserver alloc]
      initWithRunningApplication:app
                        callback:std::move(termination_callback)];
}

bool g_app_shims_allow_update_and_launch_in_tests = false;

namespace web_app {

namespace {

// UMA metric name for creating shortcut result.
constexpr const char* kCreateShortcutResult = "Apps.CreateShortcuts.Mac.Result";

// Result of creating app shortcut.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class CreateShortcutResult {
  kSuccess = 0,
  kApplicationDirNotFound = 1,
  kFailToLocalizeApplication = 2,
  kFailToGetApplicationPaths = 3,
  kFailToCreateTempDir = 4,
  kStagingDirectoryNotExist = 5,
  kFailToCreateExecutablePath = 6,
  kFailToCopyExecutablePath = 7,
  kFailToCopyPlist = 8,
  kFailToWritePkgInfoFile = 9,
  kFailToUpdatePlist = 10,
  kFailToUpdateDisplayName = 11,
  kFailToUpdateIcon = 12,
  kFailToCreateParentDir = 13,
  kFailToCopyApp = 14,
  kMaxValue = kFailToCopyApp
};

// Records the result of creating shortcut to UMA.
void RecordCreateShortcut(CreateShortcutResult result) {
  UMA_HISTOGRAM_ENUMERATION(kCreateShortcutResult, result);
}

// The maximum number to append to to an app name before giving up and using the
// extension id.
constexpr int kMaxConflictNumber = 999;

// Writes |icons| to |path| in .icns format.
bool WriteIconsToFile(const std::vector<gfx::Image>& icons,
                      const base::FilePath& path) {
  base::scoped_nsobject<NSMutableData> data(
      [[NSMutableData alloc] initWithCapacity:0]);
  base::ScopedCFTypeRef<CGImageDestinationRef> image_destination(
      CGImageDestinationCreateWithData(base::mac::NSToCFCast(data),
                                       kUTTypeAppleICNS, icons.size(),
                                       nullptr));
  DCHECK(image_destination);
  for (const gfx::Image& image : icons) {
    base::ScopedCFTypeRef<CGImageRef> cg_image(SkCreateCGImageRefWithColorspace(
        image.AsBitmap(), base::mac::GetSRGBColorSpace()));
    CGImageDestinationAddImage(image_destination, cg_image, nullptr);
  }
  if (!CGImageDestinationFinalize(image_destination)) {
    NOTREACHED() << "CGImageDestinationFinalize failed.";
    return false;
  }
  return [data writeToFile:base::mac::FilePathToNSString(path) atomically:NO];
}

// Returns true if |image| can be used for an icon resource.
bool IsImageValidForIcon(const gfx::Image& image) {
  if (image.IsEmpty())
    return false;

  // When called via ShowCreateChromeAppShortcutsDialog the ImageFamily will
  // have all the representations desired here for mac, from the kDesiredSizes
  // array in web_app.cc.
  SkBitmap bitmap = image.AsBitmap();
  if (bitmap.colorType() != kN32_SkColorType ||
      bitmap.width() != bitmap.height()) {
    return false;
  }

  switch (bitmap.width()) {
    case 512:
    case 256:
    case 128:
    case 48:
    case 32:
    case 16:
      return true;
  }
  return false;
}

// Remove the leading . from the entries of |extensions|. Any items that do not
// have a leading . are removed.
std::set<std::string> GetFileHandlerExtensionsWithoutDot(
    const std::set<std::string>& file_extensions) {
  std::set<std::string> result;
  for (const auto& file_extension : file_extensions) {
    if (file_extension.length() <= 1 || file_extension[0] != '.')
      continue;
    result.insert(file_extension.substr(1));
  }
  return result;
}

bool AppShimCreationDisabledForTest() {
  // Disable app shims in tests because shims created in ~/Applications will not
  // be cleaned up.
  return base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestType);
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

// Given the path to an app bundle, return the path to the Info.plist file.
NSString* GetPlistPath(const base::FilePath& bundle_path) {
  return base::mac::FilePathToNSString(
      bundle_path.Append("Contents").Append("Info.plist"));
}

// Data and helpers for an Info.plist under a given bundle path.
class BundleInfoPlist {
 public:
  // Retrieve info from all app shims found in |apps_path|.
  static std::list<BundleInfoPlist> GetAllInPath(
      const base::FilePath& apps_path,
      bool recursive) {
    std::list<BundleInfoPlist> bundles;
    base::FileEnumerator enumerator(apps_path, recursive,
                                    base::FileEnumerator::DIRECTORIES);
    for (base::FilePath shim_path = enumerator.Next(); !shim_path.empty();
         shim_path = enumerator.Next()) {
      bundles.emplace_back(shim_path);
    }
    return bundles;
  }

  // Retrieve info from the specified app shim in |bundle_path|.
  explicit BundleInfoPlist(const base::FilePath& bundle_path)
      : bundle_path_(bundle_path) {
    NSString* plist_path = GetPlistPath(bundle_path_);
    plist_.reset([NSDictionary dictionaryWithContentsOfFile:plist_path],
                 base::scoped_policy::RETAIN);
  }
  BundleInfoPlist(const BundleInfoPlist& other) = default;
  BundleInfoPlist& operator=(const BundleInfoPlist& other) = default;
  ~BundleInfoPlist() = default;

  const base::FilePath& bundle_path() const { return bundle_path_; }

  // Checks that the CrAppModeUserDataDir in the Info.plist is, or is a subpath
  // of the current user_data_dir. This uses starts with instead of equals
  // because the CrAppModeUserDataDir could be the user_data_dir or the
  // |app_data_dir_|.
  bool IsForCurrentUserDataDir() const {
    base::FilePath user_data_dir;
    base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
    DCHECK(!user_data_dir.empty());
    return base::StartsWith(
        base::SysNSStringToUTF8(
            [plist_ valueForKey:app_mode::kCrAppModeUserDataDirKey]),
        user_data_dir.value(), base::CompareCase::SENSITIVE);
  }

  // Checks if kCrAppModeProfileDirKey corresponds to the specified profile
  // path.
  bool IsForProfile(const base::FilePath& test_profile_path) const {
    std::string profile_path = base::SysNSStringToUTF8(
        [plist_ valueForKey:app_mode::kCrAppModeProfileDirKey]);
    return profile_path == test_profile_path.BaseName().value();
  }

  // Return the full profile path (as a subpath of the user_data_dir).
  base::FilePath GetFullProfilePath() const {
    // Figure out the profile_path. Since the user_data_dir could contain the
    // path to the web app data dir.
    base::FilePath user_data_dir = base::mac::NSStringToFilePath(
        [plist_ valueForKey:app_mode::kCrAppModeUserDataDirKey]);
    base::FilePath profile_base_name = base::mac::NSStringToFilePath(
        [plist_ valueForKey:app_mode::kCrAppModeProfileDirKey]);
    if (user_data_dir.DirName().DirName().BaseName() == profile_base_name)
      return user_data_dir.DirName().DirName();
    return user_data_dir.Append(profile_base_name);
  }

  std::string GetExtensionId() const {
    return base::SysNSStringToUTF8(
        [plist_ valueForKey:app_mode::kCrAppModeShortcutIDKey]);
  }
  std::string GetProfileName() const {
    return base::SysNSStringToUTF8(
        [plist_ valueForKey:app_mode::kCrAppModeProfileNameKey]);
  }
  GURL GetURL() const {
    return GURL(base::SysNSStringToUTF8(
        [plist_ valueForKey:app_mode::kCrAppModeShortcutURLKey]));
  }
  base::string16 GetTitle() const {
    return base::SysNSStringToUTF16(
        [plist_ valueForKey:app_mode::kCrAppModeShortcutNameKey]);
  }
  base::Version GetVersion() const {
    NSString* version_string =
        [plist_ valueForKey:app_mode::kCrBundleVersionKey];
    if (!version_string) {
      // Older bundles have the Chrome version in the following key.
      version_string =
          [plist_ valueForKey:app_mode::kCFBundleShortVersionStringKey];
    }
    return base::Version(base::SysNSStringToUTF8(version_string));
  }
  std::string GetBundleId() const {
    return base::SysNSStringToUTF8(
        [plist_ valueForKey:base::mac::CFToNSCast(kCFBundleIdentifierKey)]);
  }

 private:
  // The path of the app bundle from this this info was read.
  base::FilePath bundle_path_;

  // Data read from the Info.plist.
  base::scoped_nsobject<NSDictionary> plist_;
};

bool HasExistingExtensionShimForDifferentProfile(
    const base::FilePath& destination_directory,
    const std::string& extension_id,
    const base::FilePath& profile_dir) {
  std::list<BundleInfoPlist> bundles_info = BundleInfoPlist::GetAllInPath(
      destination_directory, false /* recursive */);
  for (const auto& info : bundles_info) {
    if (info.GetExtensionId() == extension_id &&
        !info.IsForProfile(profile_dir)) {
      return true;
    }
  }
  return false;
}

void LaunchShimOnFileThread(LaunchShimUpdateBehavior update_behavior,
                            ShimLaunchedCallback launched_callback,
                            ShimTerminatedCallback terminated_callback,
                            const ShortcutInfo& shortcut_info) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  WebAppShortcutCreator shortcut_creator(
      internals::GetShortcutDataDir(shortcut_info), &shortcut_info);

  // Recreate shims if requested, and populate |shim_paths| with the paths to
  // attempt to launch.
  bool launched_after_rebuild = false;
  std::vector<base::FilePath> shim_paths;
  switch (update_behavior) {
    case LaunchShimUpdateBehavior::DO_NOT_RECREATE:
      // Attempt to locate the shim's path using LaunchServices.
      shim_paths = shortcut_creator.GetAppBundlesById();
      break;
    case LaunchShimUpdateBehavior::RECREATE_IF_INSTALLED:
      // Only attempt to launch shims that were updated.
      launched_after_rebuild = true;
      shortcut_creator.UpdateShortcuts(false /* create_if_needed */,
                                       &shim_paths);
      break;
    case LaunchShimUpdateBehavior::RECREATE_UNCONDITIONALLY:
      // Likewise, only attempt to launch shims that were updated.
      launched_after_rebuild = true;
      shortcut_creator.UpdateShortcuts(true /* create_if_needed */,
                                       &shim_paths);
      break;
  }

  // Attempt to launch the shim.
  for (const auto& shim_path : shim_paths) {
    if (!base::PathExists(shim_path))
      continue;

    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    command_line.AppendSwitchASCII(
        app_mode::kLaunchedByChromeProcessId,
        base::NumberToString(base::GetCurrentProcId()));
    if (launched_after_rebuild)
      command_line.AppendSwitch(app_mode::kLaunchedAfterRebuild);

    // Launch without activating (NSWorkspaceLaunchWithoutActivation).
    base::scoped_nsobject<NSRunningApplication> app(
        base::mac::OpenApplicationWithPath(
            shim_path, command_line,
            NSWorkspaceLaunchDefault | NSWorkspaceLaunchWithoutActivation),
        base::scoped_policy::RETAIN);
    if (app) {
      base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                     base::BindOnce(&RunAppLaunchCallbacks, app,
                                    std::move(launched_callback),
                                    std::move(terminated_callback)));
      return;
    }
  }

  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::BindOnce(std::move(launched_callback), base::Process()));
}

base::FilePath GetLocalizableAppShortcutsSubdirName() {
  static const char kChromiumAppDirName[] = "Chromium Apps.localized";
  static const char kChromeAppDirName[] = "Chrome Apps.localized";
  static const char kChromeCanaryAppDirName[] = "Chrome Canary Apps.localized";

  switch (chrome::GetChannel()) {
    case version_info::Channel::UNKNOWN:
      return base::FilePath(kChromiumAppDirName);

    case version_info::Channel::CANARY:
      return base::FilePath(kChromeCanaryAppDirName);

    default:
      return base::FilePath(kChromeAppDirName);
  }
}

base::FilePath* GetOverriddenApplicationsFolder() {
  static base::NoDestructor<base::FilePath> overridden_path;
  return overridden_path.get();
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
base::scoped_nsobject<NSImageRep> ImageRepForGFXImage(const gfx::Image& image) {
  NSArray* image_reps = [image.AsNSImage() representations];
  DCHECK_EQ(1u, [image_reps count]);
  return base::scoped_nsobject<NSImageRep>([image_reps objectAtIndex:0],
                                           base::scoped_policy::RETAIN);
}

using ResourceIDToImage = std::map<int, base::scoped_nsobject<NSImageRep>>;

// Generates a map of NSImageReps used by SetWorkspaceIconOnFILEThread and
// passes it to |io_task|. Since ui::ResourceBundle can only be used on UI
// thread, this function also needs to run on UI thread, and the gfx::Images
// need to be converted to NSImageReps on the UI thread due to non-thread-safety
// of gfx::Image.
void GetImageResourcesOnUIThread(
    base::OnceCallback<void(std::unique_ptr<ResourceIDToImage>)> io_task) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  ui::ResourceBundle& resource_bundle = ui::ResourceBundle::GetSharedInstance();
  std::unique_ptr<ResourceIDToImage> result =
      std::make_unique<ResourceIDToImage>();

  // These resource ID should match to the ones used by
  // SetWorkspaceIconOnFILEThread below.
  for (int id : {IDR_APPS_FOLDER_16, IDR_APPS_FOLDER_32,
                 IDR_APPS_FOLDER_OVERLAY_128, IDR_APPS_FOLDER_OVERLAY_512}) {
    gfx::Image image = resource_bundle.GetNativeImageNamed(id);
    (*result)[id] = ImageRepForGFXImage(image);
  }

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(std::move(io_task), std::move(result)));
}

void SetWorkspaceIconOnWorkerThread(const base::FilePath& apps_directory,
                                    std::unique_ptr<ResourceIDToImage> images) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  base::scoped_nsobject<NSImage> folder_icon_image([[NSImage alloc] init]);
  // Use complete assets for the small icon sizes. -[NSWorkspace setIcon:] has a
  // bug when dealing with named NSImages where it incorrectly handles alpha
  // premultiplication. This is most noticable with small assets since the 1px
  // border is a much larger component of the small icons.
  // See http://crbug.com/305373 for details.
  for (int id : {IDR_APPS_FOLDER_16, IDR_APPS_FOLDER_32}) {
    const auto& found = images->find(id);
    DCHECK(found != images->end());
    [folder_icon_image addRepresentation:found->second];
  }

  // Brand larger folder assets with an embossed app launcher logo to
  // conserve distro size and for better consistency with changing hue
  // across OSX versions. The folder is textured, so compresses poorly
  // without this.
  NSImage* base_image = [NSImage imageNamed:NSImageNameFolder];
  for (int id : {IDR_APPS_FOLDER_OVERLAY_128, IDR_APPS_FOLDER_OVERLAY_512}) {
    const auto& found = images->find(id);
    DCHECK(found != images->end());
    NSImageRep* with_overlay = OverlayImageRep(base_image, found->second);
    DCHECK(with_overlay);
    if (with_overlay)
      [folder_icon_image addRepresentation:with_overlay];
  }
  [[NSWorkspace sharedWorkspace]
      setIcon:folder_icon_image
      forFile:base::mac::FilePathToNSString(apps_directory)
      options:0];
}

// Adds a localized strings file for the Chrome Apps directory using the current
// locale. OSX will use this for the display name.
// + Chrome Apps.localized (|apps_directory|)
// | + .localized
// | | en.strings
// | | de.strings
bool UpdateAppShortcutsSubdirLocalizedName(
    const base::FilePath& apps_directory) {
  base::FilePath localized = apps_directory.Append(".localized");
  if (!base::CreateDirectory(localized))
    return false;

  base::FilePath directory_name = apps_directory.BaseName().RemoveExtension();
  base::string16 localized_name =
      shell_integration::GetAppShortcutsSubdirName();
  NSDictionary* strings_dict = @{
    base::mac::FilePathToNSString(directory_name) :
        base::SysUTF16ToNSString(localized_name)
  };

  std::string locale = l10n_util::NormalizeLocale(
      l10n_util::GetApplicationLocale(std::string()));

  NSString* strings_path =
      base::mac::FilePathToNSString(localized.Append(locale + ".strings"));
  [strings_dict writeToFile:strings_path atomically:YES];

  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::BindOnce(&GetImageResourcesOnUIThread,
                                base::BindOnce(&SetWorkspaceIconOnWorkerThread,
                                               apps_directory)));
  return true;
}

std::unique_ptr<ShortcutInfo> BuildShortcutInfoFromBundle(
    const base::FilePath& bundle_path) {
  BundleInfoPlist bundle_info(bundle_path);
  std::unique_ptr<ShortcutInfo> shortcut_info(new ShortcutInfo);
  shortcut_info->extension_id = bundle_info.GetExtensionId();
  shortcut_info->url = bundle_info.GetURL();
  shortcut_info->title = bundle_info.GetTitle();
  shortcut_info->profile_name = bundle_info.GetProfileName();
  shortcut_info->profile_path = bundle_info.GetFullProfilePath();
  return shortcut_info;
}

base::FilePath GetMultiProfileAppDataDir(base::FilePath app_data_dir) {
  // The kCrAppModeUserDataDirKey is expected to be a path in kWebAppDirname,
  // and the true user data dir is extracted by going three directories up.
  // For profile-agnostic apps, remove this reference to the profile name.
  // TODO(https://crbug.com/1021237): Do not specify kCrAppModeUserDataDirKey
  // if Chrome is using the default user data dir.

  // Strip the app name directory.
  base::FilePath app_name_dir = app_data_dir.BaseName();
  app_data_dir = app_data_dir.DirName();

  // Strip kWebAppDirname.
  base::FilePath web_app_dir = app_data_dir.BaseName();
  app_data_dir = app_data_dir.DirName();

  // Strip the profile and replace it with kNewProfilePath.
  app_data_dir = app_data_dir.DirName();
  const std::string kNewProfilePath("-");
  return app_data_dir.Append(kNewProfilePath)
      .Append(web_app_dir)
      .Append(app_name_dir);
}

// Returns the bundle identifier for an app. If |profile_path| is unset, then
// the returned bundle id will be profile-agnostic.
std::string GetBundleIdentifier(
    const std::string& app_id,
    const base::FilePath& profile_path = base::FilePath()) {
  // Note that this matches APP_MODE_APP_BUNDLE_ID in chrome/chrome.gyp.
  if (!profile_path.empty()) {
    // Replace spaces in the profile path with hyphen.
    std::string normalized_profile_path;
    base::ReplaceChars(profile_path.BaseName().value(), " ", "-",
                       &normalized_profile_path);
    return base::mac::BaseBundleID() + std::string(".app.") +
           normalized_profile_path + "-" + app_id;
  }
  return base::mac::BaseBundleID() + std::string(".app.") + app_id;
}

// Return all bundles with the specified |bundle_id| which are for the current
// user data dir.
std::list<BundleInfoPlist> SearchForBundlesById(const std::string& bundle_id) {
  std::list<BundleInfoPlist> infos;

  // First search using LaunchServices
  base::ScopedCFTypeRef<CFStringRef> bundle_id_cf(
      base::SysUTF8ToCFStringRef(bundle_id));
  base::scoped_nsobject<NSArray> bundle_urls(base::mac::CFToNSCast(
      LSCopyApplicationURLsForBundleIdentifier(bundle_id_cf.get(), nullptr)));
  for (NSURL* url : bundle_urls.get()) {
    NSString* path_string = [url path];
    base::FilePath bundle_path([path_string fileSystemRepresentation]);
    BundleInfoPlist info(bundle_path);
    if (!info.IsForCurrentUserDataDir())
      continue;
    infos.push_back(info);
  }
  if (!infos.empty())
    return infos;

  // LaunchServices can fail to locate a recently-created bundle. Search
  // for an app in the applications folder to handle this case.
  // https://crbug.com/937703
  infos = BundleInfoPlist::GetAllInPath(GetChromeAppsFolder(),
                                        true /* recursive */);
  for (auto it = infos.begin(); it != infos.end();) {
    const BundleInfoPlist& info = *it;
    if (info.GetBundleId() == bundle_id && info.IsForCurrentUserDataDir()) {
      ++it;
    } else {
      infos.erase(it++);
    }
  }
  return infos;
}

}  // namespace

std::unique_ptr<ShortcutInfo> RecordAppShimErrorAndBuildShortcutInfo(
    const base::FilePath& bundle_path) {
  base::Version full_version = BundleInfoPlist(bundle_path).GetVersion();
  uint32_t major_version = 0;
  if (full_version.IsValid())
    major_version = full_version.components()[0];
  base::UmaHistogramSparse("Apps.AppShimErrorVersion", major_version);

  return BuildShortcutInfoFromBundle(bundle_path);
}

bool AppShimLaunchDisabled() {
  return AppShimCreationDisabledForTest() &&
         !g_app_shims_allow_update_and_launch_in_tests;
}

base::FilePath GetChromeAppsFolder() {
  if (!GetOverriddenApplicationsFolder()->empty())
    return *GetOverriddenApplicationsFolder();

  base::FilePath path = GetWritableApplicationsDirectory();
  if (path.empty())
    return path;

  return path.Append(GetLocalizableAppShortcutsSubdirName());
}

void SetChromeAppsFolderForTesting(const base::FilePath& path) {
  *GetOverriddenApplicationsFolder() = path;
}

WebAppShortcutCreator::WebAppShortcutCreator(const base::FilePath& app_data_dir,
                                             const ShortcutInfo* shortcut_info)
    : app_data_dir_(app_data_dir), info_(shortcut_info) {
  DCHECK(shortcut_info);
}

WebAppShortcutCreator::~WebAppShortcutCreator() {}

base::FilePath WebAppShortcutCreator::GetApplicationsShortcutPath(
    bool avoid_conflicts) const {
  if (g_app_shims_allow_update_and_launch_in_tests)
    return app_data_dir_.Append(GetShortcutBasename());

  base::FilePath applications_dir = GetChromeAppsFolder();
  if (applications_dir.empty())
    return base::FilePath();

  if (!avoid_conflicts)
    return applications_dir.Append(GetShortcutBasename());

  // Attempt to use the application's title for the file name. Resolve conflicts
  // by appending 1 through kMaxConflictNumber, before giving up and using the
  // concatenated profile and extension for a name name.
  for (int i = 1; i <= kMaxConflictNumber; ++i) {
    base::FilePath path = applications_dir.Append(GetShortcutBasename(i));
    if (base::DirectoryExists(path))
      continue;
    return path;
  }

  // If all of those are taken, then use the combination of profile and
  // extension id.
  return applications_dir.Append(GetFallbackBasename());
}

base::FilePath WebAppShortcutCreator::GetShortcutBasename(
    int copy_number) const {
  // For profile-less shortcuts, use the fallback naming scheme to avoid change.
  if (info_->profile_name.empty())
    return GetFallbackBasename();

  // Strip all preceding '.'s from the path.
  base::string16 title = info_->title;
  size_t first_non_dot = 0;
  while (first_non_dot < title.size() && title[first_non_dot] == '.')
    first_non_dot += 1;
  title = title.substr(first_non_dot);
  if (title.empty())
    return GetFallbackBasename();

  // Finder will display ':' as '/', so replace all '/' instances with ':'.
  std::replace(title.begin(), title.end(), '/', ':');

  // Append the copy number.
  std::string title_utf8 = base::UTF16ToUTF8(title);
  if (copy_number != 1)
    title_utf8 += base::StringPrintf(" %d", copy_number);
  return base::FilePath(title_utf8 + ".app");
}

base::FilePath WebAppShortcutCreator::GetFallbackBasename() const {
  std::string app_name;
  // Check if there should be a separate shortcut made for different profiles.
  // Such shortcuts will have a |profile_name| set on the ShortcutInfo,
  // otherwise it will be empty.
  if (!info_->profile_name.empty()) {
    app_name += info_->profile_path.BaseName().value();
    app_name += ' ';
  }
  app_name += info_->extension_id;
  return base::FilePath(app_name).ReplaceExtension("app");
}

bool WebAppShortcutCreator::BuildShortcut(
    const base::FilePath& staging_path) const {
  if (!base::DirectoryExists(staging_path.DirName())) {
    RecordCreateShortcut(CreateShortcutResult::kStagingDirectoryNotExist);
    LOG(ERROR) << "Staging path directory does not exist: "
               << staging_path.DirName();
    return false;
  }

  const base::FilePath framework_bundle_path = base::mac::FrameworkBundlePath();

  const base::FilePath executable_path =
      framework_bundle_path.Append("Helpers").Append("app_mode_loader");
  const base::FilePath plist_path =
      framework_bundle_path.Append("Resources").Append("app_mode-Info.plist");

  const base::FilePath destination_contents_path =
      staging_path.Append("Contents");
  const base::FilePath destination_executable_path =
      destination_contents_path.Append("MacOS");

  // First create the .app bundle directory structure.
  // Use NSFileManager so that the permissions can be set appropriately. The
  // base::CreateDirectory() routine forces mode 0700.
  NSError* error = nil;
  if (![[NSFileManager defaultManager]
                 createDirectoryAtURL:base::mac::FilePathToNSURL(
                                          destination_executable_path)
          withIntermediateDirectories:YES
                           attributes:@{
                             NSFilePosixPermissions : @(0755)
                           }
                                error:&error]) {
    RecordCreateShortcut(CreateShortcutResult::kFailToCreateExecutablePath);
    LOG(ERROR) << "Failed to create destination executable path: "
               << destination_executable_path
               << ", error=" << base::SysNSStringToUTF8([error description]);
    return false;
  }

  // Copy the executable file.
  if (!base::CopyFile(executable_path, destination_executable_path.Append(
                                           executable_path.BaseName()))) {
    RecordCreateShortcut(CreateShortcutResult::kFailToCopyExecutablePath);
    LOG(ERROR) << "Failed to copy executable: " << executable_path;
    return false;
  }

  // Copy the Info.plist.
  if (!base::CopyFile(plist_path,
                      destination_contents_path.Append("Info.plist"))) {
    RecordCreateShortcut(CreateShortcutResult::kFailToCopyPlist);
    LOG(ERROR) << "Failed to copy plist: " << plist_path;
    return false;
  }

  // Write the PkgInfo file.
  constexpr char kPkgInfoData[] = "APPL????";
  constexpr size_t kPkgInfoDataSize = base::size(kPkgInfoData) - 1;
  if (base::WriteFile(destination_contents_path.Append("PkgInfo"), kPkgInfoData,
                      kPkgInfoDataSize) != kPkgInfoDataSize) {
    RecordCreateShortcut(CreateShortcutResult::kFailToWritePkgInfoFile);
    LOG(ERROR) << "Failed to write PkgInfo file: " << destination_contents_path;
    return false;
  }

  bool result = UpdatePlist(staging_path);
  if (!result) {
    RecordCreateShortcut(CreateShortcutResult::kFailToUpdatePlist);
    return result;
  }
  result = UpdateDisplayName(staging_path);
  if (!result) {
    RecordCreateShortcut(CreateShortcutResult::kFailToUpdateDisplayName);
    return result;
  }
  result = UpdateIcon(staging_path);
  if (!result) {
    RecordCreateShortcut(CreateShortcutResult::kFailToUpdateIcon);
  }
  return result;
}

void WebAppShortcutCreator::CreateShortcutsAt(
    const std::vector<base::FilePath>& dst_app_paths,
    std::vector<base::FilePath>* updated_paths) const {
  DCHECK(updated_paths && updated_paths->empty());
  DCHECK(!dst_app_paths.empty());

  base::ScopedTempDir scoped_temp_dir;
  if (!scoped_temp_dir.CreateUniqueTempDir()) {
    RecordCreateShortcut(CreateShortcutResult::kFailToCreateTempDir);
    return;
  }

  // Create the bundle in |staging_path|. Note that the staging path will be
  // encoded in CFBundleName, and only .apps with that exact name will have
  // their display name overridden by localization. To that end, use the base
  // name from dst_app_paths.front(), to ensure that the Applications copy has
  // its display name set appropriately.
  base::FilePath staging_path =
      scoped_temp_dir.GetPath().Append(dst_app_paths.front().BaseName());
  if (!BuildShortcut(staging_path))
    return;

  // Copy to each destination in |dst_app_paths|.
  for (const auto& dst_app_path : dst_app_paths) {
    // Create the parent directory for the app.
    base::FilePath dst_parent_dir = dst_app_path.DirName();
    if (!base::CreateDirectory(dst_parent_dir)) {
      RecordCreateShortcut(CreateShortcutResult::kFailToCreateParentDir);
      LOG(ERROR) << "Creating directory " << dst_parent_dir.value()
                 << " failed.";
      continue;
    }

    // Delete any old copies that may exist.
    base::DeleteFileRecursively(dst_app_path);

    // Copy the bundle to |dst_app_path|.
    if (!base::CopyDirectory(staging_path, dst_app_path, true)) {
      RecordCreateShortcut(CreateShortcutResult::kFailToCopyApp);
      LOG(ERROR) << "Copying app to dst dir: " << dst_parent_dir.value()
                 << " failed";
      continue;
    }

    // Remove the quarantine attribute from both the bundle and the executable.
    base::mac::RemoveQuarantineAttribute(dst_app_path);
    base::mac::RemoveQuarantineAttribute(dst_app_path.Append("Contents")
                                             .Append("MacOS")
                                             .Append("app_mode_loader"));
    updated_paths->push_back(dst_app_path);
  }
}

bool WebAppShortcutCreator::CreateShortcuts(
    ShortcutCreationReason creation_reason,
    ShortcutLocations creation_locations) {
  DCHECK_NE(creation_locations.applications_menu_location,
            APP_MENU_LOCATION_HIDDEN);
  std::vector<base::FilePath> updated_app_paths;
  if (!UpdateShortcuts(true /* create_if_needed */, &updated_app_paths))
    return false;
  if (creation_reason == SHORTCUT_CREATION_BY_USER)
    RevealAppShimInFinder(updated_app_paths[0]);
  RecordCreateShortcut(CreateShortcutResult::kSuccess);
  return true;
}

bool WebAppShortcutCreator::UpdateShortcuts(
    bool create_if_needed,
    std::vector<base::FilePath>* updated_paths) {
  DCHECK(updated_paths && updated_paths->empty());

  if (create_if_needed) {
    const base::FilePath applications_dir = GetChromeAppsFolder();
    if (applications_dir.empty() ||
        !base::DirectoryExists(applications_dir.DirName())) {
      RecordCreateShortcut(CreateShortcutResult::kApplicationDirNotFound);
      LOG(ERROR) << "Couldn't find an Applications directory to copy app to.";
      return false;
    }
    // Only set folder icons and a localized name once. This avoids concurrent
    // calls to -[NSWorkspace setIcon:..], which is not reentrant.
    static bool once = UpdateAppShortcutsSubdirLocalizedName(applications_dir);
    if (!once) {
      RecordCreateShortcut(CreateShortcutResult::kFailToLocalizeApplication);
      LOG(ERROR) << "Failed to localize " << applications_dir.value();
    }
  }

  // Get the list of paths to (re)create by bundle id (wherever it was moved
  // or copied by the user).
  std::vector<base::FilePath> app_paths = GetAppBundlesById();

  // If that path does not exist, create a new entry in ~/Applications if
  // requested.
  if (app_paths.empty() && create_if_needed) {
    app_paths.push_back(
        GetApplicationsShortcutPath(true /* avoid_conflicts */));
  }
  if (app_paths.empty()) {
    RecordCreateShortcut(CreateShortcutResult::kFailToGetApplicationPaths);
    return false;
  }

  CreateShortcutsAt(app_paths, updated_paths);
  return updated_paths->size() == app_paths.size();
}

bool WebAppShortcutCreator::UpdatePlist(const base::FilePath& app_path) const {
  NSString* extension_id = base::SysUTF8ToNSString(info_->extension_id);
  NSString* extension_title = base::SysUTF16ToNSString(info_->title);
  NSString* extension_url = base::SysUTF8ToNSString(info_->url.spec());
  NSString* chrome_bundle_id =
      base::SysUTF8ToNSString(base::mac::BaseBundleID());
  NSDictionary* replacement_dict = [NSDictionary
      dictionaryWithObjectsAndKeys:
          extension_id, app_mode::kShortcutIdPlaceholder, extension_title,
          app_mode::kShortcutNamePlaceholder, extension_url,
          app_mode::kShortcutURLPlaceholder, chrome_bundle_id,
          app_mode::kShortcutBrowserBundleIDPlaceholder, nil];

  NSString* plist_path = GetPlistPath(app_path);
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
  [plist setObject:base::SysUTF8ToNSString(version_info::GetVersionNumber())
            forKey:app_mode::kCrBundleVersionKey];
  [plist setObject:base::SysUTF8ToNSString(info_->version_for_display)
            forKey:app_mode::kCFBundleShortVersionStringKey];
  if (IsMultiProfile()) {
    [plist setObject:base::SysUTF8ToNSString(
                         GetBundleIdentifier(info_->extension_id))
              forKey:base::mac::CFToNSCast(kCFBundleIdentifierKey)];
    base::FilePath data_dir = GetMultiProfileAppDataDir(app_data_dir_);
    [plist setObject:base::mac::FilePathToNSString(data_dir)
              forKey:app_mode::kCrAppModeUserDataDirKey];
  } else {
    [plist setObject:base::SysUTF8ToNSString(GetBundleIdentifier(
                         info_->extension_id, info_->profile_path))
              forKey:base::mac::CFToNSCast(kCFBundleIdentifierKey)];
    [plist setObject:base::mac::FilePathToNSString(app_data_dir_)
              forKey:app_mode::kCrAppModeUserDataDirKey];
    [plist
        setObject:base::mac::FilePathToNSString(info_->profile_path.BaseName())
           forKey:app_mode::kCrAppModeProfileDirKey];
    [plist setObject:base::SysUTF8ToNSString(info_->profile_name)
              forKey:app_mode::kCrAppModeProfileNameKey];
  }
  [plist setObject:[NSNumber numberWithBool:YES]
            forKey:app_mode::kLSHasLocalizedDisplayNameKey];
  [plist setObject:[NSNumber numberWithBool:YES]
            forKey:app_mode::kNSHighResolutionCapableKey];

  // 3. Fill in file handlers.
  const auto file_handler_extensions =
      GetFileHandlerExtensionsWithoutDot(info_->file_handler_extensions);
  if (!file_handler_extensions.empty() ||
      !info_->file_handler_mime_types.empty()) {
    base::scoped_nsobject<NSMutableArray> doc_types_value(
        [[NSMutableArray alloc] init]);
    base::scoped_nsobject<NSMutableDictionary> doc_types_dict(
        [[NSMutableDictionary alloc] init]);
    if (!file_handler_extensions.empty()) {
      base::scoped_nsobject<NSMutableArray> extensions(
          [[NSMutableArray alloc] init]);
      for (const auto& file_extension : file_handler_extensions)
        [extensions addObject:base::SysUTF8ToNSString(file_extension)];
      [doc_types_dict setObject:extensions
                         forKey:app_mode::kCFBundleTypeExtensionsKey];
    }
    if (!info_->file_handler_mime_types.empty()) {
      base::scoped_nsobject<NSMutableArray> mime_types(
          [[NSMutableArray alloc] init]);
      for (const auto& mime_type : info_->file_handler_mime_types)
        [mime_types addObject:base::SysUTF8ToNSString(mime_type)];
      [doc_types_dict setObject:mime_types
                         forKey:app_mode::kCFBundleTypeMIMETypesKey];
    }
    [doc_types_value addObject:doc_types_dict];
    [plist setObject:doc_types_value
              forKey:app_mode::kCFBundleDocumentTypesKey];
  }

  if (IsMultiProfile()) {
    [plist setObject:base::SysUTF16ToNSString(info_->title)
              forKey:base::mac::CFToNSCast(kCFBundleNameKey)];
  } else {
    // The appropriate bundle name is |info_->title|. Avoiding changing the
    // behavior of non-multi-profile apps when fixing
    // https://crbug.com/1021804.
    base::FilePath app_name = app_path.BaseName().RemoveFinalExtension();
    [plist setObject:base::mac::FilePathToNSString(app_name)
              forKey:base::mac::CFToNSCast(kCFBundleNameKey)];
  }

  return [plist writeToFile:plist_path atomically:YES];
}

bool WebAppShortcutCreator::UpdateDisplayName(
    const base::FilePath& app_path) const {
  // Localization is used to display the app name (rather than the bundle
  // filename). OSX searches for the best language in the order of preferred
  // languages, but one of them must be found otherwise it will default to
  // the filename.
  NSString* language = [[NSLocale preferredLanguages] objectAtIndex:0];
  base::FilePath localized_dir = GetResourcesPath(app_path).Append(
      base::SysNSStringToUTF8(language) + ".lproj");
  if (!base::CreateDirectory(localized_dir))
    return false;

  NSString* bundle_name = base::SysUTF16ToNSString(info_->title);
  NSString* display_name = base::SysUTF16ToNSString(info_->title);
  if (!IsMultiProfile() &&
      HasExistingExtensionShimForDifferentProfile(
          GetChromeAppsFolder(), info_->extension_id, info_->profile_path)) {
    display_name = [bundle_name
        stringByAppendingString:base::SysUTF8ToNSString(
                                    " (" + info_->profile_name + ")")];
  }

  NSDictionary* strings_plist = @{
    base::mac::CFToNSCast(kCFBundleNameKey) : bundle_name,
    app_mode::kCFBundleDisplayNameKey : display_name
  };

  NSString* localized_path =
      base::mac::FilePathToNSString(localized_dir.Append("InfoPlist.strings"));
  return [strings_plist writeToFile:localized_path atomically:YES];
}

bool WebAppShortcutCreator::UpdateIcon(const base::FilePath& app_path) const {
  if (info_->favicon.empty())
    return true;

  std::vector<gfx::Image> valid_icons;
  for (gfx::ImageFamily::const_iterator it = info_->favicon.begin();
       it != info_->favicon.end(); ++it) {
    if (IsImageValidForIcon(*it))
      valid_icons.push_back(*it);
  }
  if (valid_icons.empty())
    return false;

  base::FilePath resources_path = GetResourcesPath(app_path);
  if (!base::CreateDirectory(resources_path))
    return false;

  return WriteIconsToFile(valid_icons, resources_path.Append("app.icns"));
}

std::vector<base::FilePath> WebAppShortcutCreator::GetAppBundlesByIdUnsorted()
    const {
  base::scoped_nsobject<NSMutableArray> urls([[NSMutableArray alloc] init]);

  // Search using LaunchServices using the default bundle id.
  const std::string bundle_id = GetBundleIdentifier(
      info_->extension_id,
      IsMultiProfile() ? base::FilePath() : info_->profile_path);
  auto bundle_infos = SearchForBundlesById(bundle_id);

  // If in multi-profile mode, search using the profile-scoped bundle id, in
  // case the user has an old shim hanging around.
  if (bundle_infos.empty() && IsMultiProfile()) {
    const std::string profile_scoped_bundle_id =
        GetBundleIdentifier(info_->extension_id, info_->profile_path);
    bundle_infos = SearchForBundlesById(profile_scoped_bundle_id);
  }

  std::vector<base::FilePath> bundle_paths;
  for (const auto& bundle_info : bundle_infos)
    bundle_paths.push_back(bundle_info.bundle_path());
  return bundle_paths;
}

std::vector<base::FilePath> WebAppShortcutCreator::GetAppBundlesById() const {
  std::vector<base::FilePath> paths = GetAppBundlesByIdUnsorted();

  // Sort the matches by preference.
  base::FilePath default_path =
      GetApplicationsShortcutPath(false /* avoid_conflicts */);

  // When testing, use only the default path.
  if (g_app_shims_allow_update_and_launch_in_tests) {
    paths.clear();
    if (base::PathExists(default_path))
      paths.push_back(default_path);
    return paths;
  }

  base::FilePath apps_dir = GetChromeAppsFolder();
  auto compare = [default_path, apps_dir](const base::FilePath& a,
                                          const base::FilePath& b) {
    if (a == b)
      return false;
    // The default install path is preferred above all others.
    if (a == default_path)
      return true;
    if (b == default_path)
      return false;
    // Paths in ~/Applications are preferred to paths not in ~/Applications.
    bool a_in_apps_dir = apps_dir.IsParent(a);
    bool b_in_apps_dir = apps_dir.IsParent(b);
    if (a_in_apps_dir != b_in_apps_dir)
      return a_in_apps_dir > b_in_apps_dir;
    return a < b;
  };
  std::sort(paths.begin(), paths.end(), compare);
  return paths;
}

bool WebAppShortcutCreator::IsMultiProfile() const {
  // Only PWAs and bookmark apps are multi-profile capable.
  return info_->url.is_valid();
}

void WebAppShortcutCreator::RevealAppShimInFinder(
    const base::FilePath& app_path) const {
  auto closure = base::BindOnce(
      [](const base::FilePath& app_path) {
        // Use selectFile to show the contents of parent directory with the app
        // shim selected.
        [[NSWorkspace sharedWorkspace]
                          selectFile:base::mac::FilePathToNSString(app_path)
            inFileViewerRootedAtPath:@""];
      },
      app_path);
  // Perform the call to NSWorkSpace on the UI thread. Calling it on the IO
  // thread appears to cause crashes.
  // https://crbug.com/1067367
  base::PostTask(FROM_HERE, {content::BrowserThread::UI}, std::move(closure));
}

void LaunchShim(LaunchShimUpdateBehavior update_behavior,
                ShimLaunchedCallback launched_callback,
                ShimTerminatedCallback terminated_callback,
                std::unique_ptr<ShortcutInfo> shortcut_info) {
  if (AppShimLaunchDisabled()) {
    base::PostTask(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(std::move(launched_callback), base::Process()));
    return;
  }

  internals::PostShortcutIOTask(
      base::BindOnce(&LaunchShimOnFileThread, update_behavior,
                     std::move(launched_callback),
                     std::move(terminated_callback)),
      std::move(shortcut_info));
}

namespace internals {

bool CreatePlatformShortcuts(const base::FilePath& app_data_path,
                             const ShortcutLocations& creation_locations,
                             ShortcutCreationReason creation_reason,
                             const ShortcutInfo& shortcut_info) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  if (AppShimCreationDisabledForTest())
    return true;

  WebAppShortcutCreator shortcut_creator(app_data_path, &shortcut_info);
  return shortcut_creator.CreateShortcuts(creation_reason, creation_locations);
}

void DeletePlatformShortcuts(const base::FilePath& app_data_path,
                             const ShortcutInfo& shortcut_info) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  const std::string bundle_id = GetBundleIdentifier(shortcut_info.extension_id,
                                                    shortcut_info.profile_path);
  auto bundle_infos = SearchForBundlesById(bundle_id);
  for (const auto& bundle_info : bundle_infos)
    base::DeleteFileRecursively(bundle_info.bundle_path());
}

void DeleteMultiProfileShortcutsForApp(const std::string& app_id) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  const std::string bundle_id = GetBundleIdentifier(app_id);
  auto bundle_infos = SearchForBundlesById(bundle_id);
  for (const auto& bundle_info : bundle_infos) {
    base::DeleteFileRecursively(bundle_info.bundle_path());
  }
}

void UpdatePlatformShortcuts(const base::FilePath& app_data_path,
                             const base::string16& old_app_title,
                             const ShortcutInfo& shortcut_info) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  if (AppShimLaunchDisabled())
    return;

  WebAppShortcutCreator shortcut_creator(app_data_path, &shortcut_info);
  std::vector<base::FilePath> updated_shim_paths;
  bool create_if_needed = false;
  // Tests use UpdateAllShortcuts to force shim creation (rather than
  // relying on asynchronous creation at installation.
  if (g_app_shims_allow_update_and_launch_in_tests)
    create_if_needed = true;
  shortcut_creator.UpdateShortcuts(create_if_needed, &updated_shim_paths);
}

void DeleteAllShortcutsForProfile(const base::FilePath& profile_path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  std::list<BundleInfoPlist> bundles_info = BundleInfoPlist::GetAllInPath(
      GetChromeAppsFolder(), true /* recursive */);
  for (const auto& info : bundles_info) {
    if (!info.IsForCurrentUserDataDir())
      continue;
    if (!info.IsForProfile(profile_path))
      continue;
    base::DeleteFileRecursively(info.bundle_path());
  }
}

}  // namespace internals

}  // namespace web_app
