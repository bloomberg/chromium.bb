// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/autofill_header.h"

#import "chrome/browser/ui/cocoa/autofill/autofill_account_chooser.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/ui/autofill/autofill_dialog_view_delegate.h"
#include "chrome/browser/ui/chrome_style.h"
#include "chrome/browser/ui/cocoa/autofill/autofill_dialog_constants.h"

namespace {

// Height of the account chooser.
const CGFloat kAccountChooserHeight = 20.0;

}  // namespace

@implementation AutofillHeader

- (id)initWithDelegate:(autofill::AutofillDialogViewDelegate*)delegate {
  if (self = [super initWithNibName:nil bundle:nil]) {
    delegate_ = delegate;

    accountChooser_.reset(
        [[AutofillAccountChooser alloc] initWithFrame:NSZeroRect
                                             delegate:delegate]);

    // Set dialog title.
    title_.reset([[NSTextField alloc] initWithFrame:NSZeroRect]);
    [title_ setEditable:NO];
    [title_ setBordered:NO];
    [title_ setDrawsBackground:NO];
    [title_ setFont:[NSFont systemFontOfSize:15.0]];
    [title_ setStringValue:base::SysUTF16ToNSString(delegate_->DialogTitle())];
    [title_ sizeToFit];

    base::scoped_nsobject<NSView> view(
        [[NSView alloc] initWithFrame:NSZeroRect]);
    [view setSubviews:@[accountChooser_, title_]];
    [self setView:view];
  }
  return self;
}

- (NSView*)anchorView {
  return [[accountChooser_ subviews] objectAtIndex:1];
}

- (void)update {
  [accountChooser_ update];

  [title_ setStringValue:base::SysUTF16ToNSString(delegate_->DialogTitle())];
  [title_ sizeToFit];
}

- (NSSize)preferredSize {
  CGFloat height =
      chrome_style::kTitleTopPadding +
      kAccountChooserHeight +
      autofill::kDetailVerticalPadding;

  // The returned width is unused, and there's no simple way to compute the
  // account chooser's width, so just return 0 for the width.
  return NSMakeSize(0, height);
}

- (void)performLayout {
  NSRect bounds = [[self view] bounds];

  [title_ setFrameOrigin:NSMakePoint(chrome_style::kHorizontalPadding,
                                     autofill::kDetailVerticalPadding)];

  CGFloat accountChooserLeftX =
      NSMaxX([title_ frame]) + chrome_style::kHorizontalPadding;
  CGFloat accountChooserWidth =
      NSMaxX(bounds) - chrome_style::kHorizontalPadding - accountChooserLeftX;
  NSRect accountChooserFrame =
      NSMakeRect(accountChooserLeftX, autofill::kDetailVerticalPadding,
                 accountChooserWidth, kAccountChooserHeight);
  [accountChooser_ setFrame:accountChooserFrame];
  [accountChooser_ performLayout];
}

@end
