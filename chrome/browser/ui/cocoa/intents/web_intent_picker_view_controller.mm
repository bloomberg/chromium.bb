// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/intents/web_intent_picker_view_controller.h"

#import "chrome/browser/ui/cocoa/flipped_view.h"
#import "chrome/browser/ui/cocoa/hover_close_button.h"
#import "chrome/browser/ui/cocoa/intents/web_intent_picker_cocoa2.h"
#import "chrome/browser/ui/cocoa/key_equivalent_constants.h"
#include "chrome/browser/ui/constrained_window.h"
#include "chrome/browser/ui/intents/web_intent_picker_delegate.h"
#include "chrome/browser/ui/intents/web_intent_inline_disposition_delegate.h"
#include "ui/base/cocoa/window_size_constants.h"

@interface WebIntentPickerViewController ()

- (void)performLayout;
// Gets the inner frame with a minimum window width and height.
- (NSRect)minimumInnerFrame;

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
  }
  return self;
}

- (NSButton*)closeButton {
  return closeButton_.get();
}

- (void)update {
  [self performLayout];
}

- (void)performLayout {
  NSRect innerFrame = [self minimumInnerFrame];

  NSRect bounds = NSInsetRect(innerFrame,
                              -ConstrainedWindow::kHorizontalPadding,
                              -ConstrainedWindow::kVerticalPadding);

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

- (void)onCloseButton:(id)sender {
  picker_->delegate()->OnUserCancelledPickerDialog();
}

// Handle default OSX dialog cancel mechanisms. (Cmd-.)
- (void)cancelOperation:(id)sender {
  [self onCloseButton:sender];
}

@end
