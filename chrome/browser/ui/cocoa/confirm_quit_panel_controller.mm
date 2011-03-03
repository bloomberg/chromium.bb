// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>

#include "base/logging.h"
#include "base/scoped_nsobject.h"
#include "base/sys_string_conversions.h"
#import "chrome/browser/ui/cocoa/confirm_quit_panel_controller.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util_mac.h"

// Custom Content View /////////////////////////////////////////////////////////

// The content view of the window that draws a custom frame.
@interface ConfirmQuitFrameView : NSView {
 @private
  NSTextField* message_;  // Weak, owned by the view hierarchy.
}
- (void)setMessageText:(NSString*)text;
@end

@implementation ConfirmQuitFrameView

- (id)initWithFrame:(NSRect)frameRect {
  if ((self = [super initWithFrame:frameRect])) {
    scoped_nsobject<NSTextField> message(
        // The frame will be fixed up when |-setMessageText:| is called.
        [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 0, 0)]);
    message_ = message.get();
    [message_ setEditable:NO];
    [message_ setSelectable:NO];
    [message_ setBezeled:NO];
    [message_ setDrawsBackground:NO];
    [message_ setFont:[NSFont boldSystemFontOfSize:24]];
    [message_ setTextColor:[NSColor whiteColor]];
    [self addSubview:message_];
  }
  return self;
}

- (void)drawRect:(NSRect)dirtyRect {
  const CGFloat kCornerRadius = 5.0;
  NSBezierPath* path = [NSBezierPath bezierPathWithRoundedRect:[self bounds]
                                                       xRadius:kCornerRadius
                                                       yRadius:kCornerRadius];

  NSColor* fillColor = [NSColor colorWithCalibratedWhite:0.2 alpha:0.75];
  [fillColor set];
  [path fill];
}

- (void)setMessageText:(NSString*)text {
  const CGFloat kHorizontalPadding = 30;

  // Style the string.
  scoped_nsobject<NSMutableAttributedString> attrString(
      [[NSMutableAttributedString alloc] initWithString:text]);
  scoped_nsobject<NSShadow> textShadow([[NSShadow alloc] init]);
  [textShadow setShadowColor:[NSColor colorWithCalibratedWhite:0 alpha:0.6]];
  [textShadow setShadowOffset:NSMakeSize(0, -1)];
  [textShadow setShadowBlurRadius:1.0];
  [attrString addAttribute:NSShadowAttributeName
                     value:textShadow
                     range:NSMakeRange(0, [text length])];
  [message_ setAttributedStringValue:attrString];

  // Fixup the frame of the string.
  [message_ sizeToFit];
  NSRect messageFrame = [message_ frame];
  NSRect frame = [[self window] frame];

  if (NSWidth(messageFrame) > NSWidth(frame))
    frame.size.width = NSWidth(messageFrame) + kHorizontalPadding;

  messageFrame.origin.y = NSMidY(frame) - NSMidY(messageFrame);
  messageFrame.origin.x = NSMidX(frame) - NSMidX(messageFrame);

  [[self window] setFrame:frame display:YES];
  [message_ setFrame:messageFrame];
}

@end

// Private Interface ///////////////////////////////////////////////////////////

@interface ConfirmQuitPanelController (Private)
- (void)animateFadeOut;
- (NSString*)keyCommandString;
@end

ConfirmQuitPanelController* g_confirmQuitPanelController = nil;

////////////////////////////////////////////////////////////////////////////////

@implementation ConfirmQuitPanelController

+ (ConfirmQuitPanelController*)sharedController {
  if (!g_confirmQuitPanelController) {
    g_confirmQuitPanelController =
        [[ConfirmQuitPanelController alloc] init];
  }
  return g_confirmQuitPanelController;
}

- (id)init {
  const NSRect kWindowFrame = NSMakeRect(0, 0, 350, 70);
  scoped_nsobject<NSWindow> window(
      [[NSWindow alloc] initWithContentRect:kWindowFrame
                                  styleMask:NSBorderlessWindowMask
                                    backing:NSBackingStoreBuffered
                                      defer:NO]);
  if ((self = [super initWithWindow:window])) {
    [window setDelegate:self];
    [window setBackgroundColor:[NSColor clearColor]];
    [window setOpaque:NO];
    [window setHasShadow:NO];

    // Create the content view. Take the frame from the existing content view.
    NSRect frame = [[window contentView] frame];
    scoped_nsobject<ConfirmQuitFrameView> frameView(
        [[ConfirmQuitFrameView alloc] initWithFrame:frame]);
    contentView_ = frameView.get();
    [window setContentView:contentView_];

    // Set the proper string.
    NSString* message = l10n_util::GetNSStringF(IDS_CONFIRM_TO_QUIT_DESCRIPTION,
        base::SysNSStringToUTF16([self keyCommandString]));
    [contentView_ setMessageText:message];
  }
  return self;
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
