// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/browser/zoom_bubble_controller.h"

#include "base/mac/foundation_util.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/chrome_page_zoom.h"
#import "chrome/browser/ui/cocoa/hover_button.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#import "chrome/browser/ui/cocoa/info_bubble_view.h"
#include "chrome/browser/ui/zoom/zoom_controller.h"
#include "content/public/common/page_zoom.h"
#include "grit/generated_resources.h"
#include "skia/ext/skia_utils_mac.h"
#import "ui/base/cocoa/window_size_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/native_theme/native_theme.h"

@interface ZoomBubbleController (Private)
- (void)performLayout;
- (void)autoCloseBubble;
- (NSAttributedString*)attributedStringWithString:(NSString*)string
                                         fontSize:(CGFloat)fontSize;
// Adds a new zoom button to the bubble.
- (NSButton*)addButtonWithTitleID:(int)titleID
                         fontSize:(CGFloat)fontSize
                           action:(SEL)action;
- (NSTextField*)addZoomPercentTextField;
- (void)updateAutoCloseTimer;
@end

// Button that highlights the background on mouse over.
@interface ZoomHoverButton : HoverButton
@end

namespace {

// The amount of time to wait before the bubble automatically closes.
// Should keep in sync with kBubbleCloseDelay in
// src/chrome/browser/ui/views/location_bar/zoom_bubble_view.cc.
NSTimeInterval gAutoCloseDelay = 1.5;

// The height of the window.
const CGFloat kWindowHeight = 29.0;

// Width of the zoom in and zoom out buttons.
const CGFloat kZoomInOutButtonWidth = 44.0;

// Width of zoom label.
const CGFloat kZoomLabelWidth = 55.0;

// Horizontal margin for the reset zoom button.
const CGFloat kResetZoomMargin = 9.0;

// The font size text shown in the bubble.
const CGFloat kTextFontSize = 12.0;

// The font size of the zoom in and zoom out buttons.
const CGFloat kZoomInOutButtonFontSize = 16.0;

}  // namespace

namespace chrome {

void SetZoomBubbleAutoCloseDelayForTesting(NSTimeInterval time_interval) {
  gAutoCloseDelay = time_interval;
}

}  // namespace chrome

@implementation ZoomBubbleController

- (id)initWithParentWindow:(NSWindow*)parentWindow
             closeObserver:(void(^)(ZoomBubbleController*))closeObserver {
  scoped_nsobject<InfoBubbleWindow> window([[InfoBubbleWindow alloc]
      initWithContentRect:NSMakeRect(0, 0, 200, 100)
                styleMask:NSBorderlessWindowMask
                  backing:NSBackingStoreBuffered
                    defer:NO]);
  if ((self = [super initWithWindow:window
                       parentWindow:parentWindow
                         anchoredAt:NSZeroPoint])) {
    closeObserver_.reset(Block_copy(closeObserver));

    ui::NativeTheme* nativeTheme = ui::NativeTheme::instance();
    [[self bubble] setAlignment:info_bubble::kAlignRightEdgeToAnchorEdge];
    [[self bubble] setArrowLocation:info_bubble::kNoArrow];
    [[self bubble] setBackgroundColor:
        gfx::SkColorToCalibratedNSColor(nativeTheme->GetSystemColor(
            ui::NativeTheme::kColorId_DialogBackground))];

    [self performLayout];

    trackingArea_.reset([[CrTrackingArea alloc]
        initWithRect:NSZeroRect
             options:NSTrackingMouseEnteredAndExited |
                     NSTrackingActiveInKeyWindow |
                     NSTrackingInVisibleRect
               owner:self
            userInfo:nil]);
    [trackingArea_.get() clearOwnerWhenWindowWillClose:[self window]];
    [[[self window] contentView] addTrackingArea:trackingArea_.get()];
  }
  return self;
}

- (void)showForWebContents:(content::WebContents*)contents
                anchoredAt:(NSPoint)anchorPoint
                 autoClose:(BOOL)autoClose {
  contents_ = contents;
  [self onZoomChanged];
  InfoBubbleWindow* window =
      base::mac::ObjCCastStrict<InfoBubbleWindow>([self window]);
  [window setAllowedAnimations:autoClose
      ? info_bubble::kAnimateOrderIn | info_bubble::kAnimateOrderOut
      : info_bubble::kAnimateNone];

  self.anchorPoint = anchorPoint;
  [self showWindow:nil];

  autoClose_ = autoClose;
  [self updateAutoCloseTimer];
}

- (void)onZoomChanged {
  if (!contents_)
    return;  // NULL in tests.

  ZoomController* zoomController = ZoomController::FromWebContents(contents_);
  int percent = zoomController->zoom_percent();
  NSString* string =
      l10n_util::GetNSStringF(IDS_ZOOM_PERCENT, base::IntToString16(percent));
  [zoomPercent_ setAttributedStringValue:
      [self attributedStringWithString:string
                              fontSize:kTextFontSize]];

  [self updateAutoCloseTimer];
}

- (void)resetToDefault:(id)sender {
  chrome_page_zoom::Zoom(contents_, content::PAGE_ZOOM_RESET);
}

- (void)zoomIn:(id)sender {
  chrome_page_zoom::Zoom(contents_, content::PAGE_ZOOM_IN);
}

- (void)zoomOut:(id)sender {
  chrome_page_zoom::Zoom(contents_, content::PAGE_ZOOM_OUT);
}

- (void)windowWillClose:(NSNotification*)notification {
  contents_ = NULL;
  closeObserver_.get()(self);
  [NSObject cancelPreviousPerformRequestsWithTarget:self
                                           selector:@selector(autoCloseBubble)
                                             object:nil];
  [super windowWillClose:notification];
}

