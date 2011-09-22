// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/panels/panel_titlebar_view_cocoa.h"

#include <Carbon/Carbon.h>  // kVK_Escape
#import <Cocoa/Cocoa.h>

#include "base/logging.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "chrome/browser/themes/theme_service.h"
#import "chrome/browser/ui/cocoa/hover_image_button.h"
#import "chrome/browser/ui/cocoa/nsview_additions.h"
#import "chrome/browser/ui/cocoa/themed_window.h"
#import "chrome/browser/ui/cocoa/tracking_area.h"
#import "chrome/browser/ui/panels/panel_window_controller_cocoa.h"
#include "grit/theme_resources_standard.h"
#import "third_party/GTM/AppKit/GTMNSBezierPath+RoundRect.h"
#include "ui/gfx/mac/nsimage_cache.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"

const int kRoundedCornerSize = 3;
const int kButtonPadding = 8;
const int kIconAndTextPadding = 5;

// Used to implement TestingAPI
static NSEvent* MakeMouseEvent(NSEventType type,
                               NSPoint point,
                               int clickCount) {
  return [NSEvent mouseEventWithType:type
                            location:point
                       modifierFlags:0
                           timestamp:0
                        windowNumber:0
                             context:nil
                         eventNumber:0
                          clickCount:clickCount
                            pressure:0.0];
}

@implementation PanelTitlebarViewCocoa

- (id)initWithFrame:(NSRect)frame {
  if ((self = [super initWithFrame:frame])) {
    dragState_ = PANEL_DRAG_SUPPRESSED;
    // Create standard OSX Close Button.
    closeButton_ = [NSWindow standardWindowButton:NSWindowCloseButton
                                     forStyleMask:NSClosableWindowMask];
    [closeButton_ setTarget:self];
    [closeButton_ setAction:@selector(onCloseButtonClick:)];
    [self addSubview:closeButton_];
  }
  return self;
}

