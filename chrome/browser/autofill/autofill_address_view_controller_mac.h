// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_ADDRESS_VIEW_CONTROLLER_MAC_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_ADDRESS_VIEW_CONTROLLER_MAC_

#import <Cocoa/Cocoa.h>
#import "chrome/browser/cocoa/disclosure_view_controller.h"

@class AutoFillAddressModel;
@class AutoFillDialogController;
class AutoFillProfile;

// A class that coordinates the |addressModel| and the associated view
// held in AutoFillAddressFormView.xib.
// |initWithProfile:| is the designated initializer.  It takes |profile|
// and transcribes it to |addressModel| to which the view is bound.
@interface AutoFillAddressViewController : DisclosureViewController {
 @private
  // The primary model for this controller.  The model is instantiated
  // from within |initWithProfile:|.  We do not hold it as a scoped_nsobject
  // because it is exposed as a KVO compliant property.
  // Strong reference.
  AutoFillAddressModel* addressModel_;

  // A reference to our parent controller.  Used for notifying parent if/when
  // deletion occurs.  Also used to notify parent when the label of the address
  // changes.  May be not be nil.
  // Weak reference, owns us.
  AutoFillDialogController* parentController_;
}

@property (readonly) BOOL canAlterDefault;
@property BOOL isDefault;
@property (nonatomic, retain) AutoFillAddressModel* addressModel;

// Designated initializer.  Takes a copy of the data in |profile|,
// it is not held as a reference.
- (id)initWithProfile:(const AutoFillProfile&)profile
           disclosure:(NSCellStateValue)disclosureState
           controller:(AutoFillDialogController*) parentController;

// Action to remove this address from the dialog.  Forwards the request to
// |parentController_| which does all the actual work.  We have the action
// here so that the delete button in the AutoFillAddressViewFormView.xib has
// something to call.
- (IBAction)deleteAddress:(id)sender;

// Copy data from internal model to |profile|.
- (void)copyModelToProfile:(AutoFillProfile*)profile;

@end

#endif // CHROME_BROWSER_AUTOFILL_AUTOFILL_ADDRESS_VIEW_CONTROLLER_MAC_
