// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/tabs/tab_view.h"

#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/mac/sdk_forward_declarations.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/themes/theme_service.h"
#import "chrome/browser/ui/cocoa/tabs/tab_controller.h"
#import "chrome/browser/ui/cocoa/tabs/tab_window_controller.h"
#import "chrome/browser/ui/cocoa/themed_window.h"
#import "chrome/browser/ui/cocoa/view_id_util.h"
#include "chrome/grit/generated_resources.h"
#include "grit/theme_resources.h"
#import "third_party/google_toolbox_for_mac/src/AppKit/GTMFadeTruncatingTextFieldCell.h"
#import "ui/base/cocoa/nsgraphics_context_additions.h"
#import "ui/base/cocoa/nsview_additions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"


const int kMaskHeight = 29;  // Height of the mask bitmap.
const int kFillHeight = 25;  // Height of the "mask on" part of the mask bitmap.

// Constants for inset and control points for tab shape.
const CGFloat kInsetMultiplier = 2.0/3.0;

// The amount of time in seconds during which each type of glow increases, holds
// steady, and decreases, respectively.
const NSTimeInterval kHoverShowDuration = 0.2;
const NSTimeInterval kHoverHoldDuration = 0.02;
const NSTimeInterval kHoverHideDuration = 0.4;
const NSTimeInterval kAlertShowDuration = 0.4;
const NSTimeInterval kAlertHoldDuration = 0.4;
const NSTimeInterval kAlertHideDuration = 0.4;

// The default time interval in seconds between glow updates (when
// increasing/decreasing).
const NSTimeInterval kGlowUpdateInterval = 0.025;

// This is used to judge whether the mouse has moved during rapid closure; if it
// has moved less than the threshold, we want to close the tab.
const CGFloat kRapidCloseDist = 2.5;

@interface TabView(Private)

- (void)resetLastGlowUpdateTime;
- (NSTimeInterval)timeElapsedSinceLastGlowUpdate;
- (void)adjustGlowValue;
- (CGImageRef)tabClippingMask;

@end  // TabView(Private)

@implementation TabView

@synthesize state = state_;
@synthesize hoverAlpha = hoverAlpha_;
@synthesize alertAlpha = alertAlpha_;
@synthesize closing = closing_;

+ (CGFloat)insetMultiplier {
  return kInsetMultiplier;
}

- (id)initWithFrame:(NSRect)frame
         controller:(TabController*)controller
        closeButton:(HoverCloseButton*)closeButton {
  self = [super initWithFrame:frame];
  if (self) {
    controller_ = controller;
    closeButton_ = closeButton;

    // Make a text field for the title, but don't add it as a subview.
    // We will use the cell to draw the text directly into our layer,
    // so that we can get font smoothing enabled.
    titleView_.reset([[NSTextField alloc] init]);
    [titleView_ setAutoresizingMask:NSViewWidthSizable];
    base::scoped_nsobject<GTMFadeTruncatingTextFieldCell> labelCell(
        [[GTMFadeTruncatingTextFieldCell alloc] initTextCell:@"Label"]);
    [labelCell setControlSize:NSSmallControlSize];
    CGFloat fontSize = [NSFont systemFontSizeForControlSize:NSSmallControlSize];
    NSFont* font = [NSFont fontWithName:[[labelCell font] fontName]
                                   size:fontSize];
    [labelCell setFont:font];
    [titleView_ setCell:labelCell];
    titleViewCell_ = labelCell;
  }
  return self;
}

- (void)dealloc {
  // Cancel any delayed requests that may still be pending (drags or hover).
  [NSObject cancelPreviousPerformRequestsWithTarget:self];
  [super dealloc];
}

// Called to obtain the context menu for when the user hits the right mouse
// button (or control-clicks). (Note that -rightMouseDown: is *not* called for
// control-click.)
- (NSMenu*)menu {
  if ([self isClosing])
    return nil;

  // Sheets, being window-modal, should block contextual menus. For some reason
  // they do not. Disallow them ourselves.
  if ([[self window] attachedSheet])
    return nil;

  return [controller_ menu];
}

