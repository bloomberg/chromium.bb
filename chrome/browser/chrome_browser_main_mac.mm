// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main_mac.h"

#import <Cocoa/Cocoa.h>
#include <sys/sysctl.h>

#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/file_path.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/mac_util.h"
#include "base/memory/scoped_nsobject.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "chrome/app/breakpad_mac.h"
#import "chrome/browser/app_controller_mac.h"
#import "chrome/browser/chrome_browser_application_mac.h"
#include "chrome/browser/mac/install_from_dmg.h"
#include "chrome/browser/mac/keychain_reauthorize.h"
#import "chrome/browser/mac/keystone_glue.h"
#include "chrome/browser/metrics/metrics_service.h"
#include "chrome/browser/system_monitor/removable_device_notifications_mac.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/common/main_function_params.h"
#include "content/public/common/result_codes.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_handle.h"

namespace {

// This preference is used to track whether the KeychainReauthorize operation
// has occurred at launch. This operation only makes sense while the
// application continues to be signed by the old certificate.
NSString* const kKeychainReauthorizeAtLaunchPref =
    @"KeychainReauthorizeInAppMay2012";
const int kKeychainReauthorizeAtLaunchMaxTries = 2;

// Some users rarely restart Chrome, so they might never get a chance to run
// the at-launch KeychainReauthorize. To account for them, there's also an
// at-update KeychainReauthorize option, which runs from .keystone_install for
// users on a user Keystone ticket. This operation may make sense for a period
// of time after the application switches to being signed by the new
// certificate, as long as the at-update stub executable is still signed by
// the old one.
NSString* const kKeychainReauthorizeAtUpdatePref =
    @"KeychainReauthorizeAtUpdateMay2012";
const int kKeychainReauthorizeAtUpdateMaxTries = 3;

// This is one enum instead of two so that the values can be correlated in a
// histogram.
enum CatSixtyFour {
  // Older than any expected cat.
  SABER_TOOTHED_CAT_32 = 0,
  SABER_TOOTHED_CAT_64,

  // Known cats.
  SNOW_LEOPARD_32,
  SNOW_LEOPARD_64,
  LION_32,  // Unexpected, Lion requires a 64-bit CPU.
  LION_64,
  MOUNTAIN_LION_32,  // Unexpected, Mountain Lion requires a 64-bit CPU.
  MOUNTAIN_LION_64,

  // DON'T add new constants here. It's important to keep the constant values,
  // um, constant. Add new constants at the bottom.

  // Newer than any known cat.
  FUTURE_CAT_32,  // Unexpected, it's unlikely Apple will un-obsolete old CPUs.
  FUTURE_CAT_64,

  // What if the bitsiness of the CPU can't be determined?
  SABER_TOOTHED_CAT_DUNNO,
  SNOW_LEOPARD_DUNNO,
  LION_DUNNO,
  MOUNTAIN_LION_DUNNO,
  FUTURE_CAT_DUNNO,

  // Add new constants here.

