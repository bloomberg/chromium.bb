// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>

#include "base/logging.h"
#include "base/memory/scoped_nsobject.h"
#include "base/metrics/histogram.h"
#include "base/sys_string_conversions.h"
#import "chrome/browser/ui/cocoa/confirm_quit_panel_controller.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util_mac.h"

// Constants ///////////////////////////////////////////////////////////////////

// How long the user must hold down Cmd+Q to confirm the quit.
const NSTimeInterval kTimeToConfirmQuit = 1.5;

// Leeway between the |targetDate| and the current time that will confirm a
// quit.
const NSTimeInterval kTimeDeltaFuzzFactor = 1.0;

// Duration of the window fade out animation.
const NSTimeInterval kWindowFadeAnimationDuration = 0.2;

// For metrics recording only: How long the user must hold the keys to
// differentitate kDoubleTap from kTapHold.
const NSTimeInterval kDoubleTapTimeDelta = 0.32;

// Functions ///////////////////////////////////////////////////////////////////

namespace confirm_quit {

void RecordHistogram(ConfirmQuitMetric sample) {
  HISTOGRAM_ENUMERATION("ConfirmToQuit", sample, kSampleCount);
}

}  // namespace confirm_quit

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
  [textShadow.get() setShadowColor:[NSColor colorWithCalibratedWhite:0
                                                               alpha:0.6]];
  [textShadow.get() setShadowOffset:NSMakeSize(0, -1)];
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

// Animation ///////////////////////////////////////////////////////////////////

// This animation will run through all the windows of the passed-in
// NSApplication and will fade their alpha value to 0.0. When the animation is
// complete, this will release itself.
@interface FadeAllWindowsAnimation : NSAnimation<NSAnimationDelegate> {
 @private
  NSApplication* application_;
}
- (id)initWithApplication:(NSApplication*)app
        animationDuration:(NSTimeInterval)duration;
@end


@implementation FadeAllWindowsAnimation

- (id)initWithApplication:(NSApplication*)app
        animationDuration:(NSTimeInterval)duration {
  if ((self = [super initWithDuration:duration
                       animationCurve:NSAnimationLinear])) {
    application_ = app;
    [self setDelegate:self];
  }
  return self;
}

- (void)setCurrentProgress:(NSAnimationProgress)progress {
  for (NSWindow* window in [application_ windows]) {
    [window setAlphaValue:1.0 - progress];
  }
}

- (void)animationDidStop:(NSAnimation*)anim {
  DCHECK_EQ(self, anim);
  [self autorelease];
}

@end

// Private Interface ///////////////////////////////////////////////////////////

@interface ConfirmQuitPanelController (Private)
- (void)animateFadeOut;
- (NSEvent*)pumpEventQueueForKeyUp:(NSApplication*)app untilDate:(NSDate*)date;
- (void)hideAllWindowsForApplication:(NSApplication*)app
                        withDuration:(NSTimeInterval)duration;
@end

ConfirmQuitPanelController* g_confirmQuitPanelController = nil;

////////////////////////////////////////////////////////////////////////////////

@implementation ConfirmQuitPanelController

+ (ConfirmQuitPanelController*)sharedController {
  if (!g_confirmQuitPanelController) {
    g_confirmQuitPanelController =
        [[ConfirmQuitPanelController alloc] init];
  }
  return [[g_confirmQuitPanelController retain] autorelease];
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
        base::SysNSStringToUTF16([[self class] keyCommandString]));
    [contentView_ setMessageText:message];
  }
  return self;
}

+ (BOOL)eventTriggersFeature:(NSEvent*)event {
  if ([event type] != NSKeyDown)
    return NO;
  ui::AcceleratorCocoa eventAccelerator([event charactersIgnoringModifiers],
      [event modifierFlags] & NSDeviceIndependentModifierFlagsMask);
  return [self quitAccelerator] == eventAccelerator;
}

