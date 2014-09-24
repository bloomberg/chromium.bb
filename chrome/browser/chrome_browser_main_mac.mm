// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main_mac.h"

#import <Cocoa/Cocoa.h>
#include <sys/sysctl.h>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#import "chrome/browser/app_controller_mac.h"
#include "chrome/browser/apps/app_shim/app_shim_host_manager_mac.h"
#include "chrome/browser/browser_process.h"
#import "chrome/browser/chrome_browser_application_mac.h"
#include "chrome/browser/mac/install_from_dmg.h"
#import "chrome/browser/mac/keystone_glue.h"
#include "chrome/browser/mac/mac_startup_profiler.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "components/crash/app/breakpad_mac.h"
#include "components/metrics/metrics_service.h"
#include "content/public/common/main_function_params.h"
#include "content/public/common/result_codes.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_handle.h"

namespace {

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
  MAVERICKS_32,  // Unexpected, Mavericks requires a 64-bit CPU.
  MAVERICKS_64,

  // DON'T add new constants here. It's important to keep the constant values,
  // um, constant. Add new constants at the bottom.

  // What if the bitsiness of the CPU can't be determined?
  SABER_TOOTHED_CAT_DUNNO,
  SNOW_LEOPARD_DUNNO,
  LION_DUNNO,
  MOUNTAIN_LION_DUNNO,
  MAVERICKS_DUNNO,

  // More known cats.
  YOSEMITE_32,  // Unexpected, Yosemite requires a 64-bit CPU.
  YOSEMITE_64,
  YOSEMITE_DUNNO,

  // Newer than any known cat.
  FUTURE_CAT_32,  // Unexpected, it's unlikely Apple will un-obsolete old CPUs.
  FUTURE_CAT_64,
  FUTURE_CAT_DUNNO,

  // As new versions of Mac OS X are released with sillier and sillier names,
  // rename the FUTURE_CAT enum values to match those names, and re-create
  // FUTURE_CAT_[32|64|DUNNO] here.

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
  if (base::mac::IsOSMavericks()) {
    return cpu64_known ? (cpu64 ? MAVERICKS_64 : MAVERICKS_32) :
                         MAVERICKS_DUNNO;
  }
  if (base::mac::IsOSYosemite()) {
    return cpu64_known ? (cpu64 ? YOSEMITE_64 : YOSEMITE_32) :
                         YOSEMITE_DUNNO;
  }
  if (base::mac::IsOSLaterThanYosemite_DontCallThis()) {
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

  // Set this higher than the highest value in the CatSixtyFour enum to provide
  // some headroom and then leave it alone. See UMA_HISTOGRAM_ENUMERATION in
  // base/metrics/histogram.h.
  const int kMaxCatsAndSixtyFours = 32;
  COMPILE_ASSERT(kMaxCatsAndSixtyFours >= CAT_SIXTY_FOUR_MAX,
                 CatSixtyFour_enum_grew_too_large);

  UMA_HISTOGRAM_ENUMERATION("OSX.CatSixtyFour",
                            cat_sixty_four,
                            kMaxCatsAndSixtyFours);
}

}  // namespace

// ChromeBrowserMainPartsMac ---------------------------------------------------

ChromeBrowserMainPartsMac::ChromeBrowserMainPartsMac(
    const content::MainFunctionParams& parameters)
    : ChromeBrowserMainPartsPosix(parameters) {
}

ChromeBrowserMainPartsMac::~ChromeBrowserMainPartsMac() {
}

void ChromeBrowserMainPartsMac::PreEarlyInitialization() {
  ChromeBrowserMainPartsPosix::PreEarlyInitialization();

  if (base::mac::WasLaunchedAsLoginItemRestoreState()) {
    CommandLine* singleton_command_line = CommandLine::ForCurrentProcess();
    singleton_command_line->AppendSwitch(switches::kRestoreLastSession);
  } else if (base::mac::WasLaunchedAsHiddenLoginItem()) {
    CommandLine* singleton_command_line = CommandLine::ForCurrentProcess();
    singleton_command_line->AppendSwitch(switches::kNoStartupWindow);
  }

  RecordCatSixtyFour();
}

