// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/app/startup/setup_debugging.h"

#include "base/logging.h"
#include "components/crash/core/common/objc_zombie.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation SetupDebugging

+ (void)setUpDebuggingOptions {
// Enable the zombie treadmill on simulator builds.
// TODO(crbug.com/663390): Consider enabling this on device builds too.
#if TARGET_IPHONE_SIMULATOR
  DCHECK(ObjcEvilDoers::ZombieEnable(true, 10000));
#endif
}

@end