- (NSApplicationTerminateReply)runModalLoopForApplication:(NSApplication*)app {
  scoped_nsobject<ConfirmQuitPanelController> keepAlive([self retain]);

  // If this is the second of two such attempts to quit within a certain time
  // interval, then just quit.
  // Time of last quit attempt, if any.
  static NSDate* lastQuitAttempt;  // Initially nil, as it's static.
  NSDate* timeNow = [NSDate date];
  if (lastQuitAttempt &&
      [timeNow timeIntervalSinceDate:lastQuitAttempt] < kTimeDeltaFuzzFactor) {
    // The panel tells users to Hold Cmd+Q. However, we also want to have a
    // double-tap shortcut that allows for a quick quit path. For the users who
    // tap Cmd+Q and then hold it with the window still open, this double-tap
    // logic will run and cause the quit to get committed. If the key
    // combination held down, the system will start sending the Cmd+Q event to
    // the next key application, and so on. This is bad, so instead we hide all
    // the windows (without animation) to look like we've "quit" and then wait
    // for the KeyUp event to commit the quit.
    [self hideAllWindowsForApplication:app withDuration:0];
    NSEvent* nextEvent = [self pumpEventQueueForKeyUp:app
                                            untilDate:[NSDate distantFuture]];
    [app discardEventsMatchingMask:NSAnyEventMask beforeEvent:nextEvent];

    // Based on how long the user held the keys, record the metric.
    if ([[NSDate date] timeIntervalSinceDate:timeNow] < kDoubleTapTimeDelta)
      confirm_quit::RecordHistogram(confirm_quit::kDoubleTap);
    else
      confirm_quit::RecordHistogram(confirm_quit::kTapHold);
    return NSTerminateNow;
  } else {
    [lastQuitAttempt release];  // Harmless if already nil.
    lastQuitAttempt = [timeNow retain];  // Record this attempt for next time.
  }

  // Show the info panel that explains what the user must to do confirm quit.
  [self showWindow:self];

  // Spin a nested run loop until the |targetDate| is reached or a KeyUp event
  // is sent.
  NSDate* targetDate = [NSDate dateWithTimeIntervalSinceNow:kTimeToConfirmQuit];
  BOOL willQuit = NO;
  NSEvent* nextEvent = nil;
  do {
    // Dequeue events until a key up is received. To avoid busy waiting, figure
    // out the amount of time that the thread can sleep before taking further
    // action.
    NSDate* waitDate = [NSDate dateWithTimeIntervalSinceNow:
        kTimeToConfirmQuit - kTimeDeltaFuzzFactor];
    nextEvent = [self pumpEventQueueForKeyUp:app untilDate:waitDate];

    // Wait for the time expiry to happen. Once past the hold threshold,
    // commit to quitting and hide all the open windows.
    if (!willQuit) {
      NSDate* now = [NSDate date];
      NSTimeInterval difference = [targetDate timeIntervalSinceDate:now];
      if (difference < kTimeDeltaFuzzFactor) {
        willQuit = YES;

        // At this point, the quit has been confirmed and windows should all
        // fade out to convince the user to release the key combo to finalize
        // the quit.
        [self hideAllWindowsForApplication:app
                              withDuration:kWindowFadeAnimationDuration];
      }
    }
  } while (!nextEvent);

  // The user has released the key combo. Discard any events (i.e. the
  // repeated KeyDown Cmd+Q).
  [app discardEventsMatchingMask:NSAnyEventMask beforeEvent:nextEvent];

  if (willQuit) {
    // The user held down the combination long enough that quitting should
    // happen.
    confirm_quit::RecordHistogram(confirm_quit::kHoldDuration);
    return NSTerminateNow;
  } else {
    // Slowly fade the confirm window out in case the user doesn't
    // understand what they have to do to quit.
    [self dismissPanel];
    return NSTerminateCancel;
  }

  // Default case: terminate.
  return NSTerminateNow;
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
  scoped_nsobject<ConfirmQuitPanelController> keepAlive([self retain]);
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
+ (NSString*)keyCommandString {
  ui::AcceleratorCocoa accelerator = [[self class] quitAccelerator];
  return [[self class] keyCombinationForAccelerator:accelerator];
}

// Runs a nested loop that pumps the event queue until the next KeyUp event.
- (NSEvent*)pumpEventQueueForKeyUp:(NSApplication*)app untilDate:(NSDate*)date {
  return [app nextEventMatchingMask:NSKeyUpMask
                          untilDate:date
                             inMode:NSEventTrackingRunLoopMode
                            dequeue:YES];
}

// Iterates through the list of open windows and hides them all.
- (void)hideAllWindowsForApplication:(NSApplication*)app
                        withDuration:(NSTimeInterval)duration {
  FadeAllWindowsAnimation* animation =
      [[FadeAllWindowsAnimation alloc] initWithApplication:app
                                         animationDuration:duration];
  // Releases itself when the animation stops.
  [animation startAnimation];
}

+ (NSString*)keyCombinationForAccelerator:(const ui::AcceleratorCocoa&)item {
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