- (void)resizeSubviewsWithOldSize:(NSSize)oldBoundsSize {
  [super resizeSubviewsWithOldSize:oldBoundsSize];
  // Called when our view is resized. If it gets too small, start by hiding
  // the close button and only show it if tab is selected. Eventually, hide the
  // icon as well.
  [controller_ updateVisibility];
}

// Overridden so that mouse clicks come to this view (the parent of the
// hierarchy) first. We want to handle clicks and drags in this class and
// leave the background button for display purposes only.
- (BOOL)acceptsFirstMouse:(NSEvent*)theEvent {
  return YES;
}

- (void)mouseEntered:(NSEvent*)theEvent {
  isMouseInside_ = YES;
  [self resetLastGlowUpdateTime];
  [self adjustGlowValue];
}

- (void)mouseMoved:(NSEvent*)theEvent {
  hoverPoint_ = [self convertPoint:[theEvent locationInWindow]
                          fromView:nil];
  [self setNeedsDisplay:YES];
}

- (void)mouseExited:(NSEvent*)theEvent {
  isMouseInside_ = NO;
  hoverHoldEndTime_ =
      [NSDate timeIntervalSinceReferenceDate] + kHoverHoldDuration;
  [self resetLastGlowUpdateTime];
  [self adjustGlowValue];
}

- (void)setTrackingEnabled:(BOOL)enabled {
  if (![closeButton_ isHidden]) {
    [closeButton_ setTrackingEnabled:enabled];
  }
}

// Determines which view a click in our frame actually hit. It's either this
// view or our child close button.
- (NSView*)hitTest:(NSPoint)aPoint {
  NSPoint viewPoint = [self convertPoint:aPoint fromView:[self superview]];
  if (![closeButton_ isHidden])
    if (NSPointInRect(viewPoint, [closeButton_ frame])) return closeButton_;

  NSRect pointRect = NSMakeRect(viewPoint.x, viewPoint.y, 1, 1);

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  NSImage* left = rb.GetNativeImageNamed(IDR_TAB_ALPHA_LEFT).ToNSImage();
  if (viewPoint.x < [left size].width) {
    NSRect imageRect = NSMakeRect(0, 0, [left size].width, [left size].height);
    if ([left hitTestRect:pointRect withImageDestinationRect:imageRect
          context:nil hints:nil flipped:NO]) {
      return self;
    }
    return nil;
  }

  NSImage* right = rb.GetNativeImageNamed(IDR_TAB_ALPHA_RIGHT).ToNSImage();
  CGFloat rightX = NSWidth([self bounds]) - [right size].width;
  if (viewPoint.x > rightX) {
    NSRect imageRect = NSMakeRect(
        rightX, 0, [right size].width, [right size].height);
    if ([right hitTestRect:pointRect withImageDestinationRect:imageRect
          context:nil hints:nil flipped:NO]) {
      return self;
    }
    return nil;
  }

  if (viewPoint.y < kFillHeight)
    return self;
  return nil;
}

// Returns |YES| if this tab can be torn away into a new window.
- (BOOL)canBeDragged {
  return [controller_ tabCanBeDragged:controller_];
}