- (void)dealloc {
  if (closeButtonTrackingArea_.get())
    [self removeTrackingArea:closeButtonTrackingArea_.get()];
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (void)onCloseButtonClick:(id)sender {
  [controller_ closePanel];
}

- (void)onSettingsButtonClick:(id)sender {
  [controller_ runSettingsMenu:settingsButton_];
}

- (void)drawRect:(NSRect)rect {
  NSBezierPath* path =
      [NSBezierPath gtm_bezierPathWithRoundRect:[self bounds]
                            topLeftCornerRadius:kRoundedCornerSize
                           topRightCornerRadius:kRoundedCornerSize
                         bottomLeftCornerRadius:0.0
                        bottomRightCornerRadius:0.0];
  [path addClip];

  NSPoint phase = [[self window] themePatternPhase];
  [[NSGraphicsContext currentContext] setPatternPhase:phase];

  ThemeService* theme =
      static_cast<ThemeService*>([[self window] themeProvider]);

  NSColor* backgroundColor = nil;
  // When using the default theme, we fall back to the default system colors
  // instead of trying to use the tab coloring, since using the tab coloring
  // results in focused panel titlebars that are light and unfocused panel
  // titlebars that are dark, which is the opposite of all other window
  // titlebars on Mac.
  if (theme && !theme->UsingDefaultTheme()) {
      if ([[self window] isMainWindow]) {
        backgroundColor = theme->GetNSImageColorNamed(IDR_THEME_TOOLBAR, true);
      } else {
        // Based on -[TabView drawRect:], we need to make sure the theme has an
        // IDR_THEME_TAB_BACKGROUND or IDR_THEME_FRAME resource; otherwise,
        // we'll potentially end up with a bizarre looking blue background for
        // inactive tabs, which looks really out of place on a Mac.
        BOOL hasBackgroundImage =
            (theme->HasCustomImage(IDR_THEME_TAB_BACKGROUND) ||
             theme->HasCustomImage(IDR_THEME_FRAME));
        if (hasBackgroundImage) {
          backgroundColor =
              theme->GetNSImageColorNamed(IDR_THEME_TAB_BACKGROUND, true);
        }
      }
  }

  if (backgroundColor) {
    [backgroundColor set];
    NSRectFillUsingOperation([self bounds], NSCompositeSourceOver);
  } else {
    // Temporarily reset the pattern phase to (0, 0). We want to anchor the
    // titlebar gradient to match the default system look.
    gfx::ScopedNSGraphicsContextSaveGState scopedGState;
    [[NSGraphicsContext currentContext] setPatternPhase:NSMakePoint(0, 0)];
    [[[self window] backgroundColor] set];
    NSRectFillUsingOperation([self bounds], NSCompositeSourceOver);
  }


  // Draw the divider stroke.
  NSColor* strokeColor = nil;
  if (theme) {
    strokeColor = [[self window] isMainWindow]
        ? theme->GetNSColor(ThemeService::COLOR_TOOLBAR_STROKE, true)
        : theme->GetNSColor(ThemeService::COLOR_TOOLBAR_STROKE_INACTIVE, true);
  } else {
    strokeColor = [NSColor blackColor];
  }
  NSRect borderRect, contentRect;
  NSDivideRect([self bounds], &borderRect, &contentRect, [self cr_lineWidth],
               NSMinYEdge);
  [strokeColor set];
  NSRectFillUsingOperation(borderRect, NSCompositeSourceOver);

  // Update the panel title text color as appropriate.
  NSColor* titleColor = nil;
  if (theme)
    titleColor = [[self window] isMainWindow]
        ? theme->GetNSColor(ThemeService::COLOR_TAB_TEXT, true)
        : theme->GetNSColor(ThemeService::COLOR_BACKGROUND_TAB_TEXT, true);
  else
    titleColor = [NSColor textColor];
  [title_ setTextColor:titleColor];
}

- (void)attach {
  // Interface Builder can not put a view as a sibling of contentView,
  // so need to do it here. Placing ourself as the last child of the
  // internal view allows us to draw on top of the titlebar.
  // Note we must use [controller_ window] here since we have not been added
  // to the view hierarchy yet.
  [[[[controller_ window] contentView] superview] addSubview:self];

  // Figure out the rectangle of the titlebar and set us on top of it.
  // The titlebar covers window's root view where not covered by contentView.
  // Compute the titlebar frame in coordinate system of the window's root view.
  //        NSWindow
  //           |
  //    ___root_view____
  //     |            |
  // contentView  titlebar
  NSRect rootViewBounds = [[self superview] bounds];
  NSRect contentFrame = [[[self window] contentView] frame];
  NSRect titlebarFrame =
      NSMakeRect(NSMinX(contentFrame),
                 NSMaxY(contentFrame),
                 NSWidth(contentFrame),
                 NSMaxY(rootViewBounds) - NSMaxY(contentFrame));
  [self setFrame:titlebarFrame];

  // Initialize the settings button.
  NSImage* image = gfx::GetCachedImageWithName(@"balloon_wrench.pdf");
  [settingsButton_ setDefaultImage:image];
  [settingsButton_ setDefaultOpacity:0.6];
  [settingsButton_ setHoverImage:image];
  [settingsButton_ setHoverOpacity:0.9];
  [settingsButton_ setPressedImage:image];
  [settingsButton_ setPressedOpacity:1.0];
  [[settingsButton_ cell] setHighlightsBy:NSNoCellMask];
  [self checkMouseAndUpdateSettingsButtonVisibility];

  // Update layout of controls in the titlebar.
  [self updateCloseButtonLayout];
  [self updateIconAndTitleLayout];

  // Set autoresizing behavior: glued to edges on left, top and right.
  [self setAutoresizingMask:(NSViewMinYMargin | NSViewWidthSizable)];

  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(didChangeTheme:)
             name:kBrowserThemeDidChangeNotification
           object:nil];
  // Register for various window focus changes, so we can update our custom
  // titlebar appropriately.
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(didChangeMainWindow:)
             name:NSWindowDidBecomeMainNotification
           object:[self window]];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(didChangeMainWindow:)
             name:NSWindowDidResignMainNotification
           object:[self window]];
}

