// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/autofill_header.h"

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
    [self setView:view];
  }
  return self;
}

- (void)update {
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
  [title_ setFrameOrigin:NSMakePoint(chrome_style::kHorizontalPadding,
                                     autofill::kDetailVerticalPadding)];
}

@end
