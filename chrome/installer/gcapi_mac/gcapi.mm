// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/gcapi_mac/gcapi.h"

#import <Cocoa/Cocoa.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>

namespace {

NSString* const kChromeInstallPath = @"/Applications/Google Chrome.app";

NSString* const kBrandKey = @"KSBrandID";
NSString* const kSystemBrandPath = @"/Library/Google/Google Chrome Brand.plist";
NSString* const kUserBrandPath = @"~/Library/Google/Google Chrome Brand.plist";

NSString* const kSystemKsadminPath =
    @"/Library/Google/GoogleSoftwareUpdate/GoogleSoftwareUpdate.bundle/"
     "Contents/MacOS/ksadmin";

NSString* const kUserKsadminPath =
    @"~/Library/Google/GoogleSoftwareUpdate/GoogleSoftwareUpdate.bundle/"
     "Contents/MacOS/ksadmin";

NSString* const kSystemMasterPrefsPath =
    @"/Library/Google/Google Chrome Master Preferences";
NSString* const kUserMasterPrefsPath =
    @"~/Library/Application Support/Google/Google Chrome Master Preferences";

NSString* const kChannelKey = @"KSChannelID";
NSString* const kVersionKey = @"KSVersion";

// Condensed from chromium's base/mac/mac_util.mm.
bool IsOSXLeopardOrLater() {
  // On 10.6, Gestalt() was observed to be able to spawn threads (see
  // http://crbug.com/53200). Don't call Gestalt().
  struct utsname uname_info;
  if (uname(&uname_info) != 0)
    return false;
  if (strcmp(uname_info.sysname, "Darwin") != 0)
    return false;

  char* dot = strchr(uname_info.release, '.');
  if (!dot)
    return false;

  int darwin_major_version = atoi(uname_info.release);
  if (darwin_major_version < 6)
    return false;

  // The Darwin major version is always 4 greater than the Mac OS X minor
  // version for Darwin versions beginning with 6, corresponding to Mac OS X
  // 10.2.
  int mac_os_x_minor_version = darwin_major_version - 4;

  return mac_os_x_minor_version >= 5;
}

enum TicketKind {
  kSystemTicket, kUserTicket
};

BOOL HasChromeTicket(TicketKind kind) {
  // Don't use Objective-C 2 loop syntax, in case an installer runs on 10.4.
  NSMutableArray* keystonePaths =
      [NSMutableArray arrayWithObject:kSystemKsadminPath];
  if (kind == kUserTicket && geteuid() != 0)
    [keystonePaths insertObject:kUserKsadminPath atIndex:0];
  NSEnumerator* e = [keystonePaths objectEnumerator];
  id ksPath;
  while ((ksPath = [e nextObject])) {
    NSTask* task = nil;
    NSString* string = nil;
    bool ksadminRanSuccessfully = false;

    @try {
      task = [[NSTask alloc] init];
      [task setLaunchPath:ksPath];

      NSArray* arguments = @[
          kind == kUserTicket ? @"--user-store" : @"--system-store",
          @"--print-tickets",
          @"--productid",
          @"com.google.Chrome",
      ];
      [task setArguments:arguments];

      NSPipe* pipe = [NSPipe pipe];
      [task setStandardOutput:pipe];

      NSFileHandle* file = [pipe fileHandleForReading];

      [task launch];

      NSData* data = [file readDataToEndOfFile];
      [task waitUntilExit];

      ksadminRanSuccessfully = [task terminationStatus] == 0;
      string = [[[NSString alloc] initWithData:data
                                    encoding:NSUTF8StringEncoding] autorelease];
    }
    @catch (id exception) {
      // Most likely, ksPath didn't exist.
    }
    [task release];

    if (ksadminRanSuccessfully && [string length] > 0)
      return YES;
  }

  return NO;
}

// Returns the file permission mask for files created by gcapi.
mode_t Permissions() {
  return 0755;
}

BOOL CreatePathToFile(NSString* path) {
  path = [path stringByDeletingLastPathComponent];

  // Default owner, group, permissions:
  // * Permissions are set according to the umask of the current process. For
  //   more information, see umask.
  // * The owner ID is set to the effective user ID of the process.
  // * The group ID is set to that of the parent directory.
  // The default group ID is fine. Owner ID is fine too, since user directory
  // paths won't be created if euid is 0. Do set permissions explicitly; for
  // admin paths all admins can write, for user paths just the owner may.
  NSMutableDictionary* attributes = [NSMutableDictionary
      dictionaryWithObject:[NSNumber numberWithShort:Permissions()]
                    forKey:NSFilePosixPermissions];
  if (geteuid() == 0)
    [attributes setObject:@"wheel" forKey:NSFileGroupOwnerAccountName];

  NSFileManager* manager = [NSFileManager defaultManager];
  return [manager createDirectoryAtPath:path
            withIntermediateDirectories:YES
                             attributes:attributes
                                  error:nil];
}

// Tries to write |data| at |system_path| or if that fails and geteuid() is not
// 0 at |user_path|. Returns the path where it wrote, or nil on failure.
NSString* WriteData(NSData* data, NSString* system_path, NSString* user_path) {
  // Try system first.
  if (CreatePathToFile(system_path) &&
      [data writeToFile:system_path atomically:YES]) {
    // files are created with group of parent dir (good), owner of euid (good).
    chmod([system_path fileSystemRepresentation], Permissions() & ~0111);
    return system_path;
  }

  // Failed, try user.
  // -stringByExpandingTildeInPath returns root's home directory if this is run
  // setuid root, and in that case the kSystemBrandPath path above should have
  // worked anyway. So only try user if geteuid() isn't root.
  if (geteuid() != 0) {
    NSString* user_path = [user_path stringByExpandingTildeInPath];
    if (CreatePathToFile(user_path) &&
        [data writeToFile:user_path atomically:YES]) {
      chmod([user_path fileSystemRepresentation], Permissions() & ~0111);
      return user_path;
    }
  }
  return nil;
}

NSString* WriteBrandCode(const char* brand_code) {
  NSDictionary* brand_dict = @{
      kBrandKey: [NSString stringWithUTF8String:brand_code],
  };
  NSData* contents = [NSPropertyListSerialization
      dataFromPropertyList:brand_dict
                    format:NSPropertyListBinaryFormat_v1_0
          errorDescription:nil];

  return WriteData(contents, kSystemBrandPath, kUserBrandPath);
}

BOOL WriteMasterPrefs(const char* master_prefs_contents,
                      size_t master_prefs_contents_size) {
  NSData* contents = [NSData dataWithBytes:master_prefs_contents
                                    length:master_prefs_contents_size];
  return
      WriteData(contents, kSystemMasterPrefsPath, kUserMasterPrefsPath) != nil;
}

NSString* PathToFramework(NSString* app_path, NSDictionary* info_plist) {
  NSString* version = [info_plist objectForKey:@"CFBundleShortVersionString"];
  if (!version)
    return nil;
  return [[[app_path
      stringByAppendingPathComponent:@"Contents/Versions"]
      stringByAppendingPathComponent:version]
      stringByAppendingPathComponent:@"Google Chrome Framework.framework"];
}

NSString* PathToInstallScript(NSString* app_path, NSDictionary* info_plist) {
  return [PathToFramework(app_path, info_plist) stringByAppendingPathComponent:
      @"Resources/install.sh"];
}

NSString* PathToKeystoneResources(
    NSString* app_path, NSDictionary* info_plist) {
  return [PathToFramework(app_path, info_plist) stringByAppendingPathComponent:
      @"Frameworks/KeystoneRegistration.framework/Resources"];
}

NSString* FindOrInstallKeystone(NSString* app_path, NSDictionary* info_plist) {
  NSString* ks_path = kSystemKsadminPath;

  // Use user Keystone only if system Keystone doesn't exist /
  // isn't accessible.
  if (geteuid() != 0 &&
      ![[NSFileManager defaultManager] isExecutableFileAtPath:ks_path]) {
    ks_path = [kUserKsadminPath stringByExpandingTildeInPath];
  }

  // Always run install.py. It won't overwrite an existing keystone, but
  // it might update it or repair a broken existing installation.
  NSString* ks_resources = PathToKeystoneResources(app_path, info_plist);
  NSString* ks_install =
      [ks_resources stringByAppendingPathComponent:@"install.py"];
  NSString* ks_tbz =
      [ks_resources stringByAppendingPathComponent:@"Keystone.tbz"];
  @try {
    NSTask* task = [[[NSTask alloc] init] autorelease];
    [task setLaunchPath:ks_install];
    [task setArguments:@[@"--install", ks_tbz]];
    [task launch];
    [task waitUntilExit];
    if ([task terminationStatus] == 0)
      return ks_path;
  }
  @catch (id exception) {
    // Ignore.
  }
  return nil;
}

bool isbrandchar(int c) {
  // Always four upper-case alpha chars.
  return c >= 'A' && c <= 'Z';
}

}  // namespace

