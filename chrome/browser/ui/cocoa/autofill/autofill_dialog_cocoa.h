// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_DIALOG_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_DIALOG_COCOA_H_

#include "base/memory/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/autofill/autofill_dialog_view.h"
#include "chrome/browser/ui/cocoa/constrained_window/constrained_window_mac.h"

namespace content {
  class NavigationController;
}

namespace autofill {
  class AutofillDialogController;
}

@class AutofillDialogWindowController;
@class AutofillSignInContainer;
@class AutofillMainContainer;

namespace autofill {

class AutofillDialogCocoa : public AutofillDialogView,
                            public ConstrainedWindowMacDelegate {
 public:
  explicit AutofillDialogCocoa(AutofillDialogController* controller);
  virtual ~AutofillDialogCocoa();

  // AutofillDialogView implementation:
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void UpdateAccountChooser() OVERRIDE;
  virtual void UpdateButtonStrip() OVERRIDE;
  virtual void UpdateNotificationArea() OVERRIDE;
  virtual void UpdateSection(DialogSection section) OVERRIDE;
  virtual void FillSection(DialogSection section,
                           const DetailInput& originating_input) OVERRIDE;
  virtual void GetUserInput(DialogSection section,
                            DetailOutputMap* output) OVERRIDE;
  virtual string16 GetCvc() OVERRIDE;
  virtual bool SaveDetailsLocally() OVERRIDE;
  virtual const content::NavigationController* ShowSignIn() OVERRIDE;
  virtual void HideSignIn() OVERRIDE;
  virtual void UpdateProgressBar(double value) OVERRIDE;
  virtual void ModelChanged() OVERRIDE;

  // ConstrainedWindowMacDelegate implementation.
  virtual void OnConstrainedWindowClosed(
      ConstrainedWindowMac* window) OVERRIDE;

  AutofillDialogController* controller() { return controller_; }
  void PerformClose();

 private:

  scoped_ptr<ConstrainedWindowMac> constrained_window_;
  scoped_nsobject<AutofillDialogWindowController> sheet_controller_;

  // The controller |this| queries for logic and state.
  AutofillDialogController* controller_;
};

}  // autofill

@interface AutofillDialogWindowController : NSWindowController
                                            <NSWindowDelegate> {
 @private
  content::WebContents* webContents_;  // weak.
  autofill::AutofillDialogCocoa* autofillDialog_;  // weak.

  scoped_nsobject<AutofillMainContainer> mainContainer_;
  scoped_nsobject<AutofillSignInContainer> signInContainer_;
}

// Designated initializer. The WebContents cannot be NULL.
- (id)initWithWebContents:(content::WebContents*)webContents
      autofillDialog:(autofill::AutofillDialogCocoa*)autofillDialog;

// Closes the sheet and ends the modal loop. This will also clean up the memory.
- (IBAction)closeSheet:(id)sender;

// Forwarding AutofillDialogView calls.
- (void)updateAccountChooser;
- (content::NavigationController*)showSignIn;
- (void)hideSignIn;

@end

#endif  // CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_DIALOG_COCOA_H_
