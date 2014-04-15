// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/website_settings/permission_bubble_controller.h"

#include <algorithm>

#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#import "chrome/browser/ui/chrome_style.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_button.h"
#import "chrome/browser/ui/cocoa/hover_close_button.h"
#import "chrome/browser/ui/cocoa/hyperlink_text_view.h"
#import "chrome/browser/ui/cocoa/info_bubble_view.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#include "chrome/browser/ui/cocoa/website_settings/permission_bubble_cocoa.h"
#include "chrome/browser/ui/website_settings/permission_bubble_request.h"
#include "chrome/browser/ui/website_settings/permission_bubble_view.h"
#include "content/public/browser/user_metrics.h"
#include "grit/generated_resources.h"
#include "skia/ext/skia_utils_mac.h"
#include "ui/base/cocoa/window_size_constants.h"
#include "ui/base/l10n/l10n_util_mac.h"

using base::UserMetricsAction;

namespace {
const CGFloat kHorizontalPadding = 20.0f;
const CGFloat kVerticalPadding = 20.0f;
const CGFloat kButtonPadding = 10.0f;
const CGFloat kTitlePaddingX = 50.0f;
const CGFloat kCheckboxYAdjustment = 2.0f;

const CGFloat kFontSize = 15.0f;
}  // namespace

@interface PermissionBubbleController ()

// Returns an autoreleased NSView displaying the icon and label for |request|.
- (NSView*)labelForRequest:(PermissionBubbleRequest*)request;

// Returns an autoreleased NSView displaying the title for the bubble
// requesting settings for |host|.
- (NSView*)titleWithHostname:(const std::string&)host;

// Returns an autoreleased NSView displaying a checkbox for |request|.  The
// checkbox will be initialized as checked if |checked| is YES.
- (NSView*)checkboxForRequest:(PermissionBubbleRequest*)request
                      checked:(BOOL)checked;

// Returns an autoreleased NSView displaying the customize button.
- (NSView*)customizationButton;

// Returns an autoreleased NSView of a button with |title| and |action|.
// If |pairedButton| is non-nil, the size of both buttons will be set to be
// equal to the size of the larger of the two.
- (NSView*)buttonWithTitle:(NSString*)title
                    action:(SEL)action
                pairedWith:(NSView*)pairedButton;

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

// Called when the 'customize' button is pressed.
- (void)onCustomize:(id)sender;

// Called when a checkbox changes from checked to unchecked, or vice versa.
- (void)onCheckboxChanged:(id)sender;


@end

@implementation PermissionBubbleController

- (id)initWithParentWindow:(NSWindow*)parentWindow
                    bridge:(PermissionBubbleCocoa*)bridge {
  DCHECK(parentWindow);
  DCHECK(bridge);
  base::scoped_nsobject<InfoBubbleWindow> window([[InfoBubbleWindow alloc]
      initWithContentRect:ui::kWindowSizeDeterminedLater
                styleMask:NSBorderlessWindowMask
                  backing:NSBackingStoreBuffered
                    defer:NO]);
  [window setAllowedAnimations:info_bubble::kAnimateNone];
  if ((self = [super initWithWindow:window
                       parentWindow:parentWindow
                         anchoredAt:NSZeroPoint])) {
    [self setShouldCloseOnResignKey:NO];
    [[self bubble] setArrowLocation:info_bubble::kTopLeft];
    bridge_ = bridge;
  }
  return self;
}

- (void)windowWillClose:(NSNotification*)notification {
  bridge_->OnBubbleClosing();
  [super windowWillClose:notification];
}

