// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/gcapi_mac/gcapi.h"

#import <Cocoa/Cocoa.h>
#include <sys/utsname.h>

namespace {

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

NSString* const kSystemKsadminPath =
    @"/Library/Google/GoogleSoftwareUpdate/GoogleSoftwareUpdate.bundle/"
     "Contents/MacOS/ksadmin";

NSString* const kUserKsadminPath =
    @"~/Library/Google/GoogleSoftwareUpdate/GoogleSoftwareUpdate.bundle/"
     "Contents/MacOS/ksadmin";

enum TicketKind {
  kSystemTicket, kUserTicket
};

BOOL HasChromeTicket(TicketKind kind) {
  // Don't use Objective-C 2 loop syntax, in case an installer runs on 10.4.
  NSMutableArray* keystonePaths =
      [NSMutableArray arrayWithObject:kUserKsadminPath];
  if (kind == kSystemTicket)
    [keystonePaths insertObject:kSystemKsadminPath atIndex:0];
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

}  // namespace

int GoogleChromeCompatibilityCheck(unsigned* reasons) {
  unsigned local_reasons = 0;

  NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];

  if (!IsOSXLeopardOrLater())
    local_reasons |= GCCC_ERROR_OSNOTSUPPORTED;

  if (HasChromeTicket(kSystemTicket))
    local_reasons |= GCCC_ERROR_SYSTEMLEVELALREADYPRESENT;

  if (HasChromeTicket(kUserTicket))
    local_reasons |= GCCC_ERROR_USERLEVELALREADYPRESENT;

  [pool drain];

  // Done. Copy/return results.
  if (reasons != NULL)
    *reasons = local_reasons;

  return local_reasons == 0;
}
