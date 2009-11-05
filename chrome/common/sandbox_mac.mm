// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/sandbox_mac.h"

#include "base/debug_util.h"

#import <Cocoa/Cocoa.h>
extern "C" {
#include <sandbox.h>
}

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/json/string_escape.h"
#include "base/mac_util.h"
#include "base/scoped_cftyperef.h"
#include "base/scoped_nsautorelease_pool.h"
#include "base/string16.h"
#include "base/sys_info.h"
#include "base/sys_string_conversions.h"
#include "chrome/common/chrome_switches.h"

namespace sandbox {

// Warm up System APIs that empirically need to be accessed before the Sandbox
// is turned on.
// This method is layed out in blocks, each one containing a separate function
// that needs to be warmed up. The OS version on which we found the need to
// enable the function is also noted.
// This function is tested on the following OS versions:
//     10.5.6, 10.6.0
void SandboxWarmup() {
  base::ScopedNSAutoreleasePool scoped_pool;

  { // CGColorSpaceCreateWithName(), CGBitmapContextCreate() - 10.5.6
    scoped_cftyperef<CGColorSpaceRef> rgb_colorspace(
        CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB));

    // Allocate a 1x1 image.
    char data[4];
    scoped_cftyperef<CGContextRef> context(
        CGBitmapContextCreate(data, 1, 1, 8, 1 * 4,
                              rgb_colorspace,
                              kCGImageAlphaPremultipliedFirst |
                                  kCGBitmapByteOrder32Host));

    // Load in the color profiles we'll need (as a side effect).
    (void) mac_util::GetSRGBColorSpace();
    (void) mac_util::GetSystemColorSpace();

    // CGColorSpaceCreateSystemDefaultCMYK - 10.6
    scoped_cftyperef<CGColorSpaceRef> cmyk_colorspace(
        CGColorSpaceCreateWithName(kCGColorSpaceGenericCMYK));
  }

  { // [-NSColor colorUsingColorSpaceName] - 10.5.6
    NSColor* color = [NSColor controlTextColor];
    [color colorUsingColorSpaceName:NSCalibratedRGBColorSpace];
  }

  { // localtime() - 10.5.6
    time_t tv = {0};
    localtime(&tv);
  }

  { // Gestalt() tries to read /System/Library/CoreServices/SystemVersion.plist
    // on 10.5.6
    int32 tmp;
    base::SysInfo::OperatingSystemVersionNumbers(&tmp, &tmp, &tmp);
  }

  {  // CGImageSourceGetStatus() - 10.6
     // Create a png with just enough data to get everything warmed up...
    char png_header[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    NSData* data = [NSData dataWithBytes:png_header
                                  length:arraysize(png_header)];
    scoped_cftyperef<CGImageSourceRef> img(
        CGImageSourceCreateWithData((CFDataRef)data,
        NULL));
    CGImageSourceGetStatus(img);
  }
}

// Turns on the OS X sandbox for this process.
bool EnableSandbox() {
  // For the renderer, we give it a custom sandbox to lock things down as
  // tightly as possible, while still enabling drawing.
  NSString* sandbox_profile_path =
      [mac_util::MainAppBundle() pathForResource:@"renderer" ofType:@"sb"];
  NSString* sandbox_data = [NSString
      stringWithContentsOfFile:sandbox_profile_path
      encoding:NSUTF8StringEncoding
      error:nil];

  if (!sandbox_data) {
    LOG(ERROR) << "Failed to find the sandbox profile on disk";
    return false;
  }

  // Enable verbose logging if enabled on the command line.
  // (see renderer.sb for details).
  const CommandLine *command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableSandboxLogging)) {
    sandbox_data = [sandbox_data
        stringByReplacingOccurrencesOfString:@";ENABLE_LOGGING"
                                  withString:@""];
  }

  int32 major_version, minor_version, bugfix_version;
  base::SysInfo::OperatingSystemVersionNumbers(&major_version,
      &minor_version, &bugfix_version);

  if (major_version > 10 || (major_version == 10 && minor_version >= 6)) {
    // 10.6-only Sandbox rules.
    sandbox_data = [sandbox_data
        stringByReplacingOccurrencesOfString:@";10.6_ONLY"
                                  withString:@""];
    // Splice the path of the user's home directory into the sandbox profile
    // (see renderer.sb for details).
    // This code is in the 10.6-only block because the sandbox syntax we use
    // for this "subdir" is only supported on 10.6.
    // If we ever need this on pre-10.6 OSs then we'll have to rethink the
    // surrounding sandbox syntax.
    string16 home_dir = base::SysNSStringToUTF16(NSHomeDirectory());
    std::string home_dir_escaped;
    base::JsonDoubleQuote(home_dir, false, &home_dir_escaped);
    NSString* home_dir_escaped_ns = base::SysUTF8ToNSString(home_dir_escaped);
    sandbox_data = [sandbox_data
        stringByReplacingOccurrencesOfString:@"USER_HOMEDIR"
                                  withString:home_dir_escaped_ns];
  }

  char* error_buff = NULL;
  int error = sandbox_init([sandbox_data UTF8String], 0, &error_buff);
  bool success = (error == 0 && error_buff == NULL);
  if (error == -1) {
    LOG(ERROR) << "Failed to Initialize Sandbox: " << error_buff;
  }
  sandbox_free_error(error_buff);
  return success;
}

}  // namespace sandbox
