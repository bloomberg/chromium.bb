// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/permission_bubble/permission_bubble_controller.h"

#include <algorithm>

#include "base/mac/bind_objc_block.h"
#include "base/macros.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/permissions/permission_request.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/browser_window_utils.h"
#import "chrome/browser/ui/cocoa/bubble_anchor_helper.h"
#import "chrome/browser/ui/cocoa/chrome_style.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_button.h"
#import "chrome/browser/ui/cocoa/hover_close_button.h"
#import "chrome/browser/ui/cocoa/info_bubble_view.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_decoration.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#include "chrome/browser/ui/cocoa/page_info/page_info_utils_cocoa.h"
#include "chrome/browser/ui/cocoa/page_info/permission_selector_button.h"
#include "chrome/browser/ui/cocoa/page_info/split_block_button.h"
#include "chrome/browser/ui/cocoa/permission_bubble/permission_bubble_cocoa.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/permission_bubble/permission_prompt.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "skia/ext/skia_utils_mac.h"
#include "ui/base/cocoa/a11y_util.h"
#include "ui/base/cocoa/cocoa_base_utils.h"
#import "ui/base/cocoa/controls/hyperlink_text_view.h"
#include "ui/base/cocoa/window_size_constants.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image_skia_util_mac.h"
#include "ui/gfx/paint_vector_icon.h"
#include "url/gurl.h"

using base::UserMetricsAction;

namespace {

// Distance between permission icon and permission label.
const CGFloat kHorizontalIconPadding = 8.0f;

// Distance between two permission labels.
const CGFloat kVerticalPermissionPadding = 2.0f;

const CGFloat kHorizontalPadding = 13.0f;
const CGFloat kVerticalPadding = 15.0f;
const CGFloat kBetweenButtonsPadding = 10.0f;
const CGFloat kButtonRightEdgePadding = 17.0f;
const CGFloat kTitlePaddingX = 50.0f;
const CGFloat kBubbleMinWidth = 315.0f;
const NSSize kPermissionIconSize = {18, 18};

}  // namespace

@interface PermissionBubbleController ()

// Determines if the bubble has an anchor in a corner or no anchor at all.
- (info_bubble::BubbleArrowLocation)getExpectedArrowLocation;

// Returns the expected parent for this bubble.
- (NSWindow*)getExpectedParentWindow;

// Returns an autoreleased NSView displaying the icon and label for |request|.
- (NSView*)labelForRequest:(PermissionRequest*)request;

// Returns an autoreleased NSView displaying the title for the bubble
// requesting settings for |host|.
- (NSView*)titleWithOrigin:(const GURL&)origin;

// Returns an autoreleased NSView of a button with |title| and |action|.
- (NSView*)buttonWithTitle:(NSString*)title
                    action:(SEL)action;

// Returns an autoreleased NSView displaying a block button.
- (NSView*)blockButton;

// Returns an autoreleased NSView displaying the close 'x' button.
- (NSView*)closeButton;

// Called when the 'ok' button is pressed.
- (void)ok:(id)sender;

// Called when the 'allow' button is pressed.
- (void)onAllow:(id)sender;

// Called when the 'block' button is pressed.
- (void)onBlock:(id)sender;

// Called when the 'close' button is pressed.
- (void)onClose:(id)sender;

// Sets the width of both |viewA| and |viewB| to be the larger of the
// two views' widths.  Does not change either view's origin or height.
+ (CGFloat)matchWidthsOf:(NSView*)viewA andOf:(NSView*)viewB;

// Sets the offset of |viewA| so that its vertical center is aligned with the
// vertical center of |viewB|.
+ (void)alignCenterOf:(NSView*)viewA verticallyToCenterOf:(NSView*)viewB;

// BaseBubbleController override.
- (IBAction)cancel:(id)sender;

@end

@implementation PermissionBubbleController

- (id)initWithBrowser:(Browser*)browser bridge:(PermissionBubbleCocoa*)bridge {
  DCHECK(browser);
  DCHECK(bridge);
  browser_ = browser;
  base::scoped_nsobject<InfoBubbleWindow> window([[InfoBubbleWindow alloc]
      initWithContentRect:ui::kWindowSizeDeterminedLater
                styleMask:NSBorderlessWindowMask
                  backing:NSBackingStoreBuffered
                    defer:NO]);

  [window setAllowedAnimations:info_bubble::kAnimateNone];
  [window setReleasedWhenClosed:NO];
  if ((self = [super initWithWindow:window
                       parentWindow:[self getExpectedParentWindow]
                         anchoredAt:NSZeroPoint])) {
    [self setShouldCloseOnResignKey:NO];
    [self setShouldOpenAsKeyWindow:YES];
    [[self bubble] setArrowLocation:[self getExpectedArrowLocation]];
    bridge_ = bridge;
    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
    [center addObserver:self
               selector:@selector(parentWindowDidMove:)
                   name:NSWindowDidMoveNotification
                 object:[self getExpectedParentWindow]];
  }
  return self;
}