int GoogleChromeCompatibilityCheck(unsigned* reasons) {
  unsigned local_reasons = 0;

  NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];

  if (!IsOSXLeopardOrLater())
    local_reasons |= GCCC_ERROR_OSNOTSUPPORTED;

  if (HasChromeTicket(kSystemTicket))
    local_reasons |= GCCC_ERROR_SYSTEMLEVELALREADYPRESENT;

  if (geteuid() != 0 && HasChromeTicket(kUserTicket))
    local_reasons |= GCCC_ERROR_USERLEVELALREADYPRESENT;

  if (![[NSFileManager defaultManager] isWritableFileAtPath:@"/Applications"])
    local_reasons |= GCCC_ERROR_ACCESSDENIED;

  [pool drain];

  // Done. Copy/return results.
  if (reasons != NULL)
    *reasons = local_reasons;

  return local_reasons == 0;
}

int InstallGoogleChrome(const char* source_path,
                        const char* brand_code,
                        const char* master_prefs_contents,
                        unsigned master_prefs_contents_size) {
  if (!GoogleChromeCompatibilityCheck(NULL))
    return 0;

  @autoreleasepool {
    NSString* app_path = [NSString stringWithUTF8String:source_path];
    NSString* info_plist_path =
        [app_path stringByAppendingPathComponent:@"Contents/Info.plist"];
    NSDictionary* info_plist =
        [NSDictionary dictionaryWithContentsOfFile:info_plist_path];

    // Use install.sh from the Chrome app bundle to copy Chrome to its
    // destination.
    NSString* install_script = PathToInstallScript(app_path, info_plist);
    if (!install_script) {
      return 0;
    }

    @try {
      NSTask* task = [[[NSTask alloc] init] autorelease];
      [task setLaunchPath:install_script];
      [task setArguments:@[app_path, kChromeInstallPath]];
      [task launch];
      [task waitUntilExit];
      if ([task terminationStatus] != 0) {
        return 0;
      }
    }
    @catch (id exception) {
      return 0;
    }

    // Set brand code. If Chrome's Info.plist contains a brand code, use that.
    NSString* info_plist_brand = [info_plist objectForKey:kBrandKey];
    if (info_plist_brand &&
        [info_plist_brand respondsToSelector:@selector(UTF8String)])
      brand_code = [info_plist_brand UTF8String];

    BOOL valid_brand_code = brand_code && strlen(brand_code) == 4 &&
        isbrandchar(brand_code[0]) && isbrandchar(brand_code[1]) &&
        isbrandchar(brand_code[2]) && isbrandchar(brand_code[3]);

    NSString* brand_path = nil;
    if (valid_brand_code)
      brand_path = WriteBrandCode(brand_code);

    // Write master prefs.
    if (master_prefs_contents)
      WriteMasterPrefs(master_prefs_contents, master_prefs_contents_size);

    // Install Keystone if necessary.
    if (NSString* keystone = FindOrInstallKeystone(app_path, info_plist)) {
      // Register Chrome with Keystone.
      @try {
        NSTask* task = [[[NSTask alloc] init] autorelease];
        [task setLaunchPath:keystone];
        NSString* tag = [info_plist objectForKey:kChannelKey];
        [task setArguments:@[
            @"--register",
            @"--productid", [info_plist objectForKey:@"KSProductID"],
            @"--version", [info_plist objectForKey:kVersionKey],
            @"--xcpath", kChromeInstallPath,
            @"--url", [info_plist objectForKey:@"KSUpdateURL"],

            @"--tag", tag ? tag : @"",  // Stable channel
            @"--tag-path", info_plist_path,
            @"--tag-key", kChannelKey,

            @"--brand-path", brand_path ? brand_path : @"",
            @"--brand-key", brand_path ? kBrandKey: @"",

            @"--version-path", info_plist_path,
            @"--version-key", kVersionKey,
        ]];
        [task launch];
        [task waitUntilExit];
      }
      @catch (id exception) {
        // Chrome will try to register keystone on launch.
      }
    }

    // TODO Set default browser if requested. Will be tricky when running as
    // root.
  }
  return 1;
}

int LaunchGoogleChrome() {
  @autoreleasepool {
    return [[NSWorkspace sharedWorkspace] launchApplication:kChromeInstallPath];
  }
}
