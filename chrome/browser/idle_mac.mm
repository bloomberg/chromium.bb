// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/idle.h"

#import <Cocoa/Cocoa.h>
#import <CoreGraphics/CGEventSource.h>

@interface MacScreenMonitor : NSObject {
 @private
  BOOL screensaverRunning_;
  BOOL screenLocked_;
}

@property (readonly,
           nonatomic,
           getter=isScreensaverRunning) BOOL screensaverRunning;
@property (readonly, nonatomic, getter=isScreenLocked) BOOL screenLocked;

@end

@implementation MacScreenMonitor

@synthesize screensaverRunning = screensaverRunning_;
@synthesize screenLocked = screenLocked_;

- (id)init {
  if ((self = [super init])) {
    NSDistributedNotificationCenter* distCenter =
          [NSDistributedNotificationCenter defaultCenter];
    [distCenter addObserver:self
                   selector:@selector(onScreenSaverStarted:)
                       name:@"com.apple.screensaver.didstart"
                     object:nil];
    [distCenter addObserver:self
                   selector:@selector(onScreenSaverStopped:)
                       name:@"com.apple.screensaver.didstop"
                     object:nil];
    [distCenter addObserver:self
                   selector:@selector(onScreenLocked:)
                       name:@"com.apple.screenIsLocked"
                     object:nil];
    [distCenter addObserver:self
                   selector:@selector(onScreenUnlocked:)
                       name:@"com.apple.screenIsUnlocked"
                     object:nil];
  }
  return self;
}

- (void)dealloc {
  [[NSDistributedNotificationCenter defaultCenter] removeObject:self];
  [super dealloc];
}

- (void)onScreenSaverStarted:(NSNotification*)notification {
   screensaverRunning_ = YES;
}

- (void)onScreenSaverStopped:(NSNotification*)notification {
   screensaverRunning_ = NO;
}

- (void)onScreenLocked:(NSNotification*)notification {
   screenLocked_ = YES;
}

- (void)onScreenUnlocked:(NSNotification*)notification {
   screenLocked_ = NO;
}

@end

static MacScreenMonitor* g_screenMonitor = nil;

void InitIdleMonitor() {
  if (!g_screenMonitor)
    g_screenMonitor = [[MacScreenMonitor alloc] init];
}

void StopIdleMonitor() {
  [g_screenMonitor release];
  g_screenMonitor = nil;
}

IdleState CalculateIdleState(unsigned int idle_threshold) {
  if ([g_screenMonitor isScreensaverRunning] ||
      [g_screenMonitor isScreenLocked])
    return IDLE_STATE_LOCKED;

  CFTimeInterval idle_time = CGEventSourceSecondsSinceLastEventType(
      kCGEventSourceStateCombinedSessionState,
      kCGAnyInputEventType);
  if (idle_time >= idle_threshold)
    return IDLE_STATE_IDLE;

  return IDLE_STATE_ACTIVE;
}