// Handle clicks and drags in this button. We get here because we have
// overridden acceptsFirstMouse: and the click is within our bounds.
- (void)mouseDown:(NSEvent*)theEvent {
  if ([self isClosing])
    return;

  // Record the point at which this event happened. This is used by other mouse
  // events that are dispatched from |-maybeStartDrag::|.
  mouseDownPoint_ = [theEvent locationInWindow];

  // Record the state of the close button here, because selecting the tab will
  // unhide it.
  BOOL closeButtonActive = ![closeButton_ isHidden];

  // During the tab closure animation (in particular, during rapid tab closure),
  // we may get incorrectly hit with a mouse down. If it should have gone to the
  // close button, we send it there -- it should then track the mouse, so we
  // don't have to worry about mouse ups.
  if (closeButtonActive && [controller_ inRapidClosureMode]) {
    NSPoint hitLocation = [[self superview] convertPoint:mouseDownPoint_
                                                fromView:nil];
    if ([self hitTest:hitLocation] == closeButton_) {
      [closeButton_ mouseDown:theEvent];
      return;
    }
  }

  // If the tab gets torn off, the tab controller will be removed from the tab
  // strip and then deallocated. This will also result in *us* being
  // deallocated. Both these are bad, so we prevent this by retaining the
  // controller.
  base::scoped_nsobject<TabController> controller([controller_ retain]);

  // Try to initiate a drag. This will spin a custom event loop and may
  // dispatch other mouse events.
  [controller_ maybeStartDrag:theEvent forTab:controller];

  // The custom loop has ended, so clear the point.
  mouseDownPoint_ = NSZeroPoint;
}

- (void)mouseUp:(NSEvent*)theEvent {
  // Check for rapid tab closure.
  if ([theEvent type] == NSLeftMouseUp) {
    NSPoint upLocation = [theEvent locationInWindow];
    CGFloat dx = upLocation.x - mouseDownPoint_.x;
    CGFloat dy = upLocation.y - mouseDownPoint_.y;

    // During rapid tab closure (mashing tab close buttons), we may get hit
    // with a mouse down. As long as the mouse up is over the close button,
    // and the mouse hasn't moved too much, we close the tab.
    if (![closeButton_ isHidden] &&
        (dx*dx + dy*dy) <= kRapidCloseDist*kRapidCloseDist &&
        [controller_ inRapidClosureMode]) {
      NSPoint hitLocation =
          [[self superview] convertPoint:[theEvent locationInWindow]
                                fromView:nil];
      if ([self hitTest:hitLocation] == closeButton_) {
        [controller_ closeTab:self];
        return;
      }
    }
  }

  // Fire the action to select the tab.
  [controller_ selectTab:self];

  // Messaging the drag controller with |-endDrag:| would seem like the right
  // thing to do here. But, when a tab has been detached, the controller's
  // target is nil until the drag is finalized. Since |-mouseUp:| gets called
  // via the manual event loop inside -[TabStripDragController
  // maybeStartDrag:forTab:], the drag controller can end the dragging session
  // itself directly after calling this.
}

- (void)otherMouseUp:(NSEvent*)theEvent {
  if ([self isClosing])
    return;

  // Support middle-click-to-close.
  if ([theEvent buttonNumber] == 2) {
    // |-hitTest:| takes a location in the superview's coordinates.
    NSPoint upLocation =
        [[self superview] convertPoint:[theEvent locationInWindow]
                              fromView:nil];
    // If the mouse up occurred in our view or over the close button, then
    // close.
    if ([self hitTest:upLocation])
      [controller_ closeTab:self];
  }
}

// Returns the color used to draw the background of a tab. |selected| selects
// between the foreground and background tabs.
- (NSColor*)backgroundColorForSelected:(bool)selected {
  ThemeService* themeProvider =
      static_cast<ThemeService*>([[self window] themeProvider]);
  if (!themeProvider)
    return [[self window] backgroundColor];

  int bitmapResources[2][2] = {
    // Background window.
    {
      IDR_THEME_TAB_BACKGROUND_INACTIVE,  // Background tab.
      IDR_THEME_TOOLBAR_INACTIVE,         // Active tab.
    },
    // Currently focused window.
    {
      IDR_THEME_TAB_BACKGROUND,  // Background tab.
      IDR_THEME_TOOLBAR,         // Active tab.
    },
  };

  // Themes don't have an inactive image so only look for one if there's no
  // theme.
  bool active = [[self window] isKeyWindow] || [[self window] isMainWindow] ||
                !themeProvider->UsingDefaultTheme();
  return themeProvider->GetNSImageColorNamed(bitmapResources[active][selected]);
}

