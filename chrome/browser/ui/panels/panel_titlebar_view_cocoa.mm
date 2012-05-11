// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "grit/generated_resources.h"
#include "grit/theme_resources_standard.h"
#include "grit/theme_resources.h"
#import "third_party/GTM/AppKit/GTMNSBezierPath+RoundRect.h"
#import "third_party/GTM/AppKit/GTMNSColor+Luminance.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/mac/nsimage_cache.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"
#include "ui/gfx/image/image.h"

const int kButtonPadding = 8;
const int kIconAndTextPadding = 5;

// Distance that user needs to move the mouse in order to start the drag.
// Threshold is needed to differentiate drags from attempts to click the
// titlebar with a twitch of the mouse pointer.
const int kDragThreshold = 3;

// 'Glint' is a speck of light that moves across the titlebar to attract a bit
// more attention using movement in addition to color of the titlebar.
// It initially moves fast, then starts to slow down to avoid being annoying
// if the user chooses not to react on it.
const double kGlintAnimationDuration = 0.6;
const double kStartGlintRepeatIntervalSeconds = 0.1;
const double kFinalGlintRepeatIntervalSeconds = 2.0;
const double kGlintRepeatIntervalIncreaseFactor = 1.5;

// Used to implement TestingAPI
static NSEvent* MakeMouseEvent(NSEventType type,
                               NSPoint point,
                               int modifierFlags,
                               int clickCount) {
  return [NSEvent mouseEventWithType:type
                            location:point
                       modifierFlags:modifierFlags
                           timestamp:0
                        windowNumber:0
                             context:nil
                         eventNumber:0
                          clickCount:clickCount
                            pressure:0.0];
}

@implementation PanelTitlebarOverlayView
// Sometimes we do not want to bring chrome window to foreground when we click
// on any part of the titlebar. To do this, we first postpone the window
// reorder here (shouldDelayWindowOrderingForEvent is called during when mouse
// button is pressed but before mouseDown: is dispatched) and then complete
// canceling the reorder by [NSApp preventWindowOrdering] in mouseDown handler
// of this view.
- (BOOL)shouldDelayWindowOrderingForEvent:(NSEvent*)theEvent {
  disableReordering_ = ![controller_ canBecomeKeyWindow];
  return disableReordering_;
}

- (void)mouseDown:(NSEvent*)event {
  if (disableReordering_)
    [NSApp preventWindowOrdering];
  disableReordering_ = NO;
  // Continue bubbling the event up the chain of responders.
  [super mouseDown:event];
}

- (BOOL)acceptsFirstMouse:(NSEvent*)event {
  return YES;
}
@end

@implementation RepaintAnimation
- (id)initWithView:(NSView*)targetView duration:(double) duration {
  if ((self = [super initWithDuration:duration
                       animationCurve:NSAnimationEaseInOut])) {
    [self setAnimationBlockingMode:NSAnimationNonblocking];
    targetView_ = targetView;
  }
  return self;
}

- (void)setCurrentProgress:(NSAnimationProgress)progress {
  [super setCurrentProgress:progress];
  [targetView_ setNeedsDisplay:YES];
}
@end


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
  [self stopGlintAnimation];
  [super dealloc];
}

- (void)onCloseButtonClick:(id)sender {
  [controller_ closePanel];
}

- (void)onMinimizeButtonClick:(id)sender {
  [controller_ minimizeButtonClicked:[[NSApp currentEvent] modifierFlags]];
}

- (void)onRestoreButtonClick:(id)sender {
  [controller_ restoreButtonClicked:[[NSApp currentEvent] modifierFlags]];
}

