// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/card_unmask_prompt_controller.h"
#include "chrome/browser/ui/cocoa/autofill/card_unmask_prompt_view_bridge.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_button.h"
#include "chrome/browser/ui/chrome_style.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_custom_sheet.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_custom_window.h"
#import "chrome/browser/ui/cocoa/key_equivalent_constants.h"
#include "grit/generated_resources.h"
#include "ui/base/cocoa/window_size_constants.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
const CGFloat kButtonGap = 6.0f;
}  // namespace

namespace autofill {

// static
CardUnmaskPromptView* CardUnmaskPromptView::CreateAndShow(
    CardUnmaskPromptController* controller) {
  return new CardUnmaskPromptViewBridge(controller);
}

#pragma mark CardUnmaskPromptViewBridge

CardUnmaskPromptViewBridge::CardUnmaskPromptViewBridge(
    CardUnmaskPromptController* controller)
    : controller_(controller) {
  sheet_controller_.reset([[CardUnmaskPromptViewCocoa alloc]
      initWithWebContents:controller_->GetWebContents()
                   bridge:this]);
  base::scoped_nsobject<CustomConstrainedWindowSheet> sheet(
      [[CustomConstrainedWindowSheet alloc]
          initWithCustomWindow:[sheet_controller_ window]]);
  constrained_window_.reset(
      new ConstrainedWindowMac(this, controller_->GetWebContents(), sheet));
}

CardUnmaskPromptViewBridge::~CardUnmaskPromptViewBridge() {
}

void CardUnmaskPromptViewBridge::ControllerGone() {
}

void CardUnmaskPromptViewBridge::DisableAndWaitForVerification() {
}

void CardUnmaskPromptViewBridge::GotVerificationResult(
    const base::string16& error_message,
    bool allow_retry) {
}

void CardUnmaskPromptViewBridge::OnConstrainedWindowClosed(
    ConstrainedWindowMac* window) {
  constrained_window_.reset();
  controller_->OnUnmaskDialogClosed();
}

void CardUnmaskPromptViewBridge::PerformClose() {
  constrained_window_->CloseWebContentsModalDialog();
}

}  // autofill

#pragma mark CardUnmaskPromptViewCocoa

@implementation CardUnmaskPromptViewCocoa

- (id)initWithWebContents:(content::WebContents*)webContents
                   bridge:(autofill::CardUnmaskPromptViewBridge*)bridge {
  DCHECK(webContents);
  DCHECK(bridge);

  NSRect frame = NSMakeRect(0, 0, 550, 600);
  base::scoped_nsobject<ConstrainedWindowCustomWindow> window(
      [[ConstrainedWindowCustomWindow alloc] initWithContentRect:frame]);
  if ((self = [super initWithWindow:window])) {
    webContents_ = webContents;
    bridge_ = bridge;

    [self buildWindowButtons];
  }
  return self;
}

- (IBAction)closeSheet:(id)sender {
  bridge_->PerformClose();
}

- (void)buildWindowButtons {
  base::scoped_nsobject<NSView> buttonContainer(
      [[NSView alloc] initWithFrame:NSZeroRect]);

  base::scoped_nsobject<NSButton> button(
      [[ConstrainedWindowButton alloc] initWithFrame:NSZeroRect]);
  [button setTitle:l10n_util::GetNSStringWithFixup(IDS_CANCEL)];
  [button setKeyEquivalent:kKeyEquivalentEscape];
  [button setTarget:self];
  [button setAction:@selector(closeSheet:)];
  [button sizeToFit];
  [buttonContainer addSubview:button];

  CGFloat nextX = NSMaxX([button frame]) + kButtonGap;
  button.reset([[ConstrainedWindowButton alloc] initWithFrame:NSZeroRect]);
  [button setFrameOrigin:NSMakePoint(nextX, 0)];
  [button setTitle:l10n_util::GetNSStringWithFixup(
                       IDS_AUTOFILL_DIALOG_SUBMIT_BUTTON)];
  [button setKeyEquivalent:kKeyEquivalentReturn];
  [button setTarget:self];
  [button setAction:@selector(closeSheet:)];
  [button sizeToFit];
  [buttonContainer addSubview:button];

  const CGFloat dialogOffset = NSWidth([[self window] frame]) -
                               chrome_style::kHorizontalPadding -
                               NSMaxX([button frame]);
  [buttonContainer
      setFrame:NSMakeRect(dialogOffset, chrome_style::kClientBottomPadding,
                          NSMaxX([button frame]), NSMaxY([button frame]))];

  [[[self window] contentView] addSubview:buttonContainer];
}

@end
