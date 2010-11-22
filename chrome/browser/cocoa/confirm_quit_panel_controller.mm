// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>

#include "base/logging.h"
#include "base/mac_util.h"
#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/confirm_quit_panel_controller.h"

@interface ConfirmQuitPanelController (Private)
- (id)initInternal;
- (void)animateFadeOut;
@end

ConfirmQuitPanelController* g_confirmQuitPanelController = nil;

@implementation ConfirmQuitPanelController

+ (ConfirmQuitPanelController*)sharedController {
  if (!g_confirmQuitPanelController) {
    g_confirmQuitPanelController =
        [[ConfirmQuitPanelController alloc] initInternal];
  }
  return g_confirmQuitPanelController;
}

- (id)initInternal {
  NSString* nibPath =
      [mac_util::MainAppBundle() pathForResource:@"ConfirmQuitPanel"
                                          ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibPath owner:self])) {
  }
  return self;
}

- (void)awakeFromNib {
  DCHECK([self window]);
  DCHECK_EQ(self, [[self window] delegate]);
}

- (void)windowWillClose:(NSNotification*)notif {
  // Release all animations because CAAnimation retains its delegate (self),
  // which will cause a retain cycle. Break it!
  [[self window] setAnimations:[NSDictionary dictionary]];
  g_confirmQuitPanelController = nil;
  [self autorelease];
}

- (void)showWindow:(id)sender {
  // If a panel that is fading out is going to be reused here, make sure it
  // does not get released when the animation finishes.
  scoped_nsobject<ConfirmQuitPanelController> stayAlive([self retain]);
  [[self window] setAnimations:[NSDictionary dictionary]];
  [[self window] center];
  [[self window] setAlphaValue:1.0];
  [super showWindow:sender];
}

- (void)dismissPanel {
  [self performSelector:@selector(animateFadeOut)
             withObject:nil
             afterDelay:1.0];
}

- (void)animateFadeOut {
  NSWindow* window = [self window];
  scoped_nsobject<CAAnimation> animation(
      [[window animationForKey:@"alphaValue"] copy]);
  [animation setDelegate:self];
  [animation setDuration:0.2];
  NSMutableDictionary* dictionary =
      [NSMutableDictionary dictionaryWithDictionary:[window animations]];
  [dictionary setObject:animation forKey:@"alphaValue"];
  [window setAnimations:dictionary];
  [[window animator] setAlphaValue:0.0];
}

- (void)animationDidStop:(CAAnimation*)theAnimation finished:(BOOL)finished {
  [self close];
}

@end
