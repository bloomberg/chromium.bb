// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_ACCOUNT_CHOOSER_H_
#define CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_ACCOUNT_CHOOSER_H_

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"

namespace autofill {
  class AutofillDialogController;
}

@class MenuButton;

@interface AutofillAccountChooser : NSView {
 @private
  scoped_nsobject<NSButton> link_;
  scoped_nsobject<MenuButton> popup_;
  scoped_nsobject<NSImageView> icon_;
  autofill::AutofillDialogController* controller_;  // weak.
}

- (id)initWithFrame:(NSRect)frame
         controller:(autofill::AutofillDialogController*)controller;
- (void)update;

@end

#endif  // CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_ACCOUNT_CHOOSER_H_
