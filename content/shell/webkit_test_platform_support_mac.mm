// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "content/shell/webkit_test_platform_support.h"

#include <AppKit/AppKit.h>
#include <Foundation/Foundation.h>

namespace content {

bool WebKitTestPlatformInitialize() {

  // Load font files in the resource folder.
  static const char* const fontFileNames[] = {
      "AHEM____.TTF",
      "WebKitWeightWatcher100.ttf",
      "WebKitWeightWatcher200.ttf",
      "WebKitWeightWatcher300.ttf",
      "WebKitWeightWatcher400.ttf",
      "WebKitWeightWatcher500.ttf",
      "WebKitWeightWatcher600.ttf",
      "WebKitWeightWatcher700.ttf",
      "WebKitWeightWatcher800.ttf",
      "WebKitWeightWatcher900.ttf",
  };

  // mainBundle is Content Shell Helper.app.  Go two levels up to find
  // Content Shell.app. Due to DumpRenderTree injecting the font files into
  // its direct dependents, it's not easily possible to put the ttf files into
  // the helper's resource directory instead of the outer bundle's resource
  // directory.
  NSString* bundle = [[NSBundle mainBundle] bundlePath];
  bundle = [bundle stringByAppendingPathComponent:@"../.."];
  NSURL* resources_directory = [[NSBundle bundleWithPath:bundle] resourceURL];

  NSMutableArray* font_urls = [NSMutableArray array];
  for (unsigned i = 0; i < arraysize(fontFileNames); ++i) {
    NSURL* font_url = [resources_directory
        URLByAppendingPathComponent:[NSString
            stringWithUTF8String:fontFileNames[i]]];
    [font_urls addObject:[font_url absoluteURL]];
  }

  CFArrayRef errors = 0;
  if (!CTFontManagerRegisterFontsForURLs((CFArrayRef)font_urls,
                                         kCTFontManagerScopeProcess,
                                         &errors)) {
    DLOG(FATAL) << "Fail to activate fonts.";
    CFRelease(errors);
  }

  return true;
}

}  // namespace