void ChromeBrowserMainPartsMac::PreMainMessageLoopStart() {
  MacStartupProfiler::GetInstance()->Profile(
      MacStartupProfiler::PRE_MAIN_MESSAGE_LOOP_START);
  ChromeBrowserMainPartsPosix::PreMainMessageLoopStart();

  // Tell Cocoa to finish its initialization, which we want to do manually
  // instead of calling NSApplicationMain(). The primary reason is that NSAM()
  // never returns, which would leave all the objects currently on the stack
  // in scoped_ptrs hanging and never cleaned up. We then load the main nib
  // directly. The main event loop is run from common code using the
  // MessageLoop API, which works out ok for us because it's a wrapper around
  // CFRunLoop.

  // Initialize NSApplication using the custom subclass.
  chrome_browser_application_mac::RegisterBrowserCrApp();

  // If ui_task is not NULL, the app is actually a browser_test.
  if (!parameters().ui_task) {
    // The browser process only wants to support the language Cocoa will use,
    // so force the app locale to be overriden with that value.
    l10n_util::OverrideLocaleWithCocoaLocale();
  }

  // Before we load the nib, we need to start up the resource bundle so we
  // have the strings avaiable for localization.
  // TODO(markusheintz): Read preference pref::kApplicationLocale in order
  // to enforce the application locale.
  const std::string loaded_locale =
      ui::ResourceBundle::InitSharedInstanceWithLocale(
          std::string(), NULL, ui::ResourceBundle::LOAD_COMMON_RESOURCES);
  CHECK(!loaded_locale.empty()) << "Default locale could not be found";

  base::FilePath resources_pack_path;
  PathService::Get(chrome::FILE_RESOURCES_PACK, &resources_pack_path);
  ResourceBundle::GetSharedInstance().AddDataPackFromPath(
      resources_pack_path, ui::SCALE_FACTOR_NONE);

  // This is a no-op if the KeystoneRegistration framework is not present.
  // The framework is only distributed with branded Google Chrome builds.
  [[KeystoneGlue defaultKeystoneGlue] registerWithKeystone];

  // Disk image installation is sort of a first-run task, so it shares the
  // no first run switches.
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
  if (!first_run::IsFirstRunSuppressed(parsed_command_line())) {
    if (MaybeInstallFromDiskImage()) {
      // The application was installed and the installed copy has been
      // launched.  This process is now obsolete.  Exit.
      exit(0);
    }
  }

  // Now load the nib (from the right bundle).
  base::scoped_nsobject<NSNib> nib(
      [[NSNib alloc] initWithNibNamed:@"MainMenu"
                               bundle:base::mac::FrameworkBundle()]);
  // TODO(viettrungluu): crbug.com/20504 - This currently leaks, so if you
  // change this, you'll probably need to change the Valgrind suppression.
  [nib instantiateNibWithOwner:NSApp topLevelObjects:nil];
  // Make sure the app controller has been created.
  DCHECK([NSApp delegate]);

  [[NSUserDefaults standardUserDefaults] registerDefaults:@{
      // Prevent Cocoa from turning command-line arguments into
      // |-application:openFiles:|, since we already handle them directly.
      // @"NO" looks like a mistake, but the value really is supposed to be a
      // string.
      @"NSTreatUnknownArgumentsAsOpen": @"NO",
      // CoreAnimation has poor performance and CoreAnimation and
      // non-CoreAnimation exhibit window flickering when layers are not hosted
      // in the window server, which is the default when not not using the
      // 10.9 SDK.
      // TODO: Remove this when we build with the 10.9 SDK.
      @"NSWindowHostsLayersInWindowServer": @(base::mac::IsOSMavericksOrLater())
  }];
}

void ChromeBrowserMainPartsMac::PostMainMessageLoopStart() {
  MacStartupProfiler::GetInstance()->Profile(
      MacStartupProfiler::POST_MAIN_MESSAGE_LOOP_START);
  ChromeBrowserMainPartsPosix::PostMainMessageLoopStart();
}

void ChromeBrowserMainPartsMac::PreProfileInit() {
  MacStartupProfiler::GetInstance()->Profile(
      MacStartupProfiler::PRE_PROFILE_INIT);
  ChromeBrowserMainPartsPosix::PreProfileInit();
  // This is called here so that the app shim socket is only created after
  // taking the singleton lock.
  g_browser_process->platform_part()->app_shim_host_manager()->Init();
  AppListService::InitAll(NULL);
}

void ChromeBrowserMainPartsMac::PostProfileInit() {
  MacStartupProfiler::GetInstance()->Profile(
      MacStartupProfiler::POST_PROFILE_INIT);
  ChromeBrowserMainPartsPosix::PostProfileInit();
  g_browser_process->metrics_service()->RecordBreakpadRegistration(
      breakpad::IsCrashReporterEnabled());
}

void ChromeBrowserMainPartsMac::DidEndMainMessageLoop() {
  AppController* appController = [NSApp delegate];
  [appController didEndMainMessageLoop];
}
