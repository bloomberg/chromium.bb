// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/nsimage_cache_mac.h"

#import <AppKit/AppKit.h>

#include "base/logging.h"
#include "base/mac_util.h"

// When C++ exceptions are disabled, the C++ library defines |try| and
// |catch| so as to allow exception-expecting C++ code to build properly when
// language support for exceptions is not present.  These macros interfere
// with the use of |@try| and |@catch| in Objective-C files such as this one.
// Undefine these macros here, after everything has been #included, since
// there will be no C++ uses and only Objective-C uses from this point on.
#undef try
#undef catch

namespace nsimage_cache {

static NSMutableDictionary* image_cache = nil;

NSImage* ImageNamed(NSString* name) {
  DCHECK(name);

  // NOTE: to make this thread safe, we'd have to sync on the cache and
  // also force all the bundle calls on the main thread.

  if (!image_cache) {
    image_cache = [[NSMutableDictionary alloc] init];
    DCHECK(image_cache);
  }

  NSImage* result = [image_cache objectForKey:name];
  if (!result) {
    DLOG_IF(INFO, [[name pathExtension] length] == 0)
        << "Suggest including the extension in the image name";

    NSString* path = [mac_util::MainAppBundle() pathForImageResource:name];
    if (path) {
      @try {
        result = [[[NSImage alloc] initWithContentsOfFile:path] autorelease];
        if (result) {
          // Auto-template images with names ending in "Template".
          NSString* extensionlessName = [name stringByDeletingPathExtension];
          if ([extensionlessName hasSuffix:@"Template"])
            [result setTemplate:YES];

          [image_cache setObject:result forKey:name];
        }
      }
      @catch (id err) {
        DLOG(ERROR) << "Failed to load the image for name '"
            << [name UTF8String] << "' from path '" << [path UTF8String]
            << "', error: " << [[err description] UTF8String];
        result = nil;
      }
    }
  }

  // TODO: if we ever limit the cache size, this should retain & autorelease
  // the image.
  return result;
}

void Clear(void) {
  // NOTE: to make this thread safe, we'd have to sync on the cache.
  [image_cache removeAllObjects];
}

}  // namespace nsimage_cache
