// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_DIALOG_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_DIALOG_WINDOW_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/ui/autofill/autofill_dialog_types.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_layout.h"

@class AutofillHeader;
@class AutofillLoadingShieldController;
@class AutofillMainContainer;
@class AutofillOverlayController;
@class AutofillSignInContainer;

namespace content {
class NavigationController;
class WebContents;
}  // content

namespace autofill {
class AutofillDialogCocoa;
}  // autofill


// Forwarding AutofillDialogView calls.
@protocol AutofillDialogBridge

- (void)show;
- (void)hide;
- (void)updateNotificationArea;
- (void)updateAccountChooser;
- (void)updateButtonStrip;
- (void)updateSection:(autofill::DialogSection)section;
- (void)updateForErrors;
- (void)fillSection:(autofill::DialogSection)section
           forType:(const autofill::ServerFieldType)type;
- (void)getInputs:(autofill::FieldValueMap*)outputs
       forSection:(autofill::DialogSection)section;
- (NSString*)getCvc;
- (BOOL)saveDetailsLocally;
- (content::NavigationController*)showSignIn;
- (void)hideSignIn;
- (void)modelChanged;
- (void)updateErrorBubble;
- (void)onSignInResize:(NSSize)size;
- (void)validateSection:(autofill::DialogSection)section;

@end


// Window controller for AutofillDialogView.
@interface AutofillDialogWindowController :
    NSWindowController<NSWindowDelegate, AutofillLayout, AutofillDialogBridge> {
 @private
  content::WebContents* webContents_;  // weak.
  autofill::AutofillDialogCocoa* dialog_;  // weak.

  base::scoped_nsobject<AutofillHeader> header_;
  base::scoped_nsobject<AutofillMainContainer> mainContainer_;
  base::scoped_nsobject<AutofillSignInContainer> signInContainer_;
  base::scoped_nsobject<AutofillOverlayController> overlayController_;
  base::scoped_nsobject<AutofillLoadingShieldController>
      loadingShieldController_;
  base::scoped_nsobject<NSTextView> fieldEditor_;

  // Signals the main container has recently become visible.
  BOOL mainContainerBecameVisible_;
}

// Designated initializer. The WebContents cannot be NULL.
- (id)initWithWebContents:(content::WebContents*)webContents
                   dialog:(autofill::AutofillDialogCocoa*)dialog;

// Requests a re-layout for the entire dialog. The layout will be postponed
// until the next cycle of the runloop.
- (void)requestRelayout;

// Validate data. If it is valid, notify the delegate that the user would
// like to use the data.
- (IBAction)accept:(id)sender;

// User canceled the dialog.
- (IBAction)cancel:(id)sender;

@end


#endif  // CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_DIALOG_WINDOW_CONTROLLER_H_
