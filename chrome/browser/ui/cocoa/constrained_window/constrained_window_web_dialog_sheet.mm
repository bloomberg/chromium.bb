// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_web_dialog_sheet.h"

#include "ui/gfx/geometry/size.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

@implementation WebDialogConstrainedWindowSheet

- (id)initWithCustomWindow:(NSWindow*)customWindow
         webDialogDelegate:(ui::WebDialogDelegate*)delegate {
  if (self = [super initWithCustomWindow:customWindow]) {
    web_dialog_delegate_ = delegate;
  }

  return self;
}

- (void)updateSheetPosition {
  if (web_dialog_delegate_) {
    gfx::Size size;
    web_dialog_delegate_->GetDialogSize(&size);
    [customWindow_ setContentSize:NSMakeSize(size.width(), size.height())];
  }
  [super updateSheetPosition];
}

- (void)resizeWithNewSize:(NSSize)size {
  [customWindow_ setContentSize:size];

  // self's updateSheetPosition() sets |customWindow_|'s contentSize to a
  // fixed dialog size. Here, we want to resize to |size| instead. Use
  // super rather than self to bypass the setContentSize() call for the fixed
  // size.
  [super updateSheetPosition];
}

@end
