// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_ADDRESS_SHEET_CONTROLLER_MAC_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_ADDRESS_SHEET_CONTROLLER_MAC_
#pragma once

#import <Cocoa/Cocoa.h>

@class AutoFillAddressModel;
class AutoFillProfile;

// The sheet can be invoked in "Add" or "Edit" mode.  This dictates the caption
// seen at the top of the sheet.
enum {
  kAutoFillAddressAddMode    =  0,
  kAutoFillAddressEditMode   =  1
};
typedef NSInteger AutoFillAddressMode;

// A class that coordinates the |addressModel| and the associated view
// held in AutoFillAddressSheet.xib.
// |initWithProfile:| is the designated initializer.  It takes |profile|
// and transcribes it to |addressModel| to which the view is bound.
@interface AutoFillAddressSheetController : NSWindowController {
 @private
  // The caption at top of dialog.  Text changes according to usage.  Either
  // "New address" or "Edit address" depending on |mode_|.
  IBOutlet NSTextField* caption_;

  // The primary model for this controller.  The model is instantiated
  // from within |initWithProfile:|.  We do not hold it as a scoped_nsobject
  // because it is exposed as a KVO compliant property.
  // Strong reference.
  AutoFillAddressModel* addressModel_;

  // Either "Add" or "Edit" mode of sheet.
  AutoFillAddressMode mode_;
}

@property(nonatomic, retain) AutoFillAddressModel* addressModel;

// IBActions for save and cancel buttons.  Both invoke |endSheet:|.
- (IBAction)save:(id)sender;
- (IBAction)cancel:(id)sender;

// Designated initializer.  Takes a copy of the data in |profile|,
// it is not held as a reference.
- (id)initWithProfile:(const AutoFillProfile&)profile
                 mode:(AutoFillAddressMode)mode;

// Copy data from internal model to |profile|.
- (void)copyModelToProfile:(AutoFillProfile*)profile;

@end

#endif // CHROME_BROWSER_AUTOFILL_AUTOFILL_ADDRESS_SHEET_CONTROLLER_MAC_