- (void)setTitle:(NSString*)newTitle {
  [title_ setStringValue:newTitle];
}

- (void)setIcon:(NSImage*)newIcon {
  [icon_ setImage:newIcon];
}

- (void)updateCloseButtonLayout {
  NSRect buttonFrame = [closeButton_ frame];
  NSRect bounds = [self bounds];

  buttonFrame.origin.x = kButtonPadding;
  buttonFrame.origin.y = (NSHeight(bounds) - NSHeight(buttonFrame)) / 2;
  [closeButton_ setFrame:buttonFrame];

  DCHECK(!closeButtonTrackingArea_.get());
  closeButtonTrackingArea_.reset(
      [[CrTrackingArea alloc] initWithRect:buttonFrame
                                   options:(NSTrackingMouseEnteredAndExited |
                                            NSTrackingActiveAlways)
                              proxiedOwner:self
                                  userInfo:nil]);
  NSWindow* panelWindow = [self window];
  [closeButtonTrackingArea_.get() clearOwnerWhenWindowWillClose:panelWindow];
  [self addTrackingArea:closeButtonTrackingArea_.get()];
}

- (void)updateIconAndTitleLayout {
  NSRect closeButtonFrame = [closeButton_ frame];
  NSRect iconFrame = [icon_ frame];
  [title_ sizeToFit];
  NSRect titleFrame = [title_ frame];
  NSRect settingsButtonFrame = [settingsButton_ frame];
  NSRect bounds = [self bounds];

  // Place the icon and title at the center of the titlebar.
  int iconWidthWithPadding = NSWidth(iconFrame) + kIconAndTextPadding;
  int titleWidth = NSWidth(titleFrame);
  int availableWidth = NSWidth(bounds) - kButtonPadding * 4 -
      NSWidth(closeButtonFrame) - NSWidth(settingsButtonFrame);
  if (iconWidthWithPadding + titleWidth > availableWidth)
    titleWidth = availableWidth - iconWidthWithPadding;
  int startX = kButtonPadding * 2 + NSWidth(closeButtonFrame) +
      (availableWidth - iconWidthWithPadding - titleWidth) / 2;

  iconFrame.origin.x = startX;
  iconFrame.origin.y = (NSHeight(bounds) - NSHeight(iconFrame)) / 2;
  [icon_ setFrame:iconFrame];

  titleFrame.origin.x = startX + iconWidthWithPadding;
  titleFrame.origin.y = (NSHeight(bounds) - NSHeight(titleFrame)) / 2;
  titleFrame.size.width = titleWidth;
  [title_ setFrame:titleFrame];
}

// PanelManager controls size/position of the window.
- (BOOL)mouseDownCanMoveWindow {
  return NO;
}

- (BOOL)acceptsFirstMouse:(NSEvent*)event {
  return YES;
}

- (void)mouseEntered:(NSEvent*)event {
  [[closeButton_ cell] setHighlighted:YES];
}

- (void)mouseExited:(NSEvent*)event {
  [[closeButton_ cell] setHighlighted:NO];
}

- (void)didChangeTheme:(NSNotification*)notification {
  [self setNeedsDisplay:YES];
}

- (void)didChangeMainWindow:(NSNotification*)notification {
  [self setNeedsDisplay:YES];
  [self checkMouseAndUpdateSettingsButtonVisibility];
}

- (void)mouseDown:(NSEvent*)event {
  dragState_ = PANEL_DRAG_CAN_START;
}

- (void)mouseUp:(NSEvent*)event {
  DCHECK(dragState_ != PANEL_DRAG_IN_PROGRESS);

  if ([event clickCount] == 1)
    [controller_ flipExpansionState];
}