- (void)drawRect:(NSRect)rect {
  ThemeService* theme =
      static_cast<ThemeService*>([[self window] themeProvider]);

  [title_ setAlphaValue:1.0];
  [icon_ setAlphaValue:1.0];

  NSColor* strokeColor = nil;
  NSColor* titleColor = nil;

  if (isDrawingAttention_) {
    // Use system highlight color for DrawAttention state.
    NSColor* attentionColor = [NSColor selectedTextBackgroundColor];
    [attentionColor set];
    NSRectFillUsingOperation([self bounds], NSCompositeSourceOver);
    // Cover it with semitransparent gradient for better look.
    NSColor* startColor = [NSColor colorWithCalibratedWhite:1.0 alpha:0.2];
    NSColor* endColor = [NSColor colorWithCalibratedWhite:0.0 alpha:0.2];
    scoped_nsobject<NSGradient> gradient(
      [[NSGradient alloc] initWithStartingColor:startColor
                                    endingColor:endColor]);
    [gradient drawInRect:[self bounds] angle:270.0];

    strokeColor = [NSColor colorWithCalibratedWhite:0.6 alpha:1.0];
    titleColor = [attentionColor gtm_legibleTextColor];

    if ([glintAnimation_ isAnimating]) {
      scoped_nsobject<NSGradient> glint([NSGradient alloc]);
      NSColor* gold =
          [NSColor colorWithCalibratedRed:0.8 green:0.8 blue:0.0 alpha:1.0];
      [glint initWithColorsAndLocations:gold, 0.0,
           [NSColor colorWithCalibratedWhite:1.0 alpha:0.4], 0.3,
           [NSColor colorWithCalibratedWhite:1.0 alpha:0.0], 1.0,
           nil];
      NSRect bounds = [self bounds];
      NSPoint point = bounds.origin;
      // The size and position values are experimentally choosen to create
      // a "speck of light attached to the top edge" effect.
      int gradientRadius = NSHeight(bounds) * 2;
      point.y += gradientRadius / 2;
      double rangeOfMotion = NSWidth(bounds) + 4 * gradientRadius;
      double startPoint = - 2 * gradientRadius;
      point.x = startPoint + rangeOfMotion * [glintAnimation_ currentValue];
      [glint drawFromCenter:point
                     radius:0.0
                   toCenter:point
                     radius:gradientRadius
                    options:NSGradientDrawsBeforeStartingLocation];
    }
  } else if (theme && !theme->UsingDefaultTheme()) {
    NSColor* backgroundColor = nil;
    if ([[self window] isMainWindow]) {
      backgroundColor = theme->GetNSImageColorNamed(IDR_THEME_TOOLBAR, true);
    } else {
      // Based on -[TabView drawRect:], we need to check if the theme has an
      // IDR_THEME_TAB_BACKGROUND or IDR_THEME_FRAME resource; otherwise,
      // we'll potentially end up with a bizarre looking blue background for
      // inactive tabs, which looks really out of place on a Mac.
      if (theme->HasCustomImage(IDR_THEME_TAB_BACKGROUND) ||
          theme->HasCustomImage(IDR_THEME_FRAME)) {
        backgroundColor =
            theme->GetNSImageColorNamed(IDR_THEME_TAB_BACKGROUND, true);
      } else {
        backgroundColor = [[self window] backgroundColor];  // Fallback.
      }
    }
    // Fill with white to avoid bleeding the system titlebar through
    // semitransparent theme images.
    [[NSColor whiteColor] set];
    NSRectFill([self bounds]);

    NSPoint phase = [[self window] themePatternPhase];
    [[NSGraphicsContext currentContext] setPatternPhase:phase];
    DCHECK(backgroundColor);
    [backgroundColor set];
    NSRectFillUsingOperation([self bounds], NSCompositeSourceOver);

    strokeColor = [[self window] isMainWindow]
        ? theme->GetNSColor(ThemeService::COLOR_TOOLBAR_STROKE, true)
        : theme->GetNSColor(ThemeService::COLOR_TOOLBAR_STROKE_INACTIVE, true);

    titleColor = [[self window] isMainWindow]
        ? theme->GetNSColor(ThemeService::COLOR_TAB_TEXT, true)
        : theme->GetNSColor(ThemeService::COLOR_BACKGROUND_TAB_TEXT, true);
  } else {
    // Default theme or no theme.
    BOOL isActive = [[self window] isMainWindow];
    // Use grayish gradient similar to that of regular OSX window.
    double startWhite = isActive ? 0.8 : 1.0;
    double endWhite = isActive ? 0.65 : 0.8;
    NSColor* startColor = [NSColor colorWithCalibratedWhite:startWhite
                                                      alpha:1.0];
    NSColor* endColor = [NSColor colorWithCalibratedWhite:endWhite
                                                    alpha:1.0];
    scoped_nsobject<NSGradient> gradient(
      [[NSGradient alloc] initWithStartingColor:startColor
                                    endingColor:endColor]);
    [gradient drawInRect:[self bounds] angle:270.0];

    strokeColor = [NSColor colorWithCalibratedWhite:0.6 alpha:1.0];
    titleColor = [startColor gtm_legibleTextColor];
    if (!isActive) {
      [title_ setAlphaValue:0.5];
      if (icon_)
        [icon_ setAlphaValue:0.5];
    }
  }

  DCHECK(titleColor);
  [title_ setTextColor:titleColor];

  // Draw the divider stroke.
  DCHECK(strokeColor);
  [strokeColor set];
  NSRect borderRect, contentRect;
  NSDivideRect([self bounds], &borderRect, &contentRect, [self cr_lineWidth],
               NSMinYEdge);
  NSRectFillUsingOperation(borderRect, NSCompositeSourceOver);
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

  [title_ setWantsLayer:YES];
  [title_ setFont:[NSFont boldSystemFontOfSize:[NSFont systemFontSize]]];
  // This draws nice tight shadow, 'sinking' text into the background.
  [[title_ cell] setBackgroundStyle:NSBackgroundStyleRaised];

  [self updateCloseButtonLayout];

  // Iniitalize the minimize and restore buttons.
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  [self initializeMinimizeRestoreButton:minimizeButton_
      image:rb.GetNativeImageNamed(IDR_PANEL_MINIMIZE)
      hoverImage:rb.GetNativeImageNamed(IDR_PANEL_MINIMIZE_H)
      toolTip:l10n_util::GetNSStringWithFixup(IDS_PANEL_MINIMIZE_TOOLTIP)];
  [self initializeMinimizeRestoreButton:restoreButton_
      image: rb.GetNativeImageNamed(IDR_PANEL_RESTORE)
      hoverImage:rb.GetNativeImageNamed(IDR_PANEL_RESTORE_H)
      toolTip:l10n_util::GetNSStringWithFixup(IDS_PANEL_RESTORE_TOOLTIP)];
  [restoreButton_ setHidden:YES];  // Only visible when panel is minimized.
  [self updateMinimizeRestoreButtonLayout];

  // Set autoresizing behavior: glued to edges on left, top and right.
  [self setAutoresizingMask:(NSViewMinYMargin | NSViewWidthSizable)];

  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(didChangeTheme:)
             name:kBrowserThemeDidChangeNotification
           object:nil];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(didChangeFrame:)
             name:NSViewFrameDidChangeNotification
           object:self];
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

