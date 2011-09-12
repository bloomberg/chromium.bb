// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/mac/cfbundle_blocker.h"

#include <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>

#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/sys_string_conversions.h"
#include "third_party/mach_override/mach_override.h"

extern "C" {

// _CFBundleLoadExecutableAndReturnError is the internal implementation that
// results in a dylib being loaded via dlopen. Both CFBundleLoadExecutable and
// CFBundleLoadExecutableAndReturnError are funneled into this routine. Other
// CFBundle functions may also call directly into here, perhaps due to
// inlining their calls to CFBundleLoadExecutable.
//
// See CF-476.19/CFBundle.c (10.5.8), CF-550.43/CFBundle.c (10.6.8), and
// CF-635/Bundle.c (10.7.0) and the disassembly of the shipping object code.
//
// Because this is a private function not declared by
// <CoreFoundation/CoreFoundation.h>, provide a declaration here.
Boolean _CFBundleLoadExecutableAndReturnError(CFBundleRef bundle,
                                              Boolean force_global,
                                              CFErrorRef* error);

}  // extern "C"

namespace chrome {
namespace common {
namespace mac {

namespace {

// Returns an autoreleased array of paths that contain plug-ins that should be
// forbidden to load. Each element of the array will be a string containing
// an absolute pathname ending in '/'.
NSArray* BlockedPaths() {
  NSMutableArray* blocked_paths;

  {
    base::mac::ScopedNSAutoreleasePool autorelease_pool;

    // ~/Library, /Library, and /Network/Library. Things in /System/Library
    // aren't blacklisted.
    NSArray* blocked_prefixes =
       NSSearchPathForDirectoriesInDomains(NSLibraryDirectory,
                                           NSUserDomainMask |
                                               NSLocalDomainMask |
                                               NSNetworkDomainMask,
                                           YES);

    // Everything in the suffix list has a trailing slash so as to only block
    // loading things contained in these directories.
    NSString* const blocked_suffixes[] = {
      // SIMBL - http://code.google.com/p/simbl/source/browse/src/SIMBL.{h,m}.
      // It attempts to inject itself via an AppleScript event.
      // http://code.google.com/p/simbl/source/browse/SIMBL%20Agent/SIMBLAgent.m
      // Blocking input managers and scripting additions may already be enough
      // to prevent SIMBL plugins from loading.
      @"Application Support/SIMBL/Plugins/",

#if !defined(__LP64__)
      // Contextual menu manager plug-ins are unavailable to 64-bit processes.
      // http://developer.apple.com/library/mac/releasenotes/Cocoa/AppKitOlderNotes.html#NSMenu
      // Contextual menu plug-ins are loaded when a contextual menu is opened,
      // for example, from within
      // +[NSMenu popUpContextMenu:withEvent:forView:].
      @"Contextual Menu Items/",

      // Input managers are deprecated, would only be loaded under specific
      // circumstances, and are entirely unavailable to 64-bit processes.
      // http://developer.apple.com/library/mac/releasenotes/Cocoa/AppKitOlderNotes.html#NSInputManager
      // Input managers are loaded when the NSInputManager class is
      // initialized.
      @"InputManagers/",
#endif  // __LP64__

      // Don't load third-party scripting additions either. Scripting
      // additions are loaded by AppleScript from within AEProcessAppleEvent
      // in response to an Apple Event.
      @"ScriptingAdditions/"

      // This list is intentionally incomplete. For example, it doesn't block
      // printer drivers or Internet plug-ins.
    };

    NSUInteger blocked_paths_count = [blocked_prefixes count] *
                                     arraysize(blocked_suffixes);

    // Not autoreleased here, because the enclosing pool is scoped too
    // narrowly.
    blocked_paths =
        [[NSMutableArray alloc] initWithCapacity:blocked_paths_count];

    // Build a flat list by adding each suffix to each prefix.
    for (NSString* blocked_prefix in blocked_prefixes) {
      for (size_t blocked_suffix_index = 0;
           blocked_suffix_index < arraysize(blocked_suffixes);
           ++blocked_suffix_index) {
        NSString* blocked_suffix = blocked_suffixes[blocked_suffix_index];
        NSString* blocked_path =
            [blocked_prefix stringByAppendingPathComponent:blocked_suffix];

        [blocked_paths addObject:blocked_path];
      }
    }

    DCHECK_EQ([blocked_paths count], blocked_paths_count);
  }

  return [blocked_paths autorelease];
}

// Returns true if bundle_path identifies a path within a blocked directory.
// Blocked directories are those returned by BlockedPaths().
bool IsBundlePathBlocked(NSString* bundle_path) {
  static NSArray* blocked_paths = [BlockedPaths() retain];

  for (NSString* blocked_path in blocked_paths) {
    NSUInteger blocked_path_length = [blocked_path length];

    // Do a case-insensitive comparison because most users will be on
    // case-insensitive HFS+ filesystems and it's cheaper than asking the
    // disk. This is like [bundle_path hasPrefix:blocked_path] but is
    // case-insensitive.
    if ([bundle_path length] >= blocked_path_length &&
        [bundle_path compare:blocked_path
                     options:NSCaseInsensitiveSearch
                       range:NSMakeRange(0, blocked_path_length)] ==
        NSOrderedSame) {
      // If bundle_path is inside blocked_path (it has blocked_path as a
      // prefix), refuse to load it.
      return true;
    }
  }

  // bundle_path is not inside any blocked_path from blocked_paths.
  return false;
}

// Returns true if bundle_id identifies a bundle that is allowed to be loaded
// even when found in a blocked directory.
bool IsBundleIDAllowed(NSString* bundle_id) {
  return [bundle_id isEqualToString:@"com.google.osax.Google_Authenticator_BT"];
}

typedef Boolean (*_CFBundleLoadExecutableAndReturnError_Type)(CFBundleRef,
                                                              Boolean,
                                                              CFErrorRef*);

// Call this to execute the original implementation of
// _CFBundleLoadExecutableAndReturnError.
_CFBundleLoadExecutableAndReturnError_Type
    g_original_underscore_cfbundle_load_executable_and_return_error;

Boolean ChromeCFBundleLoadExecutableAndReturnError(CFBundleRef bundle,
                                                   Boolean force_global,
                                                   CFErrorRef* error) {
  base::mac::ScopedNSAutoreleasePool autorelease_pool;

  DCHECK(g_original_underscore_cfbundle_load_executable_and_return_error);

  base::mac::ScopedCFTypeRef<CFURLRef> url_cf(CFBundleCopyBundleURL(bundle));
  base::mac::ScopedCFTypeRef<CFStringRef> path_cf(
      CFURLCopyFileSystemPath(url_cf, kCFURLPOSIXPathStyle));
  NSString* path_ns = base::mac::CFToNSCast(path_cf);

  CFStringRef identifier_cf = CFBundleGetIdentifier(bundle);
  NSString* identifier_ns = base::mac::CFToNSCast(identifier_cf);

  if (IsBundlePathBlocked(path_ns) && !IsBundleIDAllowed(identifier_ns)) {
    NSString* identifier_ns_print = identifier_ns ? identifier_ns : @"(nil)";

    LOG(INFO) << "Blocking attempt to load bundle "
              << [identifier_ns_print UTF8String]
              << " at "
              << [path_ns fileSystemRepresentation];

    if (error) {
      base::mac::ScopedCFTypeRef<CFStringRef> bundle_id(
          base::SysUTF8ToCFStringRef(base::mac::BaseBundleID()));

      // 0xb10c10ad = "block load"
      const CFIndex kBundleLoadBlocked = 0xb10c10ad;

      NSMutableDictionary* error_dict =
          [NSMutableDictionary dictionaryWithCapacity:3];
      if (identifier_ns) {
        [error_dict setObject:identifier_ns forKey:@"identifier"];
      }
      if (path_ns) {
        [error_dict setObject:path_ns forKey:@"path"];
      }
      NSURL* url_ns = base::mac::CFToNSCast(url_cf);
      NSString* url_absolute_string_ns = [url_ns absoluteString];
      if (url_absolute_string_ns) {
        [error_dict setObject:url_absolute_string_ns forKey:@"url"];
      }

      *error = CFErrorCreate(NULL,
                             bundle_id,
                             kBundleLoadBlocked,
                             base::mac::NSToCFCast(error_dict));
    }

    return FALSE;
  }

  // Not blocked. Call through to the original implementation.
  return g_original_underscore_cfbundle_load_executable_and_return_error(
      bundle, force_global, error);
}

}  // namespace

void EnableCFBundleBlocker() {
  mach_error_t err = mach_override_ptr(
      reinterpret_cast<void*>(_CFBundleLoadExecutableAndReturnError),
      reinterpret_cast<void*>(ChromeCFBundleLoadExecutableAndReturnError),
      reinterpret_cast<void**>(
          &g_original_underscore_cfbundle_load_executable_and_return_error));
  if (err != err_none) {
    LOG(WARNING) << "mach_override _CFBundleLoadExecutableAndReturnError: "
                 << err;
  }
}

}  // namespace mac
}  // namespace common
}  // namespace chrome