// Draws the active tab background.
- (void)drawFillForActiveTab:(NSRect)dirtyRect {
  NSColor* backgroundImageColor = [self backgroundColorForSelected:YES];
  [backgroundImageColor set];

  // Themes can have partially transparent images. NSRectFill() is measurably
  // faster though, so call it for the known-safe default theme.
  ThemeService* themeProvider =
      static_cast<ThemeService*>([[self window] themeProvider]);
  if (themeProvider && themeProvider->UsingDefaultTheme())
    NSRectFill(dirtyRect);
  else
    NSRectFillUsingOperation(dirtyRect, NSCompositeSourceOver);
}

// Draws the tab background.
- (void)drawFill:(NSRect)dirtyRect {
  gfx::ScopedNSGraphicsContextSaveGState scopedGState;
  NSGraphicsContext* context = [NSGraphicsContext currentContext];
  CGContextRef cgContext = static_cast<CGContextRef>([context graphicsPort]);

  ThemeService* themeProvider =
      static_cast<ThemeService*>([[self window] themeProvider]);
  NSPoint position = [[self window]
      themeImagePositionForAlignment: THEME_IMAGE_ALIGN_WITH_TAB_STRIP];
  [context cr_setPatternPhase:position forView:self];

  CGImageRef mask([self tabClippingMask]);
  CGRect maskBounds = CGRectMake(0, 0, maskCacheWidth_, kMaskHeight);
  CGContextClipToMask(cgContext, maskBounds, mask);

  // There is only 1 active tab at a time.
  // It has a different fill color which draws over the separator line.
  if (state_ == NSOnState) {
    [self drawFillForActiveTab:dirtyRect];
    return;
  }

  // Background tabs should not paint over the tab strip separator, which is
  // two pixels high in both lodpi and hidpi.
  if (dirtyRect.origin.y < 1)
    dirtyRect.origin.y = 2 * [self cr_lineWidth];

  // There can be multiple selected tabs.
  // They have the same fill color as the active tab, but do not draw over
  // the separator.
  if (state_ == NSMixedState) {
    [self drawFillForActiveTab:dirtyRect];
    return;
  }

  // Draw the tab background.
  NSColor* backgroundImageColor = [self backgroundColorForSelected:NO];
  [backgroundImageColor set];

  // Themes can have partially transparent images. NSRectFill() is measurably
  // faster though, so call it for the known-safe default theme.
  bool usingDefaultTheme = themeProvider && themeProvider->UsingDefaultTheme();
  if (usingDefaultTheme)
    NSRectFill(dirtyRect);
  else
    NSRectFillUsingOperation(dirtyRect, NSCompositeSourceOver);

  // Draw the glow for hover and the overlay for alerts.
  CGFloat hoverAlpha = [self hoverAlpha];
  CGFloat alertAlpha = [self alertAlpha];
  if (hoverAlpha > 0 || alertAlpha > 0) {
    gfx::ScopedNSGraphicsContextSaveGState contextSave;
    CGContextBeginTransparencyLayer(cgContext, 0);

    // The alert glow overlay is like the selected state but at most at most 80%
    // opaque. The hover glow brings up the overlay's opacity at most 50%.
    CGFloat backgroundAlpha = 0.8 * alertAlpha;
    backgroundAlpha += (1 - backgroundAlpha) * 0.5 * hoverAlpha;
    CGContextSetAlpha(cgContext, backgroundAlpha);

    [self drawFillForActiveTab:dirtyRect];

    // ui::ThemeProvider::HasCustomImage is true only if the theme provides the
    // image. However, even if the theme doesn't provide a tab background, the
    // theme machinery will make one if given a frame image. See
    // BrowserThemePack::GenerateTabBackgroundImages for details.
    BOOL hasCustomTheme = themeProvider &&
        (themeProvider->HasCustomImage(IDR_THEME_TAB_BACKGROUND) ||
         themeProvider->HasCustomImage(IDR_THEME_FRAME));
    // Draw a mouse hover gradient for the default themes.
    if (hoverAlpha > 0) {
      if (themeProvider && !hasCustomTheme) {
        base::scoped_nsobject<NSGradient> glow([NSGradient alloc]);
        [glow initWithStartingColor:[NSColor colorWithCalibratedWhite:1.0
                                        alpha:1.0 * hoverAlpha]
                        endingColor:[NSColor colorWithCalibratedWhite:1.0
                                                                alpha:0.0]];
        NSRect rect = [self bounds];
        NSPoint point = hoverPoint_;
        point.y = NSHeight(rect);
        [glow drawFromCenter:point
                      radius:0.0
                    toCenter:point
                      radius:NSWidth(rect) / 3.0
                     options:NSGradientDrawsBeforeStartingLocation];
      }
    }

    CGContextEndTransparencyLayer(cgContext);
  }
}