  CAT_SIXTY_FOUR_MAX
};

CatSixtyFour CatSixtyFourValue() {
#if defined(ARCH_CPU_64_BITS)
  // If 64-bit code is running, then it's established that this CPU can run
  // 64-bit code, and no further inquiry is necessary.
  int cpu64 = 1;
  bool cpu64_known = true;
#else
  // Check a sysctl conveniently provided by the kernel that identifies
  // whether the CPU supports 64-bit operation. Note that this tests the
  // actual hardware capabilities, not the bitsiness of the running process,
  // and not the bitsiness of the running kernel. The value thus determines
  // whether the CPU is capable of running 64-bit programs (in the presence of
  // proper OS runtime support) without regard to whether the current program
  // is 64-bit (it may not be) or whether the current kernel is (the kernel
  // can launch cross-bitted user-space tasks).

  int cpu64;
  size_t len = sizeof(cpu64);
  const char kSysctlName[] = "hw.cpu64bit_capable";
  bool cpu64_known = sysctlbyname(kSysctlName, &cpu64, &len, NULL, 0) == 0;
  if (!cpu64_known) {
    PLOG(WARNING) << "sysctlbyname(\"" << kSysctlName << "\")";
  }
#endif

  if (base::mac::IsOSSnowLeopard()) {
    return cpu64_known ? (cpu64 ? SNOW_LEOPARD_64 : SNOW_LEOPARD_32) :
                         SNOW_LEOPARD_DUNNO;
  }
  if (base::mac::IsOSLion()) {
    return cpu64_known ? (cpu64 ? LION_64 : LION_32) :
                         LION_DUNNO;
  }
  if (base::mac::IsOSMountainLion()) {
    return cpu64_known ? (cpu64 ? MOUNTAIN_LION_64 : MOUNTAIN_LION_32) :
                         MOUNTAIN_LION_DUNNO;
  }
  if (base::mac::IsOSLaterThanMountainLion_DontCallThis()) {
    return cpu64_known ? (cpu64 ? FUTURE_CAT_64 : FUTURE_CAT_32) :
                         FUTURE_CAT_DUNNO;
  }

  // If it's not any of the expected OS versions or later than them, it must
  // be prehistoric.
  return cpu64_known ? (cpu64 ? SABER_TOOTHED_CAT_64 : SABER_TOOTHED_CAT_32) :
                       SABER_TOOTHED_CAT_DUNNO;
}

void RecordCatSixtyFour() {
  CatSixtyFour cat_sixty_four = CatSixtyFourValue();

  // Set this higher than the highest value in the CatSixtyFour enum to
  // provide some headroom and then leave it alone. See HISTOGRAM_ENUMERATION
  // in base/metrics/histogram.h.
  const int kMaxCatsAndSixtyFours = 32;
  COMPILE_ASSERT(kMaxCatsAndSixtyFours >= CAT_SIXTY_FOUR_MAX,
                 CatSixtyFour_enum_grew_too_large);

  UMA_HISTOGRAM_ENUMERATION("OSX.CatSixtyFour",
                            cat_sixty_four,
                            kMaxCatsAndSixtyFours);
}

}  // namespace

void RecordBreakpadStatusUMA(MetricsService* metrics) {
  metrics->RecordBreakpadRegistration(IsCrashReporterEnabled());
  metrics->RecordBreakpadHasDebugger(base::debug::BeingDebugged());
}

void WarnAboutMinimumSystemRequirements() {
  // Nothing to check for on Mac right now.
}

// From browser_main_win.h, stubs until we figure out the right thing...

int DoUninstallTasks(bool chrome_still_running) {
  return content::RESULT_CODE_NORMAL_EXIT;
}

// ChromeBrowserMainPartsMac ---------------------------------------------------

ChromeBrowserMainPartsMac::ChromeBrowserMainPartsMac(
    const content::MainFunctionParams& parameters)
    : ChromeBrowserMainPartsPosix(parameters) {
}

ChromeBrowserMainPartsMac::~ChromeBrowserMainPartsMac() {
}

void ChromeBrowserMainPartsMac::PreEarlyInitialization() {
  if (parsed_command_line().HasSwitch(switches::kKeychainReauthorize)) {
    if (base::mac::AmIBundled()) {
      LOG(FATAL) << "Inappropriate process type for Keychain reauthorization";
    }

    // Do Keychain reauthorization at the time of update installation. This
    // gets three chances to run. If the first or second try doesn't complete
    // successfully (crashes or is interrupted for any reason), there will be
    // another chance. Once this step completes successfully, it should never
    // have to run again.
    //
    // This is kicked off by a special stub executable during an automatic
    // update. See chrome/installer/mac/keychain_reauthorize_main.cc.
    chrome::browser::mac::KeychainReauthorizeIfNeeded(
        kKeychainReauthorizeAtUpdatePref, kKeychainReauthorizeAtUpdateMaxTries);

    exit(0);
  }

  ChromeBrowserMainPartsPosix::PreEarlyInitialization();

  if (base::mac::WasLaunchedAsHiddenLoginItem()) {
    CommandLine* singleton_command_line = CommandLine::ForCurrentProcess();
    singleton_command_line->AppendSwitch(switches::kNoStartupWindow);
  }

  RecordCatSixtyFour();
}

