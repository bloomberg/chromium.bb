// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/autofill_main_container.h"

#include <algorithm>
#include <cmath>

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/ui/autofill/autofill_dialog_view_delegate.h"
#include "chrome/browser/ui/chrome_style.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_details_container.h"
#include "chrome/browser/ui/cocoa/autofill/autofill_dialog_constants.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_notification_container.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_tooltip_controller.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_button.h"
#import "chrome/browser/ui/cocoa/key_equivalent_constants.h"
#include "grit/theme_resources.h"
#include "skia/ext/skia_utils_mac.h"
#import "third_party/google_toolbox_for_mac/src/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#import "ui/base/cocoa/controls/blue_label_button.h"
#import "ui/base/cocoa/controls/hyperlink_text_view.h"
#include "ui/base/cocoa/window_size_constants.h"
#include "ui/gfx/range/range.h"

namespace {

// Padding between buttons and the last suggestion or details view. The mock
// has a total of 30px - but 10px are already provided by details/suggestions.
const CGFloat kButtonVerticalPadding = 20.0;

}  // namespace

@interface AutofillMainContainer (Private)
- (void)buildWindowButtons;
- (void)layoutButtons;
- (void)updateButtons;
@end


@implementation AutofillMainContainer

@synthesize target = target_;

- (id)initWithDelegate:(autofill::AutofillDialogViewDelegate*)delegate {
  if (self = [super init]) {
    delegate_ = delegate;
  }
  return self;
}

- (void)loadView {
  [self buildWindowButtons];

  base::scoped_nsobject<NSView> view([[NSView alloc] initWithFrame:NSZeroRect]);
  [view setAutoresizesSubviews:YES];
  [view setSubviews:@[buttonContainer_]];
  [self setView:view];

  // Set up "Save in Chrome" checkbox.
  saveInChromeCheckbox_.reset([[NSButton alloc] initWithFrame:NSZeroRect]);
  [saveInChromeCheckbox_ setButtonType:NSSwitchButton];
  [saveInChromeCheckbox_ setTitle:
      base::SysUTF16ToNSString(delegate_->SaveLocallyText())];
  [saveInChromeCheckbox_ sizeToFit];
  [[self view] addSubview:saveInChromeCheckbox_];

  saveInChromeTooltip_.reset(
      [[AutofillTooltipController alloc]
            initWithArrowLocation:info_bubble::kTopCenter]);
  [saveInChromeTooltip_ setImage:
      ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
          IDR_AUTOFILL_TOOLTIP_ICON).ToNSImage()];
  [saveInChromeTooltip_ setMessage:
      base::SysUTF16ToNSString(delegate_->SaveLocallyTooltip())];
  [[self view] addSubview:[saveInChromeTooltip_ view]];
  [self updateSaveInChrome];

  detailsContainer_.reset(
      [[AutofillDetailsContainer alloc] initWithDelegate:delegate_]);
  NSSize frameSize = [[detailsContainer_ view] frame].size;
  [[detailsContainer_ view] setFrameOrigin:
      NSMakePoint(0, NSHeight([buttonContainer_ frame]))];
  frameSize.height += NSHeight([buttonContainer_ frame]);
  [[self view] setFrameSize:frameSize];
  [[self view] addSubview:[detailsContainer_ view]];

  notificationContainer_.reset(
      [[AutofillNotificationContainer alloc] initWithDelegate:delegate_]);
  [[self view] addSubview:[notificationContainer_ view]];
}

- (NSSize)decorationSizeForWidth:(CGFloat)width {
  NSSize buttonSize = [buttonContainer_ frame].size;
  NSSize buttonStripSize =
      NSMakeSize(buttonSize.width + chrome_style::kHorizontalPadding,
                 buttonSize.height + kButtonVerticalPadding +
                     chrome_style::kClientBottomPadding);

  NSSize size = NSMakeSize(std::max(buttonStripSize.width, width),
                           buttonStripSize.height);
  NSSize notificationSize =
      [notificationContainer_ preferredSizeForWidth:width];
  size.height += notificationSize.height;

  return size;
}

- (NSSize)preferredSize {
  NSSize detailsSize = [detailsContainer_ preferredSize];
  NSSize decorationSize = [self decorationSizeForWidth:detailsSize.width];

  NSSize size = NSMakeSize(std::max(decorationSize.width, detailsSize.width),
                           decorationSize.height + detailsSize.height);
  size.height += autofill::kDetailVerticalPadding;

  return size;
}

- (void)performLayout {
  NSRect bounds = [[self view] bounds];

  CGFloat currentY = 0.0;
  NSRect buttonFrame = [buttonContainer_ frame];
  buttonFrame.origin.y = currentY + chrome_style::kClientBottomPadding;
  [buttonContainer_ setFrameOrigin:buttonFrame.origin];
  currentY = NSMaxY(buttonFrame) + kButtonVerticalPadding;

  NSRect checkboxFrame = [saveInChromeCheckbox_ frame];
  [saveInChromeCheckbox_ setFrameOrigin:
      NSMakePoint(chrome_style::kHorizontalPadding,
                  NSMidY(buttonFrame) - NSHeight(checkboxFrame) / 2.0)];

  NSRect tooltipFrame = [[saveInChromeTooltip_ view] frame];
  [[saveInChromeTooltip_ view] setFrameOrigin:
      NSMakePoint(NSMaxX([saveInChromeCheckbox_ frame]) + autofill::kButtonGap,
                  NSMidY(buttonFrame) - (NSHeight(tooltipFrame) / 2.0))];

  NSRect notificationFrame = NSZeroRect;
  notificationFrame.size = [notificationContainer_ preferredSizeForWidth:
      NSWidth(bounds)];

  // Buttons/checkbox/legal take up lower part of view, notifications the
  // upper part. Adjust the detailsContainer to take up the remainder.
  CGFloat remainingHeight =
      NSHeight(bounds) - currentY - NSHeight(notificationFrame);
  NSRect containerFrame =
      NSMakeRect(0, currentY, NSWidth(bounds), remainingHeight);
  [[detailsContainer_ view] setFrame:containerFrame];
  [detailsContainer_ performLayout];

  notificationFrame.origin = NSMakePoint(0, NSMaxY(containerFrame));
  [[notificationContainer_ view] setFrame:notificationFrame];
  [notificationContainer_ performLayout];
}

