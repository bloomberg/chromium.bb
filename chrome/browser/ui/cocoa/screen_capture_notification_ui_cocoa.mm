// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "chrome/browser/ui/cocoa/screen_capture_notification_ui_cocoa.h"

#include "base/compiler_specific.h"
#include "base/i18n/rtl.h"
#include "base/mac/bundle_locations.h"
#include "base/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

@interface ScreenCaptureNotificationController()
- (void)Hide;
@end

class ScreenCaptureNotificationUICocoa : public ScreenCaptureNotificationUI {
 public:
  ScreenCaptureNotificationUICocoa();
  virtual ~ScreenCaptureNotificationUICocoa();

  // ScreenCaptureNotificationUI interface.
  virtual bool Show(const base::Closure& stop_callback,
                    const string16& title) OVERRIDE;

 private:
  ScreenCaptureNotificationController* window_controller_;

  DISALLOW_COPY_AND_ASSIGN(ScreenCaptureNotificationUICocoa);
};

ScreenCaptureNotificationUICocoa::ScreenCaptureNotificationUICocoa()
    : window_controller_(nil) {
}

ScreenCaptureNotificationUICocoa::~ScreenCaptureNotificationUICocoa() {
  // ScreenCaptureNotificationController is responsible for releasing itself in
  // its windowWillClose: method.
  [window_controller_ Hide];
  window_controller_ = nil;
}

bool ScreenCaptureNotificationUICocoa::Show(const base::Closure& stop_callback,
                                            const string16& title) {
  DCHECK(!stop_callback.is_null());
  DCHECK(window_controller_ == nil);

  window_controller_ =
      [[ScreenCaptureNotificationController alloc]
          initWithCallback:stop_callback
                     title:title];
  [window_controller_ showWindow:nil];
  return true;
}

scoped_ptr<ScreenCaptureNotificationUI> ScreenCaptureNotificationUI::Create() {
  return scoped_ptr<ScreenCaptureNotificationUI>(
      new ScreenCaptureNotificationUICocoa());
}

@implementation ScreenCaptureNotificationController
- (id)initWithCallback:(const base::Closure&)stop_callback
                 title:(const string16&)title {
  NSString* nibpath =
      [base::mac::FrameworkBundle() pathForResource:@"ScreenCaptureNotification"
                                             ofType:@"nib"];
  self = [super initWithWindowNibPath:nibpath owner:self];
  if (self) {
    stop_callback_ = stop_callback;
    title_ = title;
  }
  return self;
}

- (void)dealloc {
  [super dealloc];
}

- (IBAction)stopSharing:(id)sender {
  if (!stop_callback_.is_null()) {
    stop_callback_.Run();
  }
}

- (void)Hide {
  stop_callback_.Reset();
  [self close];
}

- (void)windowDidLoad {
  string16 text = l10n_util::GetStringFUTF16(
      IDS_MEDIA_SCREEN_CAPTURE_NOTIFICATION_TEXT, title_);
  [statusField_ setStringValue:base::SysUTF16ToNSString(text)];

  string16 button_label =
      l10n_util::GetStringUTF16(IDS_MEDIA_SCREEN_CAPTURE_NOTIFICATION_STOP);
  [stopButton_ setTitle:base::SysUTF16ToNSString(button_label)];

  // Resize the window dynamically based on the content.
  CGFloat oldConnectedWidth = NSWidth([statusField_ bounds]);
  [statusField_ sizeToFit];
  NSRect statusFrame = [statusField_ frame];
  CGFloat newConnectedWidth = NSWidth(statusFrame);

  // Set a max width for the connected to text field.
  const int kMaximumStatusWidth = 400;
  if (newConnectedWidth > kMaximumStatusWidth) {
    newConnectedWidth = kMaximumStatusWidth;
    statusFrame.size.width = newConnectedWidth;
    [statusField_ setFrame:statusFrame];
  }

  CGFloat oldstopWidth = NSWidth([stopButton_ bounds]);
  [stopButton_ sizeToFit];
  NSRect stopFrame = [stopButton_ frame];
  CGFloat newStopWidth = NSWidth(stopFrame);

  // Move the stop button appropriately.
  stopFrame.origin.x += newConnectedWidth - oldConnectedWidth;
  [stopButton_ setFrame:stopFrame];

  // Then resize the window appropriately.
  NSWindow *window = [self window];
  NSRect windowFrame = [window frame];
  windowFrame.size.width += (newConnectedWidth - oldConnectedWidth +
                             newStopWidth - oldstopWidth);
  [window setFrame:windowFrame display:NO];

  if (base::i18n::IsRTL()) {
    // Handle right to left case
    CGFloat buttonInset = NSWidth(windowFrame) - NSMaxX(stopFrame);
    CGFloat buttonTextSpacing
        = NSMinX(stopFrame) - NSMaxX(statusFrame);
    stopFrame.origin.x = buttonInset;
    statusFrame.origin.x = NSMaxX(stopFrame) + buttonTextSpacing;
    [statusField_ setFrame:statusFrame];
    [stopButton_ setFrame:stopFrame];
  }

  // Center the window at the bottom of the screen, above the dock (if present).
  NSRect desktopRect = [[NSScreen mainScreen] visibleFrame];
  NSRect windowRect = [[self window] frame];
  CGFloat x = (NSWidth(desktopRect) - NSWidth(windowRect)) / 2;
  CGFloat y = NSMinY(desktopRect);
  [[self window] setFrameOrigin:NSMakePoint(x, y)];
}