- (void)initializeMinimizeRestoreButton:(HoverImageButton*)button
                                  image:(NSImage*)image
                             hoverImage:(NSImage*)hoverImage
                                toolTip:(NSString*)toolTip {
  [button setDefaultImage:image];
  [button setDefaultOpacity:1.0];
  [button setHoverImage:hoverImage];
  [button setHoverOpacity:1.0];
  [button setPressedImage:hoverImage];
  [button setPressedOpacity:1.0];
  [button setToolTip:toolTip];
  [[button cell] setHighlightsBy:NSNoCellMask];
}

- (void)setTitle:(NSString*)newTitle {
  [title_ setStringValue:newTitle];
  [self updateIconAndTitleLayout];
}

- (void)setIcon:(NSView*)newIcon {
  [icon_ removeFromSuperview];
  icon_ = newIcon;
  if (icon_) {
    [self addSubview:icon_ positioned:NSWindowBelow relativeTo:overlay_];
    [icon_ setWantsLayer:YES];
  }
  [self updateIconAndTitleLayout];
}

- (NSView*)icon {
  return icon_;
}

- (void)setMinimizeButtonVisibility:(BOOL)visible {
  [minimizeButton_ setHidden:!visible];
}

- (void)setRestoreButtonVisibility:(BOOL)visible {
  [restoreButton_ setHidden:!visible];
}

- (void)updateMinimizeRestoreButtonLayout {
  NSRect bounds = [self bounds];
  NSRect buttonFrame = [minimizeButton_ frame];
  buttonFrame.origin.x =
      NSWidth(bounds) - NSWidth(buttonFrame) - kButtonPadding;
  buttonFrame.origin.y = (NSHeight(bounds) - NSHeight(buttonFrame)) / 2;
  [minimizeButton_ setFrame:buttonFrame];
  [restoreButton_ setFrame:buttonFrame];
}