// Draws the tab outline.
- (void)drawStroke:(NSRect)dirtyRect {
  BOOL focused = [[self window] isKeyWindow] || [[self window] isMainWindow];
  CGFloat alpha = focused ? 1.0 : tabs::kImageNoFocusAlpha;

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  float height =
      [rb.GetNativeImageNamed(IDR_TAB_ACTIVE_LEFT).ToNSImage() size].height;
  if (state_ == NSOnState) {
    NSDrawThreePartImage(NSMakeRect(0, 0, NSWidth([self bounds]), height),
        rb.GetNativeImageNamed(IDR_TAB_ACTIVE_LEFT).ToNSImage(),
        rb.GetNativeImageNamed(IDR_TAB_ACTIVE_CENTER).ToNSImage(),
        rb.GetNativeImageNamed(IDR_TAB_ACTIVE_RIGHT).ToNSImage(),
        /*vertical=*/NO,
        NSCompositeSourceOver,
        alpha,
        /*flipped=*/NO);
  } else {
    NSDrawThreePartImage(NSMakeRect(0, 0, NSWidth([self bounds]), height),
        rb.GetNativeImageNamed(IDR_TAB_INACTIVE_LEFT).ToNSImage(),
        rb.GetNativeImageNamed(IDR_TAB_INACTIVE_CENTER).ToNSImage(),
        rb.GetNativeImageNamed(IDR_TAB_INACTIVE_RIGHT).ToNSImage(),
        /*vertical=*/NO,
        NSCompositeSourceOver,
        alpha,
        /*flipped=*/NO);
  }
}

- (void)drawRect:(NSRect)dirtyRect {
  // Close button and image are drawn by subviews.
  [self drawFill:dirtyRect];
  [self drawStroke:dirtyRect];

  // We draw the title string directly instead of using a NSTextField subview.
  // This is so that we can get font smoothing to work on earlier OS, and even
  // when the tab background is a pattern image (when using themes).
  if (![titleView_ isHidden]) {
    gfx::ScopedNSGraphicsContextSaveGState scopedGState;
    NSGraphicsContext* context = [NSGraphicsContext currentContext];
    CGContextRef cgContext = static_cast<CGContextRef>([context graphicsPort]);
    CGContextSetShouldSmoothFonts(cgContext, true);
    [[titleView_ cell] drawWithFrame:[titleView_ frame] inView:self];
  }
}

- (void)setFrameOrigin:(NSPoint)origin {
  // The background color depends on the view's vertical position.
  if (NSMinY([self frame]) != origin.y)
    [self setNeedsDisplay:YES];
  [super setFrameOrigin:origin];
}

// Override this to catch the text so that we can choose when to display it.
- (void)setToolTip:(NSString*)string {
  toolTipText_.reset([string retain]);
}

