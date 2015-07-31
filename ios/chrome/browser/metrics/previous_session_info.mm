// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/previous_session_info.h"

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "components/version_info/version_info.h"

namespace {

// Key in the UserDefaults for a boolean value keeping track of memory warnings.
NSString* const kDidSeeMemoryWarningShortlyBeforeTerminating =
    @"DidSeeMemoryWarning";

// Key in the NSUserDefaults for a string value that stores the version of the
// last session.
NSString* const kLastRanVersion = @"LastRanVersion";

}  // namespace

@interface PreviousSessionInfo ()

// Whether beginRecordingCurrentSession was called.
@property(nonatomic, assign) BOOL didBeginRecordingCurrentSession;

// Redefined to be read-write.
@property(nonatomic, assign) BOOL didSeeMemoryWarningShortlyBeforeTerminating;
@property(nonatomic, assign) BOOL isFirstSessionAfterUpgrade;

@end

@implementation PreviousSessionInfo

@synthesize didBeginRecordingCurrentSession = _didBeginRecordingCurrentSession;
@synthesize didSeeMemoryWarningShortlyBeforeTerminating =
    _didSeeMemoryWarningShortlyBeforeTerminating;
@synthesize isFirstSessionAfterUpgrade = _isFirstSessionAfterUpgrade;

// Singleton PreviousSessionInfo.
static PreviousSessionInfo* gSharedInstance = nil;

+ (instancetype)sharedInstance {
  if (!gSharedInstance) {
    gSharedInstance = [[PreviousSessionInfo alloc] init];

    // Load the persisted information.
    NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
    gSharedInstance.didSeeMemoryWarningShortlyBeforeTerminating =
        [defaults boolForKey:kDidSeeMemoryWarningShortlyBeforeTerminating];
    NSString* lastRanVersion = [defaults stringForKey:kLastRanVersion];
    NSString* currentVersion =
        base::SysUTF8ToNSString(version_info::GetVersionNumber());
    gSharedInstance.isFirstSessionAfterUpgrade =
        ![lastRanVersion isEqualToString:currentVersion];
  }
  return gSharedInstance;
}

+ (void)resetSharedInstanceForTesting {
  [gSharedInstance release];
  gSharedInstance = nil;
}

- (void)beginRecordingCurrentSession {
  if (self.didBeginRecordingCurrentSession)
    return;
  self.didBeginRecordingCurrentSession = YES;

  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];

  // Set the new version.
  NSString* currentVersion =
      base::SysUTF8ToNSString(version_info::GetVersionNumber());
  [defaults setObject:currentVersion forKey:kLastRanVersion];

  // Clear the memory warning flag.
  [defaults removeObjectForKey:kDidSeeMemoryWarningShortlyBeforeTerminating];

  // Save critical state information for crash detection.
  [defaults synchronize];
}

- (void)setMemoryWarningFlag {
  if (!self.didBeginRecordingCurrentSession)
    return;

  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults setBool:YES forKey:kDidSeeMemoryWarningShortlyBeforeTerminating];
  // Save critical state information for crash detection.
  [defaults synchronize];
}

- (void)resetMemoryWarningFlag {
  if (!self.didBeginRecordingCurrentSession)
    return;

  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults removeObjectForKey:kDidSeeMemoryWarningShortlyBeforeTerminating];
  // Save critical state information for crash detection.
  [defaults synchronize];
}

@end
