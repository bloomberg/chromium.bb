// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_MAIN_CONTAINER_H_
#define CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_MAIN_CONTAINER_H_

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"

@class AutofillAccountChooser;
@class AutofillDialogWindowController;
@class GTMWidthBasedTweaker;

namespace autofill {
  class AutofillDialogController;
}

@interface AutofillMainContainer : NSViewController {
 @private
  scoped_nsobject<AutofillAccountChooser> accountChooser_;
  scoped_nsobject<GTMWidthBasedTweaker> buttonContainer_;
  AutofillDialogWindowController* target_;
  autofill::AutofillDialogController* controller_;  // Not owned.
}

@property(assign, nonatomic) AutofillDialogWindowController* target;

- (id)initWithController:(autofill::AutofillDialogController*)controller;
- (AutofillAccountChooser*)accountChooser;

@end

#endif  // CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_MAIN_CONTAINER_H_