- (NSString*)toolTipText {
  if (!toolTipText_.get()) {
    return @"";
  }
  return toolTipText_.get();
}

- (void)viewDidMoveToWindow {
  [super viewDidMoveToWindow];
  if ([self window]) {
    [controller_ updateTitleColor];
  }
}

- (NSString*)title {
  return [titleView_ stringValue];
}

- (void)setTitle:(NSString*)title {
  [titleView_ setStringValue:title];

  base::string16 title16 = base::SysNSStringToUTF16(title);
  bool isRTL = base::i18n::GetFirstStrongCharacterDirection(title16) ==
               base::i18n::RIGHT_TO_LEFT;
  titleViewCell_.truncateMode = isRTL ? GTMFadeTruncatingHead
                                      : GTMFadeTruncatingTail;

  [self setNeedsDisplayInRect:[titleView_ frame]];
}

- (NSRect)titleFrame {
  return [titleView_ frame];
}

- (void)setTitleFrame:(NSRect)titleFrame {
  [titleView_ setFrame:titleFrame];
  [self setNeedsDisplayInRect:titleFrame];
}

- (NSColor*)titleColor {
  return [titleView_ textColor];
}

- (void)setTitleColor:(NSColor*)titleColor {
  [titleView_ setTextColor:titleColor];
  [self setNeedsDisplayInRect:[titleView_ frame]];
}

- (BOOL)titleHidden {
  return [titleView_ isHidden];
}

- (void)setTitleHidden:(BOOL)titleHidden {
  [titleView_ setHidden:titleHidden];
  [self setNeedsDisplayInRect:[titleView_ frame]];
}

- (void)setState:(NSCellStateValue)state {
  if (state_ == state)
    return;
  state_ = state;
  [self setNeedsDisplay:YES];
}

- (void)setClosing:(BOOL)closing {
  closing_ = closing;  // Safe because the property is nonatomic.
  // When closing, ensure clicks to the close button go nowhere.
  if (closing) {
    [closeButton_ setTarget:nil];
    [closeButton_ setAction:nil];
  }
}

- (void)startAlert {
  // Do not start a new alert while already alerting or while in a decay cycle.
  if (alertState_ == tabs::kAlertNone) {
    alertState_ = tabs::kAlertRising;
    [self resetLastGlowUpdateTime];
    [self adjustGlowValue];
  }
}

- (void)cancelAlert {
  if (alertState_ != tabs::kAlertNone) {
    alertState_ = tabs::kAlertFalling;
    alertHoldEndTime_ =
        [NSDate timeIntervalSinceReferenceDate] + kGlowUpdateInterval;
    [self resetLastGlowUpdateTime];
    [self adjustGlowValue];
  }
}

- (BOOL)accessibilityIsIgnored {
  return NO;
}

- (NSArray*)accessibilityActionNames {
  NSArray* parentActions = [super accessibilityActionNames];

  return [parentActions arrayByAddingObject:NSAccessibilityPressAction];
}

- (NSArray*)accessibilityAttributeNames {
  NSMutableArray* attributes =
      [[super accessibilityAttributeNames] mutableCopy];
  [attributes addObject:NSAccessibilityTitleAttribute];
  [attributes addObject:NSAccessibilityEnabledAttribute];
  [attributes addObject:NSAccessibilityValueAttribute];

  return [attributes autorelease];
}

- (BOOL)accessibilityIsAttributeSettable:(NSString*)attribute {
  if ([attribute isEqual:NSAccessibilityTitleAttribute])
    return NO;

  if ([attribute isEqual:NSAccessibilityEnabledAttribute])
    return NO;

  if ([attribute isEqual:NSAccessibilityValueAttribute])
    return YES;

  return [super accessibilityIsAttributeSettable:attribute];
}

