// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/website_settings/permission_bubble_controller.h"

#include <algorithm>

#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#import "chrome/browser/ui/chrome_style.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_button.h"
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
const CGFloat kCheckboxYAdjustment = 2.0f;

const CGFloat kFontSize = 15.0f;
const base::char16 kBulletPoint = 0x2022;

}  // namespace

@interface PermissionBubbleController ()

// Called when the 'ok' button is pressed.
- (void)ok:(id)sender;

// Called when the 'allow' button is pressed.
- (void)onAllow:(id)sender;

// Called when the 'block' button is pressed.
- (void)onBlock:(id)sender;

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
  DCHECK(delegate);
  DCHECK(!customizationMode || (requests.size() == acceptStates.size()));
  delegate_ = delegate;

  NSView* contentView = [[self window] contentView];
  DCHECK([[contentView subviews] count] == 0);

  // Create one button to use as a guide for the permissions' y-offsets.
  base::scoped_nsobject<NSView> allowOrOkButton;
  if (customizationMode) {
    allowOrOkButton.reset([[self buttonWithTitle:@"OK"
                                          action:@selector(ok:)
                                      pairedWith:nil] retain]);
  } else {
    allowOrOkButton.reset([[self buttonWithTitle:@"Allow"
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
      permissionView.reset([[self labelForRequest:(*it)
                                  isSingleRequest:singlePermission] retain]);
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

  // Position the allow/ok button.
  CGFloat xOrigin = NSWidth(bubbleFrame) - NSWidth([allowOrOkButton frame]) -
      kHorizontalPadding;
  [allowOrOkButton setFrameOrigin:NSMakePoint(xOrigin, kVerticalPadding)];
  [contentView addSubview:allowOrOkButton];

  if (!customizationMode) {
    base::scoped_nsobject<NSView> blockButton(
        [[self buttonWithTitle:@"Block"
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

  if (!singlePermission) {
    base::scoped_nsobject<NSView> titleView(
       [[self titleForMultipleRequests] retain]);
    [contentView addSubview:titleView];
    [titleView setFrameOrigin:NSMakePoint(kHorizontalPadding,
                                          kVerticalPadding + yOffset)];
    yOffset += NSHeight([titleView frame]) + kVerticalPadding;
  }

  bubbleFrame.size.height = yOffset + kVerticalPadding;
  bubbleFrame = [[self window] frameRectForContentRect:bubbleFrame];
  [[self window] setFrame:bubbleFrame display:NO];

  [self setAnchorPoint:anchorPoint];
  [self showWindow:nil];
}

- (NSView*)labelForRequest:(PermissionBubbleRequest*)request
           isSingleRequest:(BOOL)singleRequest {
  DCHECK(request);
  base::scoped_nsobject<NSTextField> permissionLabel(
      [[NSTextField alloc] initWithFrame:NSZeroRect]);
  base::string16 label;
  if (!singleRequest) {
    label.push_back(kBulletPoint);
    label.push_back(' ');
  }
  if (singleRequest) {
    // TODO(leng):  Make the appropriate call when it's working.  It should call
    // GetMessageText(), but it's not returning the correct string yet.
    label += request->GetMessageTextFragment();
  } else {
    label += request->GetMessageTextFragment();
  }
  [permissionLabel setDrawsBackground:NO];
  [permissionLabel setBezeled:NO];
  [permissionLabel setEditable:NO];
  [permissionLabel setSelectable:NO];
  [permissionLabel setStringValue:base::SysUTF16ToNSString(label)];
  [permissionLabel sizeToFit];
  return permissionLabel.autorelease();
}

- (NSView*)titleForMultipleRequests {
  base::scoped_nsobject<NSTextField> titleView(
      [[NSTextField alloc] initWithFrame:NSZeroRect]);
  [titleView setDrawsBackground:NO];
  [titleView setBezeled:NO];
  [titleView setEditable:NO];
  [titleView setSelectable:NO];
  [titleView setStringValue:@"This site would like to:"];
  [titleView setFont:[NSFont systemFontOfSize:kFontSize]];
  [titleView sizeToFit];
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
      initWithString:@"Customize"
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
