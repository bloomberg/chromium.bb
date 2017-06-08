// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/mac/app_nap_activity.h"

#import <Foundation/Foundation.h>

#include "base/mac/scoped_nsobject.h"

extern "C" {
void __CFRunLoopSetOptionsReason(uint64_t options,
                                 NSString* reason,
                                 int unused);
}

namespace content {

namespace {

NSString* const kActivityReason = @"Process foregrounded";
const NSActivityOptions kActivityOptions =
    (NSActivityUserInitiatedAllowingIdleSystemSleep |
     NSActivityLatencyCritical) &
    ~(NSActivitySuddenTerminationDisabled |
      NSActivityAutomaticTerminationDisabled);

}  // namespace

struct AssertionWrapper {
  base::scoped_nsobject<id> obj;
};

AppNapActivity::AppNapActivity() {
  assertion_.reset(new AssertionWrapper());
};

AppNapActivity::~AppNapActivity() {
  DCHECK(!assertion_->obj.get());
};

void AppNapActivity::InitializeAppNapSupport() {
  // Reason strings are the same as
  // what macOS sends in the corresponding call.
  // |options| (argument 1) are magic numbers as found in the
  // callsites mentioned above.
  //
  // Normally happens during launch services check-in. (HIToolbox)
  __CFRunLoopSetOptionsReason(
      1, @"Finished checking in as application - waiting for events", 0);
  // Normally happens in a dispatch_once in the NSApplication event loop.
  // (CoreFoundation).
  __CFRunLoopSetOptionsReason(
      0x3b000000, @"Finished delay after app launch and bundle check", 0);
}

void AppNapActivity::Begin() {
  DCHECK(!assertion_->obj.get());
  id assertion =
      [[NSProcessInfo processInfo] beginActivityWithOptions:kActivityOptions
                                                     reason:kActivityReason];
  assertion_->obj.reset([assertion retain]);
}

void AppNapActivity::End() {
  id assertion = assertion_->obj.autorelease();
  DCHECK(assertion);
  [[NSProcessInfo processInfo] endActivity:assertion];
}

}  // namespace content
