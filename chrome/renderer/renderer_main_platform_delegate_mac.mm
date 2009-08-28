// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/renderer_main_platform_delegate.h"

#include "base/debug_util.h"

#import <Foundation/Foundation.h>
#import <ApplicationServices/ApplicationServices.h>
#import <Cocoa/Cocoa.h>
extern "C" {
#include <sandbox.h>
}

#include "base/mac_util.h"
#include "base/scoped_cftyperef.h"
#include "base/scoped_nsautorelease_pool.h"
#include "base/sys_info.h"
#include "chrome/common/chrome_switches.h"
#include "third_party/WebKit/WebKit/mac/WebCoreSupport/WebSystemInterface.h"

RendererMainPlatformDelegate::RendererMainPlatformDelegate(
    const MainFunctionParams& parameters)
        : parameters_(parameters) {
}

RendererMainPlatformDelegate::~RendererMainPlatformDelegate() {
}

// Warmup System APIs that empirically need to be accessed before the Sandbox
// is turned on.
// This method is layed out in blocks, each one containing a separate function
// that needs to be warmed up. The OS version on which we found the need to
// enable the function is also noted.
// This function is tested on the following OS versions:
//     10.5.6, 10.6 seed release
void SandboxWarmup() {
  base::ScopedNSAutoreleasePool scoped_pool;

  { // CGColorSpaceCreateWithName(), CGBitmapContextCreate() - 10.5.6
    CGColorSpaceRef rgb_colorspace =
        CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB);

    // Allocate a 1 byte image.
    char data[8];
    CGContextRef tmp = CGBitmapContextCreate(data, 1, 1, 8, 1*8,
                                             rgb_colorspace,
                                             kCGImageAlphaPremultipliedFirst |
                                             kCGBitmapByteOrder32Host);

    CGColorSpaceRelease(rgb_colorspace);
    CGContextRelease(tmp);
  }

  {  // [-NSColor colorUsingColorSpaceName] - 10.5.6
    NSColor* color = [NSColor controlTextColor];
    [color colorUsingColorSpaceName:NSCalibratedRGBColorSpace];
  }

  {  // localtime() - 10.5.6
    time_t tv = {0};
    localtime(&tv);
  }

  { // Gestalt() tries to read /System/Library/CoreServices/SystemVersion.plist
    // on 10.5.6
    int32 tmp;
    base::SysInfo::OperatingSystemVersionNumbers(&tmp, &tmp, &tmp);
  }

  {  // CGImageSourceGetStatus() - 10.6 seed release.
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

// TODO(mac-port): Any code needed to initialize a process for
// purposes of running a renderer needs to also be reflected in
// chrome_dll_main.cc for --single-process support.
void RendererMainPlatformDelegate::PlatformInitialize() {
  // Load WebKit system interfaces.
  InitWebCoreSystemInterface();

  // Warmup APIs before turning on the Sandbox.
  SandboxWarmup();

  if (![NSThread isMultiThreaded]) {
    NSString* string = @"";
    [NSThread detachNewThreadSelector:@selector(length)
                             toTarget:string
                           withObject:nil];
  }

  // Initialize Cocoa.  Without this call, drawing of native UI
  // elements (e.g. buttons) in WebKit will explode.
  [NSApplication sharedApplication];
}

void RendererMainPlatformDelegate::PlatformUninitialize() {
}

bool RendererMainPlatformDelegate::InitSandboxTests(bool no_sandbox) {
  return true;
}

bool RendererMainPlatformDelegate::EnableSandbox() {
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

  // Splice the path of the user's home directory into the sandbox profile
  // (see renderer.sb for details).
  sandbox_data = [sandbox_data
      stringByReplacingOccurrencesOfString:@"USER_HOMEDIR"
                                withString:NSHomeDirectory()];

  char* error_buff = NULL;
  int error = sandbox_init([sandbox_data UTF8String], 0, &error_buff);
  bool success = (error == 0 && error_buff == NULL);
  if (error == -1) {
    LOG(ERROR) << "Failed to Initialize Sandbox: " << error_buff;
  }
  sandbox_free_error(error_buff);
  return success;
}

void RendererMainPlatformDelegate::RunSandboxTests() {
  // TODO(port): Run sandbox unit test here.
}