- (void)accessibilityPerformAction:(NSString*)action {
  if ([action isEqual:NSAccessibilityPressAction] &&
      [[controller_ target] respondsToSelector:[controller_ action]]) {
    [[controller_ target] performSelector:[controller_ action]
        withObject:self];
    NSAccessibilityPostNotification(self,
                                    NSAccessibilityValueChangedNotification);
  } else {
    [super accessibilityPerformAction:action];
  }
}

- (id)accessibilityAttributeValue:(NSString*)attribute {
  if ([attribute isEqual:NSAccessibilityRoleAttribute])
    return NSAccessibilityRadioButtonRole;
  if ([attribute isEqual:NSAccessibilityRoleDescriptionAttribute])
    return l10n_util::GetNSStringWithFixup(IDS_ACCNAME_TAB);
  if ([attribute isEqual:NSAccessibilityTitleAttribute])
    return [controller_ title];
  if ([attribute isEqual:NSAccessibilityValueAttribute])
    return [NSNumber numberWithInt:[controller_ selected]];
  if ([attribute isEqual:NSAccessibilityEnabledAttribute])
    return [NSNumber numberWithBool:YES];

  return [super accessibilityAttributeValue:attribute];
}

- (ViewID)viewID {
  return VIEW_ID_TAB;
}

@end  // @implementation TabView

@implementation TabView (TabControllerInterface)

- (void)setController:(TabController*)controller {
  controller_ = controller;
}

@end  // @implementation TabView (TabControllerInterface)

@implementation TabView(Private)

- (void)resetLastGlowUpdateTime {
  lastGlowUpdate_ = [NSDate timeIntervalSinceReferenceDate];
}

- (NSTimeInterval)timeElapsedSinceLastGlowUpdate {
  return [NSDate timeIntervalSinceReferenceDate] - lastGlowUpdate_;
}

- (void)adjustGlowValue {
  // A time interval long enough to represent no update.
  const NSTimeInterval kNoUpdate = 1000000;

  // Time until next update for either glow.
  NSTimeInterval nextUpdate = kNoUpdate;

  NSTimeInterval elapsed = [self timeElapsedSinceLastGlowUpdate];
  NSTimeInterval currentTime = [NSDate timeIntervalSinceReferenceDate];

  // TODO(viettrungluu): <http://crbug.com/30617> -- split off the stuff below
  // into a pure function and add a unit test.

  CGFloat hoverAlpha = [self hoverAlpha];
  if (isMouseInside_) {
    // Increase hover glow until it's 1.
    if (hoverAlpha < 1) {
      hoverAlpha = MIN(hoverAlpha + elapsed / kHoverShowDuration, 1);
      [self setHoverAlpha:hoverAlpha];
      nextUpdate = MIN(kGlowUpdateInterval, nextUpdate);
    }  // Else already 1 (no update needed).
  } else {
    if (currentTime >= hoverHoldEndTime_) {
      // No longer holding, so decrease hover glow until it's 0.
      if (hoverAlpha > 0) {
        hoverAlpha = MAX(hoverAlpha - elapsed / kHoverHideDuration, 0);
        [self setHoverAlpha:hoverAlpha];
        nextUpdate = MIN(kGlowUpdateInterval, nextUpdate);
      }  // Else already 0 (no update needed).
    } else {
      // Schedule update for end of hold time.
      nextUpdate = MIN(hoverHoldEndTime_ - currentTime, nextUpdate);
    }
  }

  CGFloat alertAlpha = [self alertAlpha];
  if (alertState_ == tabs::kAlertRising) {
    // Increase alert glow until it's 1 ...
    alertAlpha = MIN(alertAlpha + elapsed / kAlertShowDuration, 1);
    [self setAlertAlpha:alertAlpha];

    // ... and having reached 1, switch to holding.
    if (alertAlpha >= 1) {
      alertState_ = tabs::kAlertHolding;
      alertHoldEndTime_ = currentTime + kAlertHoldDuration;
      nextUpdate = MIN(kAlertHoldDuration, nextUpdate);
    } else {
      nextUpdate = MIN(kGlowUpdateInterval, nextUpdate);
    }
  } else if (alertState_ != tabs::kAlertNone) {
    if (alertAlpha > 0) {
      if (currentTime >= alertHoldEndTime_) {
        // Stop holding, then decrease alert glow (until it's 0).
        if (alertState_ == tabs::kAlertHolding) {
          alertState_ = tabs::kAlertFalling;
          nextUpdate = MIN(kGlowUpdateInterval, nextUpdate);
        } else {
          DCHECK_EQ(tabs::kAlertFalling, alertState_);
          alertAlpha = MAX(alertAlpha - elapsed / kAlertHideDuration, 0);
          [self setAlertAlpha:alertAlpha];
          nextUpdate = MIN(kGlowUpdateInterval, nextUpdate);
        }
      } else {
        // Schedule update for end of hold time.
        nextUpdate = MIN(alertHoldEndTime_ - currentTime, nextUpdate);
      }
    } else {
      // Done the alert decay cycle.
      alertState_ = tabs::kAlertNone;
    }
  }

  if (nextUpdate < kNoUpdate)
    [self performSelector:_cmd withObject:nil afterDelay:nextUpdate];

  [self resetLastGlowUpdateTime];
  [self setNeedsDisplay:YES];
}