void ChromeBrowserMainPartsMac::PreMainMessageLoopStart() {
  ChromeBrowserMainPartsPosix::PreMainMessageLoopStart();

  // Tell Cooca to finish its initialization, which we want to do manually
  // instead of calling NSApplicationMain(). The primary reason is that NSAM()
  // never returns, which would leave all the objects currently on the stack
  // in scoped_ptrs hanging and never cleaned up. We then load the main nib
  // directly. The main event loop is run from common code using the
  // MessageLoop API, which works out ok for us because it's a wrapper around
  // CFRunLoop.

  // Initialize NSApplication using the custom subclass.
  chrome_browser_application_mac::RegisterBrowserCrApp();

  // If ui_task is not NULL, the app is actually a browser_test, so startup is
  // handled outside of BrowserMain (which is what called this).
  if (!parameters().ui_task) {
    // The browser process only wants to support the language Cocoa will use,
    // so force the app locale to be overriden with that value.
    l10n_util::OverrideLocaleWithCocoaLocale();

    // Before we load the nib, we need to start up the resource bundle so we
    // have the strings avaiable for localization.
    // TODO(markusheintz): Read preference pref::kApplicationLocale in order
    // to enforce the application locale.
    const std::string loaded_locale =
        ResourceBundle::InitSharedInstanceWithLocale(std::string(), NULL);
    CHECK(!loaded_locale.empty()) << "Default locale could not be found";

    FilePath resources_pack_path;
    PathService::Get(chrome::FILE_RESOURCES_PACK, &resources_pack_path);
    ResourceBundle::GetSharedInstance().AddDataPackFromPath(
        resources_pack_path, ui::SCALE_FACTOR_NONE);
  }

  // This is a no-op if the KeystoneRegistration framework is not present.
  // The framework is only distributed with branded Google Chrome builds.
  [[KeystoneGlue defaultKeystoneGlue] registerWithKeystone];

  // Disk image installation is sort of a first-run task, so it shares the
  // kNoFirstRun switch.
  //
  // This needs to be done after the resource bundle is initialized (for
  // access to localizations in the UI) and after Keystone is initialized
  // (because the installation may need to promote Keystone) but before the
  // app controller is set up (and thus before MainMenu.nib is loaded, because
  // the app controller assumes that a browser has been set up and will crash
  // upon receipt of certain notifications if no browser exists), before
  // anyone tries doing anything silly like firing off an import job, and
  // before anything creating preferences like Local State in order for the
  // relaunched installed application to still consider itself as first-run.
  if (!parsed_command_line().HasSwitch(switches::kNoFirstRun)) {
    if (MaybeInstallFromDiskImage()) {
      // The application was installed and the installed copy has been
      // launched.  This process is now obsolete.  Exit.
      exit(0);
    }
  }

  // Now load the nib (from the right bundle).
  scoped_nsobject<NSNib>
      nib([[NSNib alloc] initWithNibNamed:@"MainMenu"
                                   bundle:base::mac::FrameworkBundle()]);
  // TODO(viettrungluu): crbug.com/20504 - This currently leaks, so if you
  // change this, you'll probably need to change the Valgrind suppression.
  [nib instantiateNibWithOwner:NSApp topLevelObjects:nil];
  // Make sure the app controller has been created.
  DCHECK([NSApp delegate]);

  // Prevent Cocoa from turning command-line arguments into
  // |-application:openFiles:|, since we already handle them directly.
  [[NSUserDefaults standardUserDefaults]
      setObject:@"NO" forKey:@"NSTreatUnknownArgumentsAsOpen"];
}

void ChromeBrowserMainPartsMac::PreProfileInit() {
  removable_device_notifications_mac_ =
      new chrome::RemovableDeviceNotificationsMac();
  ChromeBrowserMainPartsPosix::PreProfileInit();
}

void ChromeBrowserMainPartsMac::DidEndMainMessageLoop() {
  AppController* appController = [NSApp delegate];
  [appController didEndMainMessageLoop];
}
