// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/fullscreen.h"

#import <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>

#import "base/logging.h"

@interface FullScreenMonitor : NSObject {
 @private
  BOOL fullScreen_;
  EventHandlerRef eventHandler_;
}

@property (nonatomic, getter=isFullScreen) BOOL fullScreen;

@end

static OSStatus handleAppEvent(EventHandlerCallRef myHandler,
                               EventRef event,
                               void* userData) {
  DCHECK(userData);

  FullScreenMonitor* fullScreenMonitor =
      reinterpret_cast<FullScreenMonitor*>(userData);

  UInt32 mode = 0;
  OSStatus status = GetEventParameter(event,
                                      kEventParamSystemUIMode,
                                      typeUInt32,
                                      NULL,
                                      sizeof(UInt32),
                                      NULL,
                                      &mode);
  if (status != noErr)
    return status;
  BOOL isFullScreenMode = mode == kUIModeAllHidden;
  [fullScreenMonitor setFullScreen:isFullScreenMode];
  return noErr;
}

@implementation FullScreenMonitor

@synthesize fullScreen = fullScreen_;

- (id)init {
  if ((self = [super init])) {
    // Check if the user is in presentation mode initially.
    SystemUIMode currentMode;
    GetSystemUIMode(&currentMode, NULL);
    fullScreen_ = currentMode == kUIModeAllHidden;

    // Register a Carbon event to receive the notification about the login
    // session's UI mode change.
    EventTypeSpec events[] =
      {{ kEventClassApplication, kEventAppSystemUIModeChanged }};
    OSStatus status = InstallApplicationEventHandler(
        NewEventHandlerUPP(handleAppEvent),
        GetEventTypeCount(events),
        events,
        self,
        &eventHandler_);
    if (status) {
      [self release];
      self = nil;
    }
  }
  return self;
}

- (void)dealloc {
  if (eventHandler_)
    RemoveEventHandler(eventHandler_);
  [super dealloc];
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
  // Check if the main display has been captured (game in particular).
  if (CGDisplayIsCaptured(CGMainDisplayID()))
    return true;

  return [g_fullScreenMonitor isFullScreen];
}
