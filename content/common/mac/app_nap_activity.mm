// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/mac/app_nap_activity.h"

#import <Foundation/Foundation.h>

#include "base/mac/scoped_nsobject.h"

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