+ (NSPoint)getAnchorPointForBrowser:(Browser*)browser {
  return GetPageInfoAnchorPointForBrowser(
      browser,
      [PermissionBubbleController hasVisibleLocationBarForBrowser:browser]);
}

+ (bool)hasVisibleLocationBarForBrowser:(Browser*)browser {
  return HasVisibleLocationBarForBrowser(browser);
}

- (void)windowWillClose:(NSNotification*)notification {
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:NSWindowDidMoveNotification
              object:nil];
  bridge_->OnBubbleClosing();
  [super windowWillClose:notification];
}

- (void)showWindow:(id)sender {
  LocationBarViewMac* bridge =
      [[self.parentWindow windowController] locationBarBridge];
  if ([self hasVisibleLocationBar] && bridge) {
    decoration_ = bridge->GetPageInfoDecoration();
    decoration_->SetActive(true);
  }

  [super showWindow:sender];
}

- (void)close {
  if (decoration_) {
    decoration_->SetActive(false);
    decoration_ = nullptr;
  }

  [super close];
}

- (void)parentWindowWillToggleFullScreen:(NSNotification*)notification {
  // Override the base class implementation, which would have closed the bubble.
}

- (void)parentWindowDidResize:(NSNotification*)notification {
  // Override the base class implementation, which sets the anchor point. But
  // it's not necessary since BrowserWindowController will notify the
  // PermissionRequestManager to update the anchor position on a resize.
}

- (void)parentWindowDidMove:(NSNotification*)notification {
  DCHECK(bridge_);
  [self setAnchorPoint:[self getExpectedAnchorPoint]];
}

- (void)showWithDelegate:(PermissionPrompt::Delegate*)delegate {
  DCHECK(delegate);
  delegate_ = delegate;

  const std::vector<PermissionRequest*>& requests = delegate->Requests();
  DCHECK(!requests.empty());

  NSView* contentView = [[self window] contentView];
  [contentView setSubviews:@[]];

  // Create one button to use as a guide for the permissions' y-offsets.
  NSString* allowTitle = l10n_util::GetNSString(IDS_PERMISSION_ALLOW);
  base::scoped_nsobject<NSView> allowButton(
      [[self buttonWithTitle:allowTitle action:@selector(onAllow:)] retain]);
  CGFloat yOffset = 2 * kVerticalPadding + NSMaxY([allowButton frame]);

  CGFloat maxPermissionLineWidth = 0;
  CGFloat verticalPadding = 0.0f;
  for (auto it = requests.begin(); it != requests.end(); it++) {
    base::scoped_nsobject<NSView> permissionView(
        [[self labelForRequest:(*it)] retain]);
    NSPoint origin = [permissionView frame].origin;
    origin.x += kHorizontalPadding;
    origin.y += yOffset + verticalPadding;
    [permissionView setFrameOrigin:origin];
    [contentView addSubview:permissionView];

    maxPermissionLineWidth = std::max(
        maxPermissionLineWidth, NSMaxX([permissionView frame]));
    yOffset += NSHeight([permissionView frame]);

    // Add extra padding for all but first permission.
    verticalPadding = kVerticalPermissionPadding;
  }

  base::scoped_nsobject<NSView> titleView(
      [[self titleWithOrigin:requests[0]->GetOrigin()] retain]);
  [contentView addSubview:titleView];
  [titleView setFrameOrigin:NSMakePoint(kHorizontalPadding,
                                        kVerticalPadding + yOffset)];

  // 'x' button in the upper-right-hand corner.
  base::scoped_nsobject<NSView> closeButton([[self closeButton] retain]);

  // Determine the dimensions of the bubble.
  // Once the height and width are set, the buttons can be laid out correctly.
  NSRect bubbleFrame = NSMakeRect(0, 0, kBubbleMinWidth, 0);

  // Fix the height of the bubble relative to the title.
  bubbleFrame.size.height = NSMaxY([titleView frame]) + kVerticalPadding +
                            info_bubble::kBubbleArrowHeight;

  // The title and 'x' button row must fit within the bubble.
  CGFloat titleRowWidth = NSMaxX([titleView frame]) +
                          NSWidth([closeButton frame]) +
                          chrome_style::kCloseButtonPadding;

  bubbleFrame.size.width =
      std::max(NSWidth(bubbleFrame),
               std::max(titleRowWidth, maxPermissionLineWidth)) +
      kHorizontalPadding;

  // Now that the bubble's dimensions have been set, lay out the buttons.

  // Place the close button at the upper-right-hand corner of the bubble.
  NSPoint closeButtonOrigin =
      NSMakePoint(NSWidth(bubbleFrame) - NSWidth([closeButton frame]) -
                      chrome_style::kCloseButtonPadding,
                  NSHeight(bubbleFrame) - NSWidth([closeButton frame]) -
                      chrome_style::kCloseButtonPadding);
  // Account for the bubble's arrow.
  closeButtonOrigin.y -= info_bubble::kBubbleArrowHeight;

  [closeButton setFrameOrigin:closeButtonOrigin];
  [contentView addSubview:closeButton];

  // Position the allow/ok button.
  CGFloat xOrigin = NSWidth(bubbleFrame) - NSWidth([allowButton frame]) -
                    kButtonRightEdgePadding;
  [allowButton setFrameOrigin:NSMakePoint(xOrigin, kVerticalPadding)];
  [contentView addSubview:allowButton];

  base::scoped_nsobject<NSView> blockButton;
  blockButton.reset([[self blockButton] retain]);
  CGFloat width =
      [PermissionBubbleController matchWidthsOf:blockButton andOf:allowButton];
  // Ensure the allow/ok button is still in the correct position.
  xOrigin = NSWidth(bubbleFrame) - width - kHorizontalPadding;
  [allowButton setFrameOrigin:NSMakePoint(xOrigin, kVerticalPadding)];
  // Line up the block button.
  xOrigin = NSMinX([allowButton frame]) - width - kBetweenButtonsPadding;
  [blockButton setFrameOrigin:NSMakePoint(xOrigin, kVerticalPadding)];
  [contentView addSubview:blockButton];

  bubbleFrame = [[self window] frameRectForContentRect:bubbleFrame];
  if ([[self window] isVisible]) {
    // Unfortunately, calling -setFrame followed by -setFrameOrigin  (called
    // within -setAnchorPoint) causes flickering.  Avoid the flickering by
    // manually adjusting the new frame's origin so that the top left stays the
    // same, and only calling -setFrame.
    NSRect currentWindowFrame = [[self window] frame];
    bubbleFrame.origin = currentWindowFrame.origin;
    bubbleFrame.origin.y = bubbleFrame.origin.y +
        currentWindowFrame.size.height - bubbleFrame.size.height;
    [[self window] setFrame:bubbleFrame display:YES];
  } else {
    [[self window] setFrame:bubbleFrame display:NO];
    [self setAnchorPoint:[self getExpectedAnchorPoint]];
    [self showWindow:nil];
    [[self window] makeFirstResponder:nil];
    [[self window] setInitialFirstResponder:allowButton.get()];
  }
}