- (void)showAtAnchor:(NSPoint)anchorPoint
         withDelegate:(PermissionBubbleView::Delegate*)delegate
          forRequests:(const std::vector<PermissionBubbleRequest*>&)requests
         acceptStates:(const std::vector<bool>&)acceptStates
    customizationMode:(BOOL)customizationMode {
  DCHECK(!requests.empty());
  DCHECK(delegate);
  DCHECK(!customizationMode || (requests.size() == acceptStates.size()));
  delegate_ = delegate;

  NSView* contentView = [[self window] contentView];
  [contentView setSubviews:@[]];

  // Create one button to use as a guide for the permissions' y-offsets.
  base::scoped_nsobject<NSView> allowOrOkButton;
  if (customizationMode) {
    NSString* okTitle = l10n_util::GetNSString(IDS_OK);
    allowOrOkButton.reset([[self buttonWithTitle:okTitle
                                          action:@selector(ok:)
                                      pairedWith:nil] retain]);
  } else {
    NSString* allowTitle = l10n_util::GetNSString(IDS_PERMISSION_ALLOW);
    allowOrOkButton.reset([[self buttonWithTitle:allowTitle
                                          action:@selector(onAllow:)
                                      pairedWith:nil] retain]);
  }
  CGFloat yOffset = 2 * kVerticalPadding + NSMaxY([allowOrOkButton frame]);
  BOOL singlePermission = requests.size() == 1;

  checkboxes_.reset(customizationMode ? [[NSMutableArray alloc] init] : nil);
  for (auto it = requests.begin(); it != requests.end(); it++) {
    base::scoped_nsobject<NSView> permissionView;
    if (customizationMode) {
      int index = it - requests.begin();
      permissionView.reset(
          [[self checkboxForRequest:(*it)
                            checked:acceptStates[index] ? YES : NO] retain]);
      [base::mac::ObjCCastStrict<NSButton>(permissionView) setTag:index];
      [checkboxes_ addObject:permissionView];
    } else {
      permissionView.reset([[self labelForRequest:(*it)] retain]);
    }
    NSPoint origin = [permissionView frame].origin;
    origin.x += kHorizontalPadding;
    origin.y += yOffset;
    [permissionView setFrameOrigin:origin];
    [contentView addSubview:permissionView];

    yOffset += NSHeight([permissionView frame]);
  }

  // The maximum width of the above permissions will dictate the width of the
  // bubble.  It is calculated here so that it can be used for the positioning
  // of the buttons.
  NSRect bubbleFrame = NSZeroRect;
  for (NSView* view in [contentView subviews]) {
    bubbleFrame = NSUnionRect(
        bubbleFrame, NSInsetRect([view frame], -kHorizontalPadding, 0));
  }

  base::scoped_nsobject<NSView> titleView(
      [[self titleWithHostname:requests[0]->GetRequestingHostname().host()]
          retain]);
  [contentView addSubview:titleView];
  [titleView setFrameOrigin:NSMakePoint(kHorizontalPadding,
                                        kVerticalPadding + yOffset)];
  yOffset += NSHeight([titleView frame]) + kVerticalPadding;

  // The title must fit within the bubble.
  bubbleFrame.size.width = std::max(NSWidth(bubbleFrame),
                                    NSWidth([titleView frame]));

  // 'x' button in the upper-right-hand corner.
  base::scoped_nsobject<NSView> closeButton([[self closeButton] retain]);
  // Place the close button at the rightmost edge of the bubble.
  [closeButton setFrameOrigin:NSMakePoint(
      NSMaxX(bubbleFrame), yOffset - chrome_style::kCloseButtonPadding)];
  // Increase the size of the bubble by the width of the close button and its
  // padding.
  bubbleFrame.size.width +=
      NSWidth([closeButton frame]) + chrome_style::kCloseButtonPadding;
  [contentView addSubview:closeButton];

  // Position the allow/ok button.
  CGFloat xOrigin = NSWidth(bubbleFrame) - NSWidth([allowOrOkButton frame]) -
      kHorizontalPadding;
  [allowOrOkButton setFrameOrigin:NSMakePoint(xOrigin, kVerticalPadding)];
  [contentView addSubview:allowOrOkButton];

  if (!customizationMode) {
    NSString* blockTitle = l10n_util::GetNSString(IDS_PERMISSION_DENY);
    base::scoped_nsobject<NSView> blockButton(
        [[self buttonWithTitle:blockTitle
                        action:@selector(onBlock:)
                    pairedWith:allowOrOkButton] retain]);
    xOrigin = NSMinX([allowOrOkButton frame]) - NSWidth([blockButton frame]) -
        kButtonPadding;
    [blockButton setFrameOrigin:NSMakePoint(xOrigin, kVerticalPadding)];
    [contentView addSubview:blockButton];
  }

  if (!singlePermission && !customizationMode) {
    base::scoped_nsobject<NSView> customizeButton(
        [[self customizationButton] retain]);
    // The Y center should match the Y centers of the buttons.
    CGFloat customizeButtonYOffset = kVerticalPadding +
      std::ceil(0.5f * (NSHeight([allowOrOkButton frame]) -
                        NSHeight([customizeButton frame])));
    [customizeButton setFrameOrigin:NSMakePoint(kHorizontalPadding,
                                                customizeButtonYOffset)];
    [contentView addSubview:customizeButton];
  }

  bubbleFrame.size.height = yOffset + kVerticalPadding;
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
    [self setAnchorPoint:anchorPoint];
    [self showWindow:nil];
  }
}

