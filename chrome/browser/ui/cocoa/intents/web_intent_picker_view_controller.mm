// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/intents/web_intent_picker_view_controller.h"

#import "chrome/browser/ui/cocoa/flipped_view.h"
#import "chrome/browser/ui/cocoa/hover_close_button.h"
#import "chrome/browser/ui/cocoa/intents/web_intent_message_view_controller.h"
#import "chrome/browser/ui/cocoa/intents/web_intent_picker_cocoa2.h"
#import "chrome/browser/ui/cocoa/key_equivalent_constants.h"
#include "chrome/browser/ui/constrained_window.h"
#include "chrome/browser/ui/intents/web_intent_picker_delegate.h"
#include "chrome/browser/ui/intents/web_intent_inline_disposition_delegate.h"
#include "grit/generated_resources.h"
#include "ui/base/cocoa/window_size_constants.h"
#include "ui/base/l10n/l10n_util_mac.h"

@interface WebIntentPickerViewController ()

- (void)performLayout;
// Gets the inner frame with a minimum window width and height.
- (NSRect)minimumInnerFrame;
// Gets the view controller currently being displayed.
- (NSViewController<WebIntentViewController>*)currentViewController;
- (WebIntentPickerState)newPickerState;

// Update the various views to match changes to the picker model.
- (void)updateNoService;

- (void)onCloseButton:(id)sender;
- (void)cancelOperation:(id)sender;

@end

@implementation WebIntentPickerViewController

- (id)initWithPicker:(WebIntentPickerCocoa2*)picker {
  if ((self = [super init])) {
    picker_ = picker;

    scoped_nsobject<NSView> view(
        [[FlippedView alloc] initWithFrame:ui::kWindowSizeDeterminedLater]);
    [self setView:view];

    closeButton_.reset([[HoverCloseButton alloc] initWithFrame:NSZeroRect]);
    [closeButton_ setTarget:self];
    [closeButton_ setAction:@selector(onCloseButton:)];
    [[closeButton_ cell] setKeyEquivalent:kKeyEquivalentEscape];
    [[self view] addSubview:closeButton_];

    messageViewController_.reset(
        [[WebIntentMessageViewController alloc] init]);

  }
  return self;
}

- (NSButton*)closeButton {
  return closeButton_.get();
}

- (WebIntentPickerState)state {
  return state_;
}

- (WebIntentMessageViewController*)messageViewController {
  return messageViewController_;
}

- (void)update {
  WebIntentPickerState newState = [self newPickerState];
  NSView* currentView = [[self currentViewController] view];
  if (state_ != newState || ![currentView superview]) {
    [currentView removeFromSuperview];
    state_ = newState;
    currentView = [[self currentViewController] view];
    [[self view] addSubview:currentView];

    // Ensure that the close button is topmost.
    [closeButton_ removeFromSuperview];
    [[self view] addSubview:closeButton_];
  }

  switch (state_) {
    case PICKER_STATE_NO_SERVICE:
      [self updateNoService];
      break;
  }

  [self performLayout];
}

- (void)performLayout {
  NSRect innerFrame = [self minimumInnerFrame];
  NSViewController<WebIntentViewController>* viewController =
      [self currentViewController];
  NSSize minSize =
      [viewController minimumSizeForInnerWidth:NSWidth(innerFrame)];
  innerFrame.size.width = std::max(minSize.width, NSWidth(innerFrame));
  innerFrame.size.width = std::min(
      static_cast<CGFloat>(WebIntentPicker::kWindowMaxWidth),
      NSWidth(innerFrame));
  innerFrame.size.height = std::max(minSize.height, NSHeight(innerFrame));

  NSRect bounds = NSInsetRect(innerFrame,
                              -ConstrainedWindow::kHorizontalPadding,
                              -ConstrainedWindow::kVerticalPadding);

  [[viewController view] setFrame:bounds];
  [viewController layoutSubviewsWithinFrame:innerFrame];

  NSRect closeFrame;
  closeFrame.size.width = ConstrainedWindow::GetCloseButtonSize();
  closeFrame.size.height = ConstrainedWindow::GetCloseButtonSize();
  closeFrame.origin.x = NSMaxX(innerFrame) - NSWidth(closeFrame);
  closeFrame.origin.y = NSMinY(innerFrame);
  [closeButton_ setFrame:closeFrame];

  [[self view] setFrame:bounds];
}

- (NSRect)minimumInnerFrame {
  NSRect bounds = NSMakeRect(0, 0, WebIntentPicker::kWindowMinWidth,
                             WebIntentPicker::kWindowMinHeight);
  return NSInsetRect(bounds,
                     ConstrainedWindow::kHorizontalPadding,
                     ConstrainedWindow::kVerticalPadding);
}

- (NSViewController<WebIntentViewController>*)currentViewController {
  switch (state_) {
    case PICKER_STATE_NO_SERVICE:
      return messageViewController_;
  }
  return nil;
}

- (WebIntentPickerState)newPickerState {
  return PICKER_STATE_NO_SERVICE;
}

- (void)updateNoService {
  [messageViewController_ setTitle:l10n_util::GetNSStringWithFixup(
      IDS_INTENT_PICKER_NO_SERVICES_TITLE)];
  [messageViewController_ setMessage:l10n_util::GetNSStringWithFixup(
      IDS_INTENT_PICKER_NO_SERVICES)];
}

- (void)onCloseButton:(id)sender {
  picker_->delegate()->OnUserCancelledPickerDialog();
}

// Handle default OSX dialog cancel mechanisms. (Cmd-.)
- (void)cancelOperation:(id)sender {
  [self onCloseButton:sender];
}

@end