- (CGImageRef)tabClippingMask {
  // NOTE: NSHeight([self bounds]) doesn't match the height of the bitmaps.
  CGFloat scale = 1;
  if ([[self window] respondsToSelector:@selector(backingScaleFactor)])
    scale = [[self window] backingScaleFactor];

  NSRect bounds = [self bounds];
  CGFloat tabWidth = NSWidth(bounds);
  if (tabWidth == maskCacheWidth_ && scale == maskCacheScale_)
    return maskCache_.get();

  maskCacheWidth_ = tabWidth;
  maskCacheScale_ = scale;

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  NSImage* leftMask = rb.GetNativeImageNamed(IDR_TAB_ALPHA_LEFT).ToNSImage();
  NSImage* rightMask = rb.GetNativeImageNamed(IDR_TAB_ALPHA_RIGHT).ToNSImage();

  CGFloat leftWidth = leftMask.size.width;
  CGFloat rightWidth = rightMask.size.width;

  // Image masks must be in the DeviceGray colorspace. Create a context and
  // draw the mask into it.
  base::ScopedCFTypeRef<CGColorSpaceRef> colorspace(
      CGColorSpaceCreateDeviceGray());
  base::ScopedCFTypeRef<CGContextRef> maskContext(
      CGBitmapContextCreate(NULL, tabWidth * scale, kMaskHeight * scale,
                            8, tabWidth * scale, colorspace, 0));
  CGContextScaleCTM(maskContext, scale, scale);
  NSGraphicsContext* maskGraphicsContext =
      [NSGraphicsContext graphicsContextWithGraphicsPort:maskContext
                                                 flipped:NO];

  gfx::ScopedNSGraphicsContextSaveGState scopedGState;
  [NSGraphicsContext setCurrentContext:maskGraphicsContext];

  // Draw mask image.
  [[NSColor blackColor] setFill];
  CGContextFillRect(maskContext, CGRectMake(0, 0, tabWidth, kMaskHeight));

  NSDrawThreePartImage(NSMakeRect(0, 0, tabWidth, kMaskHeight),
      leftMask, nil, rightMask, /*vertical=*/NO, NSCompositeSourceOver, 1.0,
      /*flipped=*/NO);

  CGFloat middleWidth = tabWidth - leftWidth - rightWidth;
  NSRect middleRect = NSMakeRect(leftWidth, 0, middleWidth, kFillHeight);
  [[NSColor whiteColor] setFill];
  NSRectFill(middleRect);

  maskCache_.reset(CGBitmapContextCreateImage(maskContext));
  return maskCache_;
}

@end  // @implementation TabView(Private)