- (void)updateCloseButtonLayout {
  NSRect buttonFrame = [closeButton_ frame];
  NSRect bounds = [self bounds];

  buttonFrame.origin.x = kButtonPadding;
  // Lower Close Button's frame 1 px to avoid it 'peeking' in MINIMIZED mode.
  buttonFrame.origin.y = (NSHeight(bounds) - NSHeight(buttonFrame)) / 2 - 1;
  [closeButton_ setFrame:buttonFrame];
  if (!closeButtonTrackingArea_.get()) {
    closeButtonTrackingArea_.reset(
        [[CrTrackingArea alloc] initWithRect:[closeButton_ bounds]
                                     options:(NSTrackingMouseEnteredAndExited |
                                              NSTrackingActiveAlways)
                                proxiedOwner:self
                                    userInfo:nil]);
    NSWindow* panelWindow = [self window];
    [closeButtonTrackingArea_.get() clearOwnerWhenWindowWillClose:panelWindow];
    [closeButton_ addTrackingArea:closeButtonTrackingArea_.get()];
  }
}

- (void)updateIconAndTitleLayout {
  NSRect closeButtonFrame = [closeButton_ frame];
  NSRect iconFrame = [icon_ frame];
  // NSTextField for title_ is set to Layout:Truncate, LineBreaks:TruncateTail
  // in Interface Builder so it is sized in a single-line mode.
  [title_ sizeToFit];
  NSRect titleFrame = [title_ frame];
  // Only one of minimize/restore button is visible at a time so just allow for
  // the width of one of them.
  NSRect minimizeRestoreButtonFrame = [minimizeButton_ frame];
  NSRect bounds = [self bounds];

  // Place the icon and title at the center of the titlebar.
  int iconWidthWithPadding = NSWidth(iconFrame) + kIconAndTextPadding;
  int titleWidth = NSWidth(titleFrame);
  int availableWidth = NSWidth(bounds) - kButtonPadding * 4 -
      NSWidth(closeButtonFrame) - NSWidth(minimizeRestoreButtonFrame);
  if (iconWidthWithPadding + titleWidth > availableWidth)
    titleWidth = availableWidth - iconWidthWithPadding;
  int startX = kButtonPadding * 2 + NSWidth(closeButtonFrame) +
      (availableWidth - iconWidthWithPadding - titleWidth) / 2;

  iconFrame.origin.x = startX;
  iconFrame.origin.y = (NSHeight(bounds) - NSHeight(iconFrame)) / 2;
  [icon_ setFrame:iconFrame];

  titleFrame.origin.x = startX + iconWidthWithPadding;
  // In bottom-heavy text labels, let's compensate for occasional integer
  // rounding to avoid text label to feel too low.
  titleFrame.origin.y = (NSHeight(bounds) - NSHeight(titleFrame)) / 2 + 1;
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

- (void)didChangeFrame:(NSNotification*)notification {
  // Update buttons first because title layout depends on buttons layout.
  [self updateMinimizeRestoreButtonLayout];
  [self updateIconAndTitleLayout];
}

- (void)didChangeTheme:(NSNotification*)notification {
  [self setNeedsDisplay:YES];
}

- (void)didChangeMainWindow:(NSNotification*)notification {
  [self setNeedsDisplay:YES];
}

- (void)mouseDown:(NSEvent*)event {
  dragState_ = PANEL_DRAG_CAN_START;
  dragStartLocation_ =
      [[self window] convertBaseToScreen:[event locationInWindow]];
}

- (void)mouseUp:(NSEvent*)event {
  DCHECK(dragState_ != PANEL_DRAG_IN_PROGRESS);

  if ([event clickCount] == 1)
    [controller_ onTitlebarMouseClicked:[event modifierFlags]];
}

- (BOOL)exceedsDragThreshold:(NSPoint)mouseLocation {
  float deltaX = fabs(dragStartLocation_.x - mouseLocation.x);
  float deltaY = fabs(dragStartLocation_.y - mouseLocation.y);
  return deltaX > kDragThreshold || deltaY > kDragThreshold;
}

- (void)mouseDragged:(NSEvent*)event {
  if (dragState_ == PANEL_DRAG_SUPPRESSED)
    return;

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
      case NSLeftMouseDragged: {
        // Get current mouse location in Cocoa's screen coordinates.
        NSPoint mouseLocation =
            [[self window] convertBaseToScreen:[event locationInWindow]];
        if (dragState_ == PANEL_DRAG_CAN_START) {
          if (![self exceedsDragThreshold:mouseLocation])
            return;  // Don't start real drag yet.
          [self startDrag:dragStartLocation_];
        }
        DCHECK(dragState_ == PANEL_DRAG_IN_PROGRESS);
        [self drag:mouseLocation];
        break;
      }

      case NSKeyUp:
        if ([event keyCode] == kVK_Escape) {
          [self endDrag:YES];
          keepGoing = NO;
        }
        break;

      case NSLeftMouseUp:
        if (dragState_ == PANEL_DRAG_CAN_START)
          [self mouseUp:event];  // Drag didn't really start, minimize instead.
        else
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

- (void)startDrag:(NSPoint)mouseLocation {
  DCHECK(dragState_ == PANEL_DRAG_CAN_START);
  dragState_ = PANEL_DRAG_IN_PROGRESS;
  [controller_ startDrag:mouseLocation];
}

- (void)endDrag:(BOOL)cancelled {
  if (dragState_ == PANEL_DRAG_IN_PROGRESS)
    [controller_ endDrag:cancelled];
  dragState_ = PANEL_DRAG_SUPPRESSED;
  dragStartLocation_ = NSZeroPoint;
}

- (void)drag:(NSPoint)mouseLocation {
  if (dragState_ != PANEL_DRAG_IN_PROGRESS)
    return;
  [controller_ drag:mouseLocation];
}

- (void)drawAttention {
  if (isDrawingAttention_)
    return;
  isDrawingAttention_ = YES;

  [self startGlintAnimation];
}

- (void)stopDrawingAttention {
  if (!isDrawingAttention_)
    return;
  isDrawingAttention_ = NO;

  [self stopGlintAnimation];
  [self setNeedsDisplay:YES];
}

- (BOOL)isDrawingAttention {
  return isDrawingAttention_;
}

- (void)startGlintAnimation {
  glintInterval_ = kStartGlintRepeatIntervalSeconds;
  [self restartGlintAnimation:nil];
}

- (void)stopGlintAnimation {
  if (glintAnimationTimer_.get()) {
    [glintAnimationTimer_ invalidate];
    glintAnimationTimer_.reset();
  }
  if ([glintAnimation_ isAnimating])
    [glintAnimation_ stopAnimation];
}

- (void)restartGlintAnimation:(NSTimer*)timer {
  if (!glintAnimation_.get()) {
    glintAnimation_.reset(
        [[RepaintAnimation alloc] initWithView:self
                                      duration:kGlintAnimationDuration]);
    [glintAnimation_ setDelegate:self];
  }
  [glintAnimation_ startAnimation];
}

- (void)animationDidEnd:(NSAnimation*)animation {
  if (animation == glintAnimation_.get()) {  // Restart after a timeout.
    glintAnimationTimer_.reset([[NSTimer
        scheduledTimerWithTimeInterval:glintInterval_
                                target:self
                              selector:@selector(restartGlintAnimation:)
                              userInfo:nil
                               repeats:NO] retain]);
    // Gradually reduce the frequency of repeating the animation,
    // calming it down if user decides not to act upon it.
    if (glintInterval_ < kFinalGlintRepeatIntervalSeconds)
      glintInterval_ *= kGlintRepeatIntervalIncreaseFactor;
  }
}

// (Private/TestingAPI)
- (PanelWindowControllerCocoa*)controller {
  return controller_;
}

- (NSTextField*)title {
  return title_;
}

- (void)simulateCloseButtonClick {
  [[closeButton_ cell] performClick:closeButton_];
}

- (void)pressLeftMouseButtonTitlebar:(NSPoint)mouseLocation
                           modifiers:(int)modifierFlags {
  // Convert from Cocoa's screen coordinates to base coordinates since the mouse
  // event takes base coordinates.
  NSEvent* event = MakeMouseEvent(
      NSLeftMouseDown, [[self window] convertScreenToBase:mouseLocation],
      modifierFlags, 0);
  [self mouseDown:event];
}

- (void)releaseLeftMouseButtonTitlebar:(int)modifierFlags {
  NSEvent* event = MakeMouseEvent(NSLeftMouseUp, NSZeroPoint, modifierFlags, 1);
  [self mouseUp:event];
}

- (void)dragTitlebar:(NSPoint)mouseLocation {
  if (dragState_ == PANEL_DRAG_CAN_START)
    [self startDrag:dragStartLocation_];
  // No need to do any conversion since |mouseLocation| is already in Cocoa's
  // screen coordinates.
  [self drag:mouseLocation];
}

- (void)cancelDragTitlebar {
  [self endDrag:YES];
}

- (void)finishDragTitlebar {
  [self endDrag:NO];
}

- (NSButton*)closeButton {
  return closeButton_;
}

- (NSButton*)minimizeButton {
  return minimizeButton_;
}

- (NSButton*)restoreButton {
  return restoreButton_;
}

@end

