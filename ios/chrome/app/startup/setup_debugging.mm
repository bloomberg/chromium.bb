// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/app/startup/setup_debugging.h"

#include <objc/runtime.h>

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "components/crash/core/common/objc_zombie.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

#if !defined(NDEBUG)

// Swizzles [UIImage imageNamed:] to trigger a DCHECK if an invalid image is
// attempted to be loaded.
void swizzleUIImageImageNamed() {
  // Retained by the swizzle block.
  NSMutableSet* whiteList = [NSMutableSet set];

  // TODO(crbug.com/720337): Add missing image.
  [whiteList addObject:@"card_close_button_pressed_incognito"];
  // TODO(crbug.com/720355): Add missing image.
  [whiteList addObject:@"find_close_pressed_incognito"];
  // TODO(crbug.com/720338): Add missing images.
  [whiteList addObject:@"glif-mic-to-dots-large_37"];
  [whiteList addObject:@"glif-google-to-dots_28"];
  // TODO(crbug.com/721338): Add missing image.
  [whiteList addObject:@"voice_icon_keyboard_accessory"];
  [whiteList addObject:@"voice_icon"];

  // The original implementation of [UIImage imageNamed:].
  // Called by the new implementation.
  static IMP originalImp;
  IMP* originalImpPtr = &originalImp;

  id swizzleBlock = ^(id self, NSString* imageName) {
    // Call the original [UIImage imageNamed:] method.
    UIImage* (*imp)(id, SEL, id) = (UIImage*(*)(id,SEL,id))*originalImpPtr;
    Class aClass = objc_getClass("UIImage");
    UIImage* image = imp(aClass, @selector(imageNamed:), imageName);

    if (![whiteList containsObject:imageName]) {
      DCHECK(image) << "Missing image: " << base::SysNSStringToUTF8(imageName);
    }
    return image;
  };

  Method method = class_getClassMethod([UIImage class], @selector(imageNamed:));
  DCHECK(method);

  IMP blockImp = imp_implementationWithBlock(swizzleBlock);
  originalImp = method_setImplementation(method, blockImp);
}

#endif  // !defined(NDEBUG) && TARGET_IPHONE_SIMULATOR

}  // namespace

@implementation SetupDebugging

+ (void)setUpDebuggingOptions {
// Enable the zombie treadmill on simulator builds.
// TODO(crbug.com/663390): Consider enabling this on device builds too.
#if TARGET_IPHONE_SIMULATOR
  DCHECK(ObjcEvilDoers::ZombieEnable(true, 10000));
#endif

#if !defined(NDEBUG)
  // Enable the detection of missing image assets.
  swizzleUIImageImageNamed();
#endif
}

@end