- (void)updateAnchorPosition {
  [self setParentWindow:[self getExpectedParentWindow]];
  [self setAnchorPoint:[self getExpectedAnchorPoint]];
  [[self bubble] setArrowLocation:[self getExpectedArrowLocation]];
}

- (NSPoint)getExpectedAnchorPoint {
  return [PermissionBubbleController getAnchorPointForBrowser:browser_];
}

- (bool)hasVisibleLocationBar {
  return [PermissionBubbleController hasVisibleLocationBarForBrowser:browser_];
}

- (info_bubble::BubbleArrowLocation)getExpectedArrowLocation {
  return info_bubble::kTopLeading;
}

- (NSWindow*)getExpectedParentWindow {
  DCHECK(browser_->window());
  return browser_->window()->GetNativeWindow();
}

- (NSView*)labelForRequest:(PermissionRequest*)request {
  DCHECK(request);
  base::scoped_nsobject<NSView> permissionView(
      [[NSView alloc] initWithFrame:NSZeroRect]);
  base::scoped_nsobject<NSImageView> permissionIcon(
      [[NSImageView alloc] initWithFrame:NSZeroRect]);
  [permissionIcon
      setImage:NSImageFromImageSkia(gfx::CreateVectorIcon(
                   request->GetIconId(), 18, gfx::kChromeIconGrey))];
  [permissionIcon setFrameSize:kPermissionIconSize];
  ui::a11y_util::HideImageFromAccessibilityOrder(permissionIcon);
  [permissionView addSubview:permissionIcon];

  base::scoped_nsobject<NSTextField> permissionLabel(
      [[NSTextField alloc] initWithFrame:NSZeroRect]);
  base::string16 label = request->GetMessageTextFragment();
  [permissionLabel setDrawsBackground:NO];
  [permissionLabel setBezeled:NO];
  [permissionLabel setEditable:NO];
  [permissionLabel setSelectable:NO];
  [permissionLabel
      setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
  [permissionLabel setStringValue:base::SysUTF16ToNSString(label)];
  [permissionLabel sizeToFit];
  [permissionLabel setFrameOrigin:
      NSMakePoint(NSWidth([permissionIcon frame]) + kHorizontalIconPadding, 0)];
  [permissionView addSubview:permissionLabel];

  // Match the horizontal centers of the two subviews.  Note that the label's
  // center is rounded down, and the icon's center, up.  It looks better that
  // way - with the text's center slightly lower than the icon's center - if the
  // height delta is not evenly split.
  NSRect iconFrame = [permissionIcon frame];
  NSRect labelFrame = [permissionLabel frame];
  NSRect unionFrame = NSUnionRect(iconFrame, labelFrame);

  iconFrame.origin.y =
      std::ceil((NSHeight(unionFrame) - NSHeight(iconFrame)) / 2);
  labelFrame.origin.y =
      std::floor((NSHeight(unionFrame) - NSHeight(labelFrame)) / 2);

  [permissionLabel setFrame:labelFrame];
  [permissionIcon setFrame:iconFrame];
  [permissionView setFrame:unionFrame];

  return permissionView.autorelease();
}

- (NSView*)titleWithOrigin:(const GURL&)origin {
  base::scoped_nsobject<NSTextField> titleView(
      [[NSTextField alloc] initWithFrame:NSZeroRect]);
  [titleView setDrawsBackground:NO];
  [titleView setBezeled:NO];
  [titleView setEditable:NO];
  [titleView setSelectable:NO];
  [titleView setStringValue:l10n_util::GetNSStringF(
                                IDS_PERMISSIONS_BUBBLE_PROMPT,
                                url_formatter::FormatUrlForSecurityDisplay(
                                    origin, url_formatter::SchemeDisplay::
                                                OMIT_CRYPTOGRAPHIC))];
  [titleView setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
  [titleView sizeToFit];
  NSRect titleFrame = [titleView frame];
  [titleView setFrameSize:NSMakeSize(NSWidth(titleFrame) + kTitlePaddingX,
                                     NSHeight(titleFrame))];
  return titleView.autorelease();
}

- (NSView*)buttonWithTitle:(NSString*)title
                    action:(SEL)action {
  base::scoped_nsobject<NSButton> button(
      [[ConstrainedWindowButton alloc] initWithFrame:NSZeroRect]);
  [button setButtonType:NSMomentaryPushInButton];
  [button setTitle:title];
  [button setTarget:self];
  [button setAction:action];
  [button sizeToFit];
  return button.autorelease();
}

- (NSView*)blockButton {
  NSString* blockTitle = l10n_util::GetNSString(IDS_PERMISSION_DENY);
  return [self buttonWithTitle:blockTitle
                        action:@selector(onBlock:)];
}

- (NSView*)closeButton {
  int dimension = chrome_style::GetCloseButtonSize();
  NSRect frame = NSMakeRect(0, 0, dimension, dimension);
  base::scoped_nsobject<NSButton> button(
      [[WebUIHoverCloseButton alloc] initWithFrame:frame]);
  [button setAction:@selector(onClose:)];
  [button setTarget:self];
  return button.autorelease();
}

- (void)ok:(id)sender {
  if (delegate_)
    delegate_->Accept();
}

- (void)onAllow:(id)sender {
  if (delegate_)
    delegate_->Accept();
}

- (void)onBlock:(id)sender {
  if (delegate_)
    delegate_->Deny();
}

- (void)onClose:(id)sender {
  if (delegate_)
    delegate_->Closing();
}

- (void)activateTabWithContents:(content::WebContents*)newContents
               previousContents:(content::WebContents*)oldContents
                        atIndex:(NSInteger)index
                         reason:(int)reason {
  // The show/hide of this bubble is handled by the PermissionRequestManager.
  // So bypass the base class, which would close the bubble here.
}

+ (CGFloat)matchWidthsOf:(NSView*)viewA andOf:(NSView*)viewB {
  NSRect frameA = [viewA frame];
  NSRect frameB = [viewB frame];
  CGFloat width = std::max(NSWidth(frameA), NSWidth(frameB));
  [viewA setFrameSize:NSMakeSize(width, NSHeight(frameA))];
  [viewB setFrameSize:NSMakeSize(width, NSHeight(frameB))];
  return width;
}

+ (void)alignCenterOf:(NSView*)viewA verticallyToCenterOf:(NSView*)viewB {
  NSRect frameA = [viewA frame];
  NSRect frameB = [viewB frame];
  frameA.origin.y =
      NSMinY(frameB) + std::floor((NSHeight(frameB) - NSHeight(frameA)) / 2);
  [viewA setFrameOrigin:frameA.origin];
}

- (IBAction)cancel:(id)sender {
  // This is triggered by ESC when the bubble has focus.
  if (delegate_)
    delegate_->Closing();
  [super cancel:sender];
}

@end  // implementation PermissionBubbleController
