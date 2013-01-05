// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/fullscreen.h"

#import <Cocoa/Cocoa.h>

#import "base/logging.h"

// Replicate specific 10.7 SDK declarations for building with prior SDKs.
#if !defined(MAC_OS_X_VERSION_10_7) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_7

enum {
  NSApplicationPresentationFullScreen = 1 << 10
};

#endif  // MAC_OS_X_VERSION_10_7

namespace {

BOOL AreOptionsFullScreen(NSApplicationPresentationOptions options) {
  // If both dock and menu bar are hidden, that is the equivalent of the Carbon
  // SystemUIMode (or Info.plist's LSUIPresentationMode) kUIModeAllHidden.
  if (((options & NSApplicationPresentationHideDock) ||
       (options & NSApplicationPresentationAutoHideDock)) &&
      ((options & NSApplicationPresentationHideMenuBar) ||
       (options & NSApplicationPresentationAutoHideMenuBar))) {
    return YES;
  }

  if (options & NSApplicationPresentationFullScreen)
    return YES;

  return NO;
}

}  // namespace

@interface FullScreenMonitor : NSObject {
 @private
  BOOL fullScreen_;
}

@property (nonatomic, getter=isFullScreen) BOOL fullScreen;

@end

@implementation FullScreenMonitor

@synthesize fullScreen = fullScreen_;

- (id)init {
  if ((self = [super init])) {
    [NSApp addObserver:self
            forKeyPath:@"currentSystemPresentationOptions"
               options:NSKeyValueObservingOptionNew |
                       NSKeyValueObservingOptionInitial
               context:nil];
  }
  return self;
}

- (void)dealloc {
  [NSApp removeObserver:self
             forKeyPath:@"currentSystemPresentationOptions"];
  [super dealloc];
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  NSApplicationPresentationOptions options =
      [[change objectForKey:NSKeyValueChangeNewKey] integerValue];
  [self setFullScreen:AreOptionsFullScreen(options)];
}

@end

static FullScreenMonitor* g_fullScreenMonitor = nil;

void InitFullScreenMonitor() {
  if (!g_fullScreenMonitor)
    g_fullScreenMonitor = [[FullScreenMonitor alloc] init];
}

void StopFullScreenMonitor() {
  [g_fullScreenMonitor release];
  g_fullScreenMonitor = nil;
}

bool IsFullScreenMode() {
  // Check if the main display has been captured (by games in particular).
  if (CGDisplayIsCaptured(CGMainDisplayID()))
    return true;

  return [g_fullScreenMonitor isFullScreen];
}