- (void)windowWillClose:(NSNotification*)notification {
  [self stopSharing:self];
  [self autorelease];
}

@end


@implementation ScreenCaptureNotificationWindow
- (id)initWithContentRect:(NSRect)contentRect
                styleMask:(NSUInteger)aStyle
                  backing:(NSBackingStoreType)bufferingType
                    defer:(BOOL)flag {
  // Pass NSBorderlessWindowMask for the styleMask to remove the title bar.
  self = [super initWithContentRect:contentRect
                          styleMask:NSBorderlessWindowMask
                            backing:bufferingType
                              defer:flag];

  if (self) {
    // Set window to be clear and non-opaque so we can see through it.
    [self setBackgroundColor:[NSColor clearColor]];
    [self setOpaque:NO];
    [self setMovableByWindowBackground:YES];

    // Pull the window up to Status Level so that it always displays.
    [self setLevel:NSStatusWindowLevel];
  }
  return self;
}
@end

@implementation ScreenCaptureNotificationView
- (void)drawRect:(NSRect)rect {
  // All magic numbers taken from screen shots provided by UX.
  NSRect bounds = NSInsetRect([self bounds], 1, 1);

  NSBezierPath *path = [NSBezierPath bezierPathWithRoundedRect:bounds
                                                       xRadius:5
                                                       yRadius:5];
  NSColor *gray = [NSColor colorWithCalibratedWhite:0.91 alpha:1.0];
  [gray setFill];
  [path fill];
  [path setLineWidth:4];
  NSColor *green = [NSColor colorWithCalibratedRed:0.13
                                             green:0.69
                                              blue:0.11
                                             alpha:1.0];
  [green setStroke];
  [path stroke];


  // Draw drag handle on proper side
  const CGFloat kHeight = 21.0;
  const CGFloat kBaseInset = 12.0;
  const CGFloat kDragHandleWidth = 5.0;

  NSColor *dark = [NSColor colorWithCalibratedWhite:0.70 alpha:1.0];
  NSColor *light = [NSColor colorWithCalibratedWhite:0.97 alpha:1.0];

  // Turn off aliasing so it's nice and crisp.
  NSGraphicsContext *context = [NSGraphicsContext currentContext];
  BOOL alias = [context shouldAntialias];
  [context setShouldAntialias:NO];

  // Handle bidirectional locales properly.
  CGFloat inset = base::i18n::IsRTL() ?
      NSMaxX(bounds) - kBaseInset - kDragHandleWidth : kBaseInset;

  NSPoint top = NSMakePoint(inset, NSMidY(bounds) - kHeight / 2.0);
  NSPoint bottom = NSMakePoint(inset, top.y + kHeight);

  path = [NSBezierPath bezierPath];
  [path moveToPoint:top];
  [path lineToPoint:bottom];
  [dark setStroke];
  [path stroke];

  top.x += 1;
  bottom.x += 1;
  path = [NSBezierPath bezierPath];
  [path moveToPoint:top];
  [path lineToPoint:bottom];
  [light setStroke];
  [path stroke];

  top.x += 2;
  bottom.x += 2;
  path = [NSBezierPath bezierPath];
  [path moveToPoint:top];
  [path lineToPoint:bottom];
  [dark setStroke];
  [path stroke];

  top.x += 1;
  bottom.x += 1;
  path = [NSBezierPath bezierPath];
  [path moveToPoint:top];
  [path lineToPoint:bottom];
  [light setStroke];
  [path stroke];

  [context setShouldAntialias:alias];
}

@end