- (void)mouseDragged:(NSEvent*)event {
  // In addition to events needed to control the drag operation, fetch the right
  // mouse click events and key down events and ignore them, to prevent their
  // accumulation in the queue and "playing out" when the mouse is released.
  const NSUInteger mask =
      NSLeftMouseUpMask | NSLeftMouseDraggedMask | NSKeyUpMask |
      NSRightMouseDownMask | NSKeyDownMask ;
  BOOL keepGoing = YES;

  while (keepGoing) {
    base::mac::ScopedNSAutoreleasePool autorelease_pool;

    NSEvent* event = [NSApp nextEventMatchingMask:mask
                                        untilDate:[NSDate distantFuture]
                                           inMode:NSDefaultRunLoopMode
                                          dequeue:YES];

    switch ([event type]) {
      case NSLeftMouseDragged:
        if (dragState_ == PANEL_DRAG_CAN_START)
          [self startDrag];
        [self dragWithDeltaX:[event deltaX]];
        break;

      case NSKeyUp:
        if ([event keyCode] == kVK_Escape) {
          [self endDrag:YES];
          keepGoing = NO;
        }
        break;

      case NSLeftMouseUp:
        [self endDrag:NO];
        keepGoing = NO;
        break;

      case NSRightMouseDownMask:
        break;

      default:
        // Dequeue and ignore other mouse and key events so the Chrome context
        // menu does not come after right click on a page during Panel
        // rearrangement, or the keystrokes are not 'accumulated' and entered
        // at once when the drag ends.
        break;
    }
  }
}

- (void)startDrag {
  DCHECK(dragState_ == PANEL_DRAG_CAN_START);
  dragState_ = PANEL_DRAG_IN_PROGRESS;
  [controller_ startDrag];
}

- (void)endDrag:(BOOL)cancelled {
  if (dragState_ == PANEL_DRAG_IN_PROGRESS)
    [controller_ endDrag:cancelled];
  dragState_ = PANEL_DRAG_SUPPRESSED;
}

- (void)dragWithDeltaX:(int)deltaX {
  if (dragState_ != PANEL_DRAG_IN_PROGRESS)
    return;
  [controller_ dragWithDeltaX:deltaX];
}

// (Private/TestingAPI)
- (PanelWindowControllerCocoa*)controller {
  return controller_;
}

- (void)simulateCloseButtonClick {
  [[closeButton_ cell] performClick:closeButton_];
}

- (void)pressLeftMouseButtonTitlebar {
  NSEvent* event = MakeMouseEvent(NSLeftMouseDown, NSZeroPoint, 0);
  [self mouseDown:event];
}

- (void)releaseLeftMouseButtonTitlebar {
  NSEvent* event = MakeMouseEvent(NSLeftMouseUp, NSZeroPoint, 1);
  [self mouseUp:event];
}

- (void)dragTitlebarDeltaX:(double)delta_x
                    deltaY:(double)delta_y {
  if (dragState_ == PANEL_DRAG_CAN_START)
    [self startDrag];
  [self dragWithDeltaX:delta_x];
}

- (void)cancelDragTitlebar {
  [self endDrag:YES];
}

- (void)finishDragTitlebar {
  [self endDrag:NO];
}

- (void)updateSettingsButtonVisibility:(BOOL)mouseOverWindow {
  // The settings button is visible if the panel is main window or the mouse is
  // over it.
  BOOL isMainWindow = [[self window] isMainWindow];
  [settingsButton_ setHidden:!(isMainWindow || mouseOverWindow)];
}

- (void)checkMouseAndUpdateSettingsButtonVisibility {
  BOOL mouseOverWindow = NSPointInRect([NSEvent mouseLocation],
                                       [[controller_ window] frame]);
  [self updateSettingsButtonVisibility:mouseOverWindow];
}

@end