- (void)mouseEntered:(NSEvent*)theEvent {
  isMouseInside_ = YES;
  [self updateAutoCloseTimer];
}

- (void)mouseExited:(NSEvent*)theEvent {
  isMouseInside_ = NO;
  [self updateAutoCloseTimer];
}

// Private /////////////////////////////////////////////////////////////////////

- (void)performLayout {
  // Zoom out button.
  NSButton* zoomOutButton = [self addButtonWithTitleID:IDS_ZOOM_MINUS2
                                              fontSize:kZoomInOutButtonFontSize
                                                action:@selector(zoomOut:)];
  NSRect rect = NSMakeRect(0, 0, kZoomInOutButtonWidth, kWindowHeight);
  [zoomOutButton setFrame:rect];

  // Zoom label.
  zoomPercent_.reset([[self addZoomPercentTextField] retain]);
  rect.origin.x += NSWidth(rect);
  rect.size.width = kZoomLabelWidth;
  [zoomPercent_ sizeToFit];
  NSRect zoomRect = rect;
  zoomRect.size.height = NSHeight([zoomPercent_ frame]);
  zoomRect.origin.y = roundf((NSHeight(rect) - NSHeight(zoomRect)) / 2.0);
  [zoomPercent_ setFrame:zoomRect];

  // Zoom in button.
  NSButton* zoomInButton = [self addButtonWithTitleID:IDS_ZOOM_PLUS2
                                             fontSize:kZoomInOutButtonFontSize
                                               action:@selector(zoomIn:)];
  rect.origin.x += NSWidth(rect);
  rect.size.width = kZoomInOutButtonWidth;
  [zoomInButton setFrame:rect];

  // Separator view.
  rect.origin.x += NSWidth(rect);
  rect.size.width = 1;
  scoped_nsobject<NSBox> separatorView([[NSBox alloc] initWithFrame:rect]);
  [separatorView setBoxType:NSBoxCustom];
  ui::NativeTheme* nativeTheme = ui::NativeTheme::instance();
  [separatorView setBorderColor:
      gfx::SkColorToCalibratedNSColor(nativeTheme->GetSystemColor(
          ui::NativeTheme::kColorId_MenuSeparatorColor))];
  [[[self window] contentView] addSubview:separatorView];

  // Reset zoom button.
  NSButton* resetButton =
      [self addButtonWithTitleID:IDS_ZOOM_SET_DEFAULT_SHORT
                        fontSize:kTextFontSize
                          action:@selector(resetToDefault:)];
  rect.origin.x += NSWidth(rect);
  rect.size.width =
      [[resetButton attributedTitle] size].width + kResetZoomMargin * 2.0;
  [resetButton setFrame:rect];

  // Update window frame.
  NSRect windowFrame = [[self window] frame];
  windowFrame.size.height = NSHeight(rect);
  windowFrame.size.width = NSMaxX(rect);
  [[self window] setFrame:windowFrame display:YES];
}

- (void)autoCloseBubble {
  if (!autoClose_)
    return;
  [self close];
}

- (NSAttributedString*)attributedStringWithString:(NSString*)string
                                           fontSize:(CGFloat)fontSize {
  scoped_nsobject<NSMutableParagraphStyle> paragraphStyle(
      [[NSMutableParagraphStyle alloc] init]);
  [paragraphStyle setAlignment:NSCenterTextAlignment];
  NSDictionary* attributes = @{
      NSFontAttributeName:
      [NSFont systemFontOfSize:fontSize],
      NSForegroundColorAttributeName:
      [NSColor colorWithCalibratedWhite:0.58 alpha:1.0],
      NSParagraphStyleAttributeName:
      paragraphStyle.get()
  };
  return [[[NSAttributedString alloc]
      initWithString:string
          attributes:attributes] autorelease];
}

- (NSButton*)addButtonWithTitleID:(int)titleID
                         fontSize:(CGFloat)fontSize
                           action:(SEL)action {
  scoped_nsobject<NSButton> button(
      [[ZoomHoverButton alloc] initWithFrame:NSZeroRect]);
  NSString* title = l10n_util::GetNSStringWithFixup(titleID);
  [button setAttributedTitle:[self attributedStringWithString:title
                                                     fontSize:fontSize]];
  [[button cell] setBordered:NO];
  [button setTarget:self];
  [button setAction:action];
  [[[self window] contentView] addSubview:button];
  return button.autorelease();
}

- (NSTextField*)addZoomPercentTextField {
  scoped_nsobject<NSTextField> textField(
      [[NSTextField alloc] initWithFrame:NSZeroRect]);
  [textField setEditable:NO];
  [textField setBordered:NO];
  [textField setDrawsBackground:NO];
  [[[self window] contentView] addSubview:textField];
  return textField.autorelease();
}

- (void)updateAutoCloseTimer {
  [NSObject cancelPreviousPerformRequestsWithTarget:self
                                           selector:@selector(autoCloseBubble)
                                             object:nil];
  if (autoClose_ && !isMouseInside_) {
    [self performSelector:@selector(autoCloseBubble)
               withObject:nil
               afterDelay:gAutoCloseDelay];
  }
}

@end

@implementation ZoomHoverButton

- (void)drawRect:(NSRect)rect {
  NSRect bounds = [self bounds];
  if ([self hoverState] != kHoverStateNone) {
    ui::NativeTheme* nativeTheme = ui::NativeTheme::instance();
    [gfx::SkColorToCalibratedNSColor(nativeTheme->GetSystemColor(
        ui::NativeTheme::kColorId_FocusedMenuItemBackgroundColor)) set];
    NSRectFillUsingOperation(bounds, NSCompositeSourceOver);
  }

  [[self cell] drawTitle:[self attributedTitle]
               withFrame:bounds
                  inView:self];
}

@end