- (void)buildWindowButtons {
  if (buttonContainer_.get())
    return;

  buttonContainer_.reset([[GTMWidthBasedTweaker alloc] initWithFrame:
      ui::kWindowSizeDeterminedLater]);
  [buttonContainer_
      setAutoresizingMask:(NSViewMinXMargin | NSViewMaxYMargin)];

  base::scoped_nsobject<NSButton> button(
      [[ConstrainedWindowButton alloc] initWithFrame:NSZeroRect]);
  [button setKeyEquivalent:kKeyEquivalentEscape];
  [button setTarget:target_];
  [button setAction:@selector(cancel:)];
  [button sizeToFit];
  [buttonContainer_ addSubview:button];

  CGFloat nextX = NSMaxX([button frame]) + autofill::kButtonGap;
  button.reset([[BlueLabelButton alloc] initWithFrame:NSZeroRect]);
  [button setFrameOrigin:NSMakePoint(nextX, 0)];
  [button setKeyEquivalent:kKeyEquivalentReturn];
  [button setTarget:target_];
  [button setAction:@selector(accept:)];
  [buttonContainer_ addSubview:button];
  [self updateButtons];

  NSRect frame = NSMakeRect(
      -NSMaxX([button frame]) - chrome_style::kHorizontalPadding, 0,
      NSMaxX([button frame]), NSHeight([button frame]));
  [buttonContainer_ setFrame:frame];
}

- (void)layoutButtons {
  base::scoped_nsobject<GTMUILocalizerAndLayoutTweaker> layoutTweaker(
      [[GTMUILocalizerAndLayoutTweaker alloc] init]);
  [layoutTweaker tweakUI:buttonContainer_];

  // Now ensure both buttons have the same height. The second button is
  // known to be the larger one.
  CGFloat buttonHeight =
      NSHeight([[[buttonContainer_ subviews] objectAtIndex:1] frame]);

  // Force first button to be the same height.
  NSView* button = [[buttonContainer_ subviews] objectAtIndex:0];
  NSSize buttonSize = [button frame].size;
  buttonSize.height = buttonHeight;
  [button setFrameSize:buttonSize];
}

- (void)updateButtons {
  NSButton* button = base::mac::ObjCCastStrict<NSButton>(
      [[buttonContainer_ subviews] objectAtIndex:0]);
  [button setTitle:base::SysUTF16ToNSString(delegate_->CancelButtonText())];
  button = base::mac::ObjCCastStrict<NSButton>(
    [[buttonContainer_ subviews] objectAtIndex:1]);
  [button setTitle:base::SysUTF16ToNSString(delegate_->ConfirmButtonText())];
  [self layoutButtons];
}

- (AutofillSectionContainer*)sectionForId:(autofill::DialogSection)section {
  return [detailsContainer_ sectionForId:section];
}

- (void)modelChanged {
  [self updateSaveInChrome];
  [self updateButtons];
  [detailsContainer_ modelChanged];
}

- (BOOL)saveDetailsLocally {
  return [saveInChromeCheckbox_ state] == NSOnState;
}

- (void)updateNotificationArea {
  [notificationContainer_ setNotifications:delegate_->CurrentNotifications()];
  id delegate = [[[self view] window] windowController];
  if ([delegate respondsToSelector:@selector(requestRelayout)])
    [delegate performSelector:@selector(requestRelayout)];
}

- (BOOL)validate {
  return [detailsContainer_ validate];
}

- (void)updateSaveInChrome {
  [saveInChromeCheckbox_ setHidden:!delegate_->ShouldOfferToSaveInChrome()];
  [[saveInChromeTooltip_ view] setHidden:[saveInChromeCheckbox_ isHidden]];
  [saveInChromeCheckbox_ setState:
      (delegate_->ShouldSaveInChrome() ? NSOnState : NSOffState)];
}

- (void)makeFirstInvalidInputFirstResponder {
  NSView* field = [detailsContainer_ firstInvalidField];
  if (!field)
    return;

  [detailsContainer_ scrollToView:field];
  [[[self view] window] makeFirstResponder:field];
}

- (void)scrollInitialEditorIntoViewAndMakeFirstResponder {
  // Try to focus on the first invalid field. If there isn't one, focus on the
  // first editable field instead.
  NSView* field = [detailsContainer_ firstInvalidField];
  if (!field)
    field = [detailsContainer_ firstVisibleField];
  if (!field)
    return;

  [detailsContainer_ scrollToView:field];
  [[[self view] window] makeFirstResponder:field];
}

- (void)updateErrorBubble {
  [detailsContainer_ updateErrorBubble];
}

@end


@implementation AutofillMainContainer (Testing)

- (NSButton*)saveInChromeCheckboxForTesting {
  return saveInChromeCheckbox_.get();
}

- (NSButton*)saveInChromeTooltipForTesting {
  return base::mac::ObjCCast<NSButton>([saveInChromeTooltip_ view]);
}

@end
