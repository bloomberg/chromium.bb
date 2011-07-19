// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/mac/dock.h"

#include <ApplicationServices/ApplicationServices.h>
#import <Foundation/Foundation.h>
#include <CoreFoundation/CoreFoundation.h>
#include <signal.h>

#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/mac/launchd.h"

namespace dock {
namespace {

NSString* const kDockTileDataKey = @"tile-data";
NSString* const kDockFileDataKey = @"file-data";
NSString* const kDockCFURLStringKey = @"_CFURLString";
NSString* const kDockCFURLStringTypeKey = @"_CFURLStringType";

// Returns an array parallel to |persistent_apps| containing only the
// pathnames of the Dock tiles contained therein. Returns nil on failure, such
// as when the structure of |persistent_apps| is not understood.
NSMutableArray* PersistentAppPaths(NSArray* persistent_apps) {
  NSMutableArray* app_paths =
      [NSMutableArray arrayWithCapacity:[persistent_apps count]];

  for (NSDictionary* app in persistent_apps) {
    if (![app isKindOfClass:[NSDictionary class]]) {
      LOG(ERROR) << "app not NSDictionary";
      return nil;
    }

    NSDictionary* tile_data = [app objectForKey:kDockTileDataKey];
    if (![tile_data isKindOfClass:[NSDictionary class]]) {
      LOG(ERROR) << "tile_data not NSDictionary";
      return nil;
    }

    NSDictionary* file_data = [tile_data objectForKey:kDockFileDataKey];
    if (![file_data isKindOfClass:[NSDictionary class]]) {
      LOG(ERROR) << "file_data not NSDictionary";
      return nil;
    }

    NSNumber* type = [file_data objectForKey:kDockCFURLStringTypeKey];
    if (![type isKindOfClass:[NSNumber class]]) {
      LOG(ERROR) << "type not NSNumber";
      return nil;
    }
    if ([type intValue] != 0) {
      LOG(ERROR) << "type not 0";
      return nil;
    }

    NSString* path = [file_data objectForKey:kDockCFURLStringKey];
    if (![path isKindOfClass:[NSString class]]) {
      LOG(ERROR) << "path not NSString";
      return nil;
    }

    [app_paths addObject:path];
  }

  return app_paths;
}

// Returns the process ID for a process whose bundle identifier is bundle_id.
// The process is looked up using the Process Manager. Returns -1 on error,
// including when no process matches the bundle identifier.
pid_t PIDForProcessBundleID(const std::string& bundle_id) {
  // This technique is racy: what happens if |psn| becomes invalid before a
  // subsequent call to GetNextProcess, or if the Process Manager's internal
  // order of processes changes? Tolerate the race by allowing failure. Since
  // this function is only used on Leopard to find the Dock process so that it
  // can be restarted, the worst that can happen here is that the Dock won't
  // be restarted.
  ProcessSerialNumber psn = {0, kNoProcess};
  OSStatus status;
  while ((status = GetNextProcess(&psn)) == noErr) {
    pid_t process_pid;
    if ((status = GetProcessPID(&psn, &process_pid)) != noErr) {
      LOG(ERROR) << "GetProcessPID: " << status;
      continue;
    }

    base::mac::ScopedCFTypeRef<CFDictionaryRef> process_dictionary(
        ProcessInformationCopyDictionary(&psn,
            kProcessDictionaryIncludeAllInformationMask));
    if (!process_dictionary.get()) {
      LOG(ERROR) << "ProcessInformationCopyDictionary";
      continue;
    }

    CFStringRef process_bundle_id_cf = static_cast<CFStringRef>(
        CFDictionaryGetValue(process_dictionary, kCFBundleIdentifierKey));
    if (!process_bundle_id_cf) {
      // Not all processes have a bundle ID.
      continue;
    } else if (CFGetTypeID(process_bundle_id_cf) != CFStringGetTypeID()) {
      LOG(ERROR) << "process_bundle_id_cf not CFStringRef";
      continue;
    }

    std::string process_bundle_id =
        base::SysCFStringRefToUTF8(process_bundle_id_cf);
    if (process_bundle_id == bundle_id) {
      // Found it!
      return process_pid;
    }
  }

  // status will be procNotFound (-600) if the process wasn't found.
  LOG(ERROR) << "GetNextProcess: " << status;

  return -1;
}

// Restart the Dock process by sending it a SIGHUP.
void Restart() {
  pid_t pid;

  if (base::mac::IsOSSnowLeopardOrLater()) {
    // Doing this via launchd using the proper job label is the safest way to
    // handle the restart. Unlike "killall Dock", looking this up via launchd
    // guarantees that only the right process will be targeted.
    pid = launchd::PIDForJob("com.apple.Dock.agent");
  } else {
    // On Leopard, the Dock doesn't have a known fixed job label name as it
    // does on Snow Leopard and Lion because it's not launched as a launch
    // agent. Look the PID up by finding a process with the expected bundle
    // identifier using the Process Manager.
    pid = PIDForProcessBundleID("com.apple.dock");
  }

  if (pid <= 0) {
    return;
  }

  // Sending a SIGHUP to the Dock seems to be a more reliable way to get the
  // replacement Dock process to read the newly written plist than using the
  // equivalent of "launchctl stop" (even if followed by "launchctl start.")
  // Note that this is a potential race in that pid may no longer be valid or
  // may even have been reused.
  kill(pid, SIGHUP);
}

}  // namespace

void AddIcon(NSString* installed_path, NSString* dmg_app_path) {
  // ApplicationServices.framework/Frameworks/HIServices.framework contains an
  // undocumented function, CoreDockAddFileToDock, that is able to add items
  // to the Dock "live" without requiring a Dock restart. Under the hood, it
  // communicates with the Dock via Mach IPC. It is available as of Mac OS X
  // 10.6. AddIcon could call CoreDockAddFileToDock if available, but
  // CoreDockAddFileToDock seems to always to add the new Dock icon last,
  // where AddIcon takes care to position the icon appropriately. Based on
  // disassembly, the signature of the undocumented function appears to be
  //    extern "C" OSStatus CoreDockAddFileToDock(CFURLRef url, int);
  // The int argument doesn't appear to have any effect. It's not used as the
  // position to place the icon as hoped.

  // There's enough potential allocation in this function to justify a
  // distinct pool.
  base::mac::ScopedNSAutoreleasePool autorelease_pool;

  NSString* const kDockDomain = @"com.apple.dock";
  NSUserDefaults* user_defaults = [NSUserDefaults standardUserDefaults];

  NSDictionary* dock_plist_const =
      [user_defaults persistentDomainForName:kDockDomain];
  if (![dock_plist_const isKindOfClass:[NSDictionary class]]) {
    LOG(ERROR) << "dock_plist_const not NSDictionary";
    return;
  }
  NSMutableDictionary* dock_plist =
      [NSMutableDictionary dictionaryWithDictionary:dock_plist_const];

  NSString* const kDockPersistentAppsKey = @"persistent-apps";
  NSArray* persistent_apps_const =
      [dock_plist objectForKey:kDockPersistentAppsKey];
  if (![persistent_apps_const isKindOfClass:[NSArray class]]) {
    LOG(ERROR) << "persistent_apps_const not NSArray";
    return;
  }
  NSMutableArray* persistent_apps =
      [NSMutableArray arrayWithArray:persistent_apps_const];

  NSMutableArray* persistent_app_paths = PersistentAppPaths(persistent_apps);
  if (!persistent_app_paths) {
    return;
  }

  // Directories in the Dock's plist are given with trailing slashes. Since
  // installed_path and dmg_app_path both refer to application bundles,
  // they're directories and will show up with trailing slashes. This is an
  // artifact of the Dock's internal use of CFURL. Look for paths that match,
  // and when adding an item to the Dock's plist, keep it in the form that the
  // Dock likes.
  NSString* installed_path_dock = [installed_path stringByAppendingString:@"/"];
  NSString* dmg_app_path_dock = [dmg_app_path stringByAppendingString:@"/"];

  NSUInteger already_installed_app_index = NSNotFound;
  NSUInteger app_index = NSNotFound;
  for (NSUInteger index = 0; index < [persistent_apps count]; ++index) {
    NSString* app_path = [persistent_app_paths objectAtIndex:index];
    if ([app_path isEqualToString:installed_path_dock]) {
      // If the Dock already contains a reference to the newly installed
      // application, don't add another one.
      already_installed_app_index = index;
    } else if ([app_path isEqualToString:dmg_app_path_dock]) {
      // If the Dock contains a reference to the application on the disk
      // image, replace it with a reference to the newly installed
      // application. However, if the Dock contains a reference to both the
      // application on the disk image and the newly installed application,
      // just remove the one referencing the disk image.
      //
      // This case is only encountered when the user drags the icon from the
      // disk image volume window in the Finder directly into the Dock.
      app_index = index;
    }
  }

  bool made_change = false;

  if (app_index != NSNotFound) {
    // Remove the Dock's reference to the application on the disk image.
    [persistent_apps removeObjectAtIndex:app_index];
    [persistent_app_paths removeObjectAtIndex:app_index];
    made_change = true;
  }

  if (already_installed_app_index == NSNotFound) {
    // The Dock doesn't yet have a reference to the icon at the
    // newly installed path. Figure out where to put the new icon.
    NSString* app_name = [installed_path lastPathComponent];

    if (app_index == NSNotFound) {
      // If an application with this name is already in the Dock, put the new
      // one right before it.
      for (NSUInteger index = 0; index < [persistent_apps count]; ++index) {
        NSString* dock_app_name =
            [[persistent_app_paths objectAtIndex:index] lastPathComponent];
        if ([dock_app_name isEqualToString:app_name]) {
          app_index = index;
          break;
        }
      }
    }

#if defined(GOOGLE_CHROME_BUILD)
    if (app_index == NSNotFound) {
      // If this is an officially-branded Chrome (including Canary) and an
      // application matching the "other" flavor is already in the Dock, put
      // them next to each other. Google Chrome will precede Google Chrome
      // Canary in the Dock.
      NSString* chrome_name = @"Google Chrome.app";
      NSString* canary_name = @"Google Chrome Canary.app";
      for (NSUInteger index = 0; index < [persistent_apps count]; ++index) {
        NSString* dock_app_name =
            [[persistent_app_paths objectAtIndex:index] lastPathComponent];
        if ([dock_app_name isEqualToString:canary_name] &&
            [app_name isEqualToString:chrome_name]) {
          app_index = index;

          // Break: put Google Chrome.app before the first Google Chrome
          // Canary.app.
          break;
        } else if ([dock_app_name isEqualToString:chrome_name] &&
                   [app_name isEqualToString:canary_name]) {
          app_index = index + 1;

          // No break: put Google Chrome Canary.app after the last Google
          // Chrome.app.
        }
      }
    }
#endif  // GOOGLE_CHROME_BUILD

    if (app_index == NSNotFound) {
      // Put the new application after the last browser application already
      // present in the Dock.
      NSArray* other_browser_app_names =
          [NSArray arrayWithObjects:
#if defined(GOOGLE_CHROME_BUILD)
                                    @"Chromium.app",  // Unbranded Google Chrome
#else
                                    @"Google Chrome.app",
                                    @"Google Chrome Canary.app",
#endif
                                    @"Safari.app",
                                    @"Firefox.app",
                                    @"Camino.app",
                                    @"Opera.app",
                                    @"OmniWeb.app",
                                    @"WebKit.app",    // Safari nightly
                                    @"Aurora.app",    // Firefox dev
                                    @"Nightly.app",   // Firefox nightly
                                    nil];
      for (NSUInteger index = 0; index < [persistent_apps count]; ++index) {
        NSString* dock_app_name =
            [[persistent_app_paths objectAtIndex:index] lastPathComponent];
        if ([other_browser_app_names containsObject:dock_app_name]) {
          app_index = index + 1;
        }
      }
    }

    if (app_index == NSNotFound) {
      // Put the new application last in the Dock.
      app_index = [persistent_apps count];
    }

    // Set up the new Dock tile.
    NSDictionary* new_tile_file_data =
        [NSDictionary dictionaryWithObjectsAndKeys:
            installed_path_dock, kDockCFURLStringKey,
            [NSNumber numberWithInt:0], kDockCFURLStringTypeKey,
            nil];
    NSDictionary* new_tile_data =
        [NSDictionary dictionaryWithObject:new_tile_file_data
                                    forKey:kDockFileDataKey];
    NSDictionary* new_tile =
        [NSDictionary dictionaryWithObject:new_tile_data
                                    forKey:kDockTileDataKey];

    // Add the new tile to the Dock.
    [persistent_apps insertObject:new_tile atIndex:app_index];
    [persistent_app_paths insertObject:installed_path_dock atIndex:app_index];
    made_change = true;
  }

  // Verify that the arrays are still parallel.
  DCHECK_EQ([persistent_apps count], [persistent_app_paths count]);

  if (!made_change) {
    // If no changes were made, there's no point in rewriting the Dock's
    // plist or restarting the Dock.
    return;
  }

  // Rewrite the plist.
  [dock_plist setObject:persistent_apps forKey:kDockPersistentAppsKey];
  [user_defaults setPersistentDomain:dock_plist forName:kDockDomain];

  Restart();
}

}  // namespace dock