- (NSView*)labelForRequest:(PermissionBubbleRequest*)request {
  DCHECK(request);
  base::scoped_nsobject<NSView> permissionView(
      [[NSView alloc] initWithFrame:NSZeroRect]);
  base::scoped_nsobject<NSImageView> permissionIcon(
      [[NSImageView alloc] initWithFrame:NSZeroRect]);
  [permissionIcon setImage:ui::ResourceBundle::GetSharedInstance().
      GetNativeImageNamed(request->GetIconID()).ToNSImage()];
  [permissionIcon setFrameSize:[[permissionIcon image] size]];
  [permissionView addSubview:permissionIcon];

  base::scoped_nsobject<NSTextField> permissionLabel(
      [[NSTextField alloc] initWithFrame:NSZeroRect]);
  base::string16 label = request->GetMessageTextFragment();
  [permissionLabel setDrawsBackground:NO];
  [permissionLabel setBezeled:NO];
  [permissionLabel setEditable:NO];
  [permissionLabel setSelectable:NO];
  [permissionLabel setStringValue:base::SysUTF16ToNSString(label)];
  [permissionLabel sizeToFit];
  [permissionLabel setFrameOrigin:
      NSMakePoint(NSWidth([permissionIcon frame]), 0)];
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

- (NSView*)titleWithHostname:(const std::string&)host {
  base::scoped_nsobject<NSTextField> titleView(
      [[NSTextField alloc] initWithFrame:NSZeroRect]);
  [titleView setDrawsBackground:NO];
  [titleView setBezeled:NO];
  [titleView setEditable:NO];
  [titleView setSelectable:NO];
  [titleView setStringValue:
      l10n_util::GetNSStringF(IDS_PERMISSIONS_BUBBLE_PROMPT,
                              base::UTF8ToUTF16(host))];
  [titleView setFont:[NSFont systemFontOfSize:kFontSize]];
  [titleView sizeToFit];
  NSRect titleFrame = [titleView frame];
  [titleView setFrameSize:NSMakeSize(NSWidth(titleFrame) + kTitlePaddingX,
                                     NSHeight(titleFrame))];
  return titleView.autorelease();
}

- (NSView*)checkboxForRequest:(PermissionBubbleRequest*)request
                      checked:(BOOL)checked {
  DCHECK(request);
  base::scoped_nsobject<NSButton> checkbox(
      [[NSButton alloc] initWithFrame:NSZeroRect]);
  [checkbox setButtonType:NSSwitchButton];
  base::string16 permission = request->GetMessageTextFragment();
  [checkbox setTitle:base::SysUTF16ToNSString(permission)];
  [checkbox setState:(checked ? NSOnState : NSOffState)];
  [checkbox setTarget:self];
  [checkbox setAction:@selector(onCheckboxChanged:)];
  [checkbox sizeToFit];
  [checkbox setFrameOrigin:NSMakePoint(0, kCheckboxYAdjustment)];
  return checkbox.autorelease();
}

- (NSView*)customizationButton {
  NSColor* linkColor =
      gfx::SkColorToCalibratedNSColor(chrome_style::GetLinkColor());
  base::scoped_nsobject<NSButton> customizeButton(
      [[NSButton alloc] initWithFrame:NSZeroRect]);
  [customizeButton setButtonType:NSMomentaryChangeButton];
  [customizeButton setAttributedTitle:[[NSAttributedString alloc]
      initWithString:l10n_util::GetNSString(IDS_PERMISSION_CUSTOMIZE)
          attributes:@{ NSForegroundColorAttributeName : linkColor }]];
  [customizeButton setTarget:self];
  [customizeButton setAction:@selector(onCustomize:)];
  [customizeButton setBordered:NO];
  [customizeButton sizeToFit];
  return customizeButton.autorelease();
}

- (NSView*)buttonWithTitle:(NSString*)title
                    action:(SEL)action
                pairedWith:(NSView*)pairedButton {
  base::scoped_nsobject<NSButton> button(
      [[ConstrainedWindowButton alloc] initWithFrame:NSZeroRect]);
  [button setButtonType:NSMomentaryPushInButton];
  [button setTitle:title];
  [button setTarget:self];
  [button setAction:action];
  [button sizeToFit];
  if (pairedButton) {
    NSRect buttonFrame = [button frame];
    NSRect pairedFrame = [pairedButton frame];
    CGFloat width = std::max(NSWidth(buttonFrame), NSWidth(pairedFrame));
    [button setFrameSize:NSMakeSize(width, buttonFrame.size.height)];
    [pairedButton setFrameSize:NSMakeSize(width, pairedFrame.size.height)];
  }
  return button.autorelease();
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
  DCHECK(delegate_);
  delegate_->Closing();
}

- (void)onAllow:(id)sender {
  DCHECK(delegate_);
  delegate_->Accept();
}

- (void)onBlock:(id)sender {
  DCHECK(delegate_);
  delegate_->Deny();
}

- (void)onClose:(id)sender {
  DCHECK(delegate_);
  delegate_->Closing();
}

- (void)onCustomize:(id)sender {
  DCHECK(delegate_);
  delegate_->SetCustomizationMode();
}

- (void)onCheckboxChanged:(id)sender {
  DCHECK(delegate_);
  NSButton* checkbox = base::mac::ObjCCastStrict<NSButton>(sender);
  delegate_->ToggleAccept([checkbox tag], [checkbox state] == NSOnState);
}

@end  // implementation PermissionBubbleController
