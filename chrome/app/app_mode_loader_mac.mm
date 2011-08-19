// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// On Mac, shortcuts can't have command-line arguments. Instead, produce small
// app bundles which locate the Chromium framework and load it, passing the
// appropriate data. This is the code for such an app bundle. It should be kept
// minimal and do as little work as possible (with as much work done on
// framework side as possible).

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>

#include "chrome/common/mac/app_mode_common.h"

namespace {

// Checks that condition |x| is true. If not, outputs |msg| (together with
// source filename and line number) and exits.
#define CHECK_MSG(x, msg) if (!(x)) check_msg_helper(__FILE__, __LINE__, msg);
void check_msg_helper(const char* file, int line, const char* msg) {
  fprintf(stderr, "%s (%d): %s\n", file, line, msg);
  exit(1);
}

// Converts an NSString to a UTF8 C string (which is allocated, and may be freed
// using |free()|). If |s| is nil or can't produce such a string, this returns
// |NULL|.
char* NSStringToUTF8CString(NSString* s) {
  CHECK_MSG([s isKindOfClass:[NSString class]], "expected an NSString");
  const char* cstring = [s UTF8String];
  return cstring ? strdup(cstring) : NULL;
}

// Converts an NSString to a file-system representation C string (which is
// allocated, and may be freed using |free()|). If |s| is nil or can't produce
// such a string, this returns |NULL|.
char* NSStringToFSCString(NSString* s) {
  CHECK_MSG([s isKindOfClass:[NSString class]], "expected an NSString");
  const char* cstring = [s fileSystemRepresentation];
  return cstring ? strdup(cstring) : NULL;
}

}  // namespace

__attribute__((visibility("default")))
int main(int argc, char** argv) {
  app_mode::ChromeAppModeInfo info;
  info.major_version = 0;  // v0.1
  info.minor_version = 1;
  info.argc = argc;
  info.argv = argv;

  // The Cocoa APIs are a bit more convenient; for this an autorelease pool is
  // needed.
  NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];

  // Get the current main bundle, i.e., that of the app loader that's running.
  NSBundle* app_bundle = [NSBundle mainBundle];
  CHECK_MSG(app_bundle, "couldn't get loader bundle");

  // Get the bundle ID of the browser that created this app bundle.
  NSString* cr_bundle_id = [app_bundle
      objectForInfoDictionaryKey:(NSString*)app_mode::kBrowserBundleIDKey];
  CHECK_MSG(cr_bundle_id, "couldn't get browser bundle ID");

  // Get the browser bundle path.
  // TODO(viettrungluu): more fun
  NSString* cr_bundle_path =
      [(NSString*)CFPreferencesCopyAppValue(
          app_mode::kLastRunAppBundlePathPrefsKey,
          (CFStringRef)cr_bundle_id) autorelease];
  CHECK_MSG(cr_bundle_path, "couldn't get browser bundle path");

  // Get the browser bundle.
  NSBundle* cr_bundle = [NSBundle bundleWithPath:cr_bundle_path];
  CHECK_MSG(cr_bundle, "couldn't get browser bundle");

  // Get the current browser version.
  NSString* cr_version =
      [cr_bundle objectForInfoDictionaryKey:@"CFBundleShortVersionString"];
  CHECK_MSG(cr_version, "couldn't get browser version");

  // Get the current browser versioned directory.
  NSArray* cr_versioned_path_components =
      [NSArray arrayWithObjects:cr_bundle_path,
                                @"Contents",
                                @"Versions",
                                cr_version,
                                nil];
  NSString* cr_versioned_path =
      [[NSString pathWithComponents:cr_versioned_path_components]
          stringByStandardizingPath];
  CHECK_MSG(cr_versioned_path, "couldn't get browser versioned path");
  // And copy it, since |cr_versioned_path| will go away with the pool.
  info.chrome_versioned_path = NSStringToFSCString(cr_versioned_path);

  // Optional, so okay if it's NULL.
  info.app_mode_bundle_path = NSStringToFSCString([app_bundle bundlePath]);

  // Read information about the this app shortcut from the Info.plist.
  // Don't check for null-ness on optional items.
  NSDictionary* info_plist = [app_bundle infoDictionary];
  CHECK_MSG(info_plist, "couldn't get loader Info.plist");

  info.app_mode_id = NSStringToUTF8CString(
      [info_plist objectForKey:@"CrAppModeShortcutID"]);
  CHECK_MSG(info.app_mode_id, "couldn't get app shortcut ID");

  info.app_mode_short_name = NSStringToUTF8CString(
      [info_plist objectForKey:@"CrAppModeShortcutShortName"]);

  info.app_mode_name = NSStringToUTF8CString(
      [info_plist objectForKey:@"CrAppModeShortcutName"]);

  info.app_mode_url = NSStringToUTF8CString(
      [info_plist objectForKey:@"CrAppModeShortcutURL"]);
  CHECK_MSG(info.app_mode_url, "couldn't get app shortcut URL");

  // Get the framework path.
  NSString* cr_bundle_exe =
      [cr_bundle objectForInfoDictionaryKey:@"CFBundleExecutable"];
  NSString* cr_framework_path =
      [cr_versioned_path stringByAppendingPathComponent:
          [cr_bundle_exe stringByAppendingString:@" Framework.framework"]];
  cr_framework_path =
      [cr_framework_path stringByAppendingPathComponent:
          [cr_bundle_exe stringByAppendingString:@" Framework"]];

  // Open the framework.
  void* cr_dylib = dlopen([cr_framework_path fileSystemRepresentation],
                          RTLD_LAZY);
  CHECK_MSG(cr_dylib, "couldn't load framework");

  // Drain the pool as late as possible.
  [pool drain];

  typedef int (*StartFun)(const app_mode::ChromeAppModeInfo*);
  StartFun ChromeAppModeStart = (StartFun)dlsym(cr_dylib, "ChromeAppModeStart");
  CHECK_MSG(ChromeAppModeStart, "couldn't get entry point");

  // Exit instead of returning to avoid the the removal of |main()| from stack
  // backtraces under tail call optimization.
  int rv = ChromeAppModeStart(&info);
  exit(rv);
}
