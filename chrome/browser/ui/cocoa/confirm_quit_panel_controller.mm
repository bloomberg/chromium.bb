// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>

#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/scoped_nsobject.h"
#include "base/sys_string_conversions.h"
#import "chrome/browser/ui/cocoa/confirm_quit_panel_controller.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util_mac.h"

@interface ConfirmQuitPanelController (Private)
- (id)initInternal;
- (void)animateFadeOut;
- (NSString*)keyCommandString;
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
      [base::mac::MainAppBundle() pathForResource:@"ConfirmQuitPanel"
                                          ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibPath owner:self])) {
  }
  return self;
}

- (void)awakeFromNib {
  DCHECK([self window]);
  DCHECK_EQ(self, [[self window] delegate]);
  NSString* message = l10n_util::GetNSStringF(IDS_CONFIRM_TO_QUIT_DESCRIPTION,
      base::SysNSStringToUTF16([self keyCommandString]));
  [message_ setStringValue:message];
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

// This looks at the Main Menu and determines what the user has set as the
// key combination for quit. It then gets the modifiers and builds an object
// to hold the data.
+ (ui::AcceleratorCocoa)quitAccelerator {
  NSMenu* mainMenu = [NSApp mainMenu];
  // Get the application menu (i.e. Chromium).
  NSMenu* appMenu = [[mainMenu itemAtIndex:0] submenu];
  for (NSMenuItem* item in [appMenu itemArray]) {
    // Find the Quit item.
    if ([item action] == @selector(terminate:)) {
      return ui::AcceleratorCocoa([item keyEquivalent],
                                  [item keyEquivalentModifierMask]);
    }
  }
  // Default to Cmd+Q.
  return ui::AcceleratorCocoa(@"q", NSCommandKeyMask);
}

// This looks at the Main Menu and determines what the user has set as the
// key combination for quit. It then gets the modifiers and builds a string
// to display them.
- (NSString*)keyCommandString {
  ui::AcceleratorCocoa accelerator = [[self class] quitAccelerator];
  return [self keyCombinationForAccelerator:accelerator];
}

- (NSString*)keyCombinationForAccelerator:(const ui::AcceleratorCocoa&)item {
  NSMutableString* string = [NSMutableString string];
  NSUInteger modifiers = item.modifiers();

  if (modifiers & NSCommandKeyMask)
    [string appendString:@"\u2318"];
  if (modifiers & NSControlKeyMask)
    [string appendString:@"\u2303"];
  if (modifiers & NSAlternateKeyMask)
    [string appendString:@"\u2325"];
  if (modifiers & NSShiftKeyMask)
    [string appendString:@"\u21E7"];

  [string appendString:[item.characters() uppercaseString]];
  return string;
}

@end
